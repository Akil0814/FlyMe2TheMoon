#include "application.h"
#include "../builtin/resources/builtin_resources.h"
#include "../builtin/audio/builtin_music_player.h"

#include "composition/application_scene_composition.h"
#include "lifecycle/application_event_boundary.h"
#include "lifecycle/frame_pacing.h"
#include "lifecycle/shutdown_boundary.h"
#include "lifecycle/application_exit_policy.h"
#include "lifecycle/application_termination_logging.h"
#include "presentation/application_sdl_presentation.h"
#include "presentation/application_window_settings.h"

#include "../builtin/resources/builtin_asset_catalog.h"
#include "../builtin/scenes/application_failure_scene_payload.h"
#include "../audio/audio_service.h"
#include "../bootstrap/bootstrapper.h"
#include "../core/time.h"
#include "../effects/runtime/effect_manager.h"
#include "../loading/content_runtime_cleanup.h"
#include "../localization/localization_manager.h"
#include "../localization/localization_service.h"
#include "../io/path/path_manager.h"
#include "../resources/resource_service.h"
#include "../save/save_service.h"
#include "../tools/logger.h"
#include "../tools/termination_manager.h"
#include "../ui/style/ui_theme_defaults.h"

#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <new>
#include <utility>

#include <SDL3_image/SDL_image.h>
#include "../core/render/sdl_render_boundary.h"
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

namespace elysia::application
{
Application::~Application()
{
    shutdown();
}

bool Application::check_startup_step(
    bool flag,
    std::string_view category,
    const char* err_msg,
    std::source_location location)
{
    if (flag)
        return true;

    const std::string error_message =
        err_msg ? err_msg : "Application runtime initialization failed.";
    return startup_fail(category,error_message,location);
}

bool Application::startup_fail(
    std::string_view category,
    const std::string& err_msg,
    std::source_location location)
{
    auto* logger = elysia::tools::Logger::instance();
    logger->error(category,err_msg,location);
    logger->terminating(
        "application",
        "Application terminating during startup after a fatal failure",
        location);
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_ERROR,
        "Game Start Error",
        err_msg.c_str(),
        _window);
    shutdown();
    return false;
}

bool Application::startup_fail(
    const elysia::bootstrap::BootstrapFailure& failure)
{
    std::filesystem::path project_root;
    if (failure.code != elysia::bootstrap::BootstrapFailure::Code::ProjectRoot)
        if (const auto* paths = elysia::io::PathManager::instance();
            paths && paths->is_initialized())
            project_root = paths->root();
    const std::string formatted = elysia::core::format_failure_diagnostic(
        failure.diagnostic,failure.error_code(),"bootstrap",project_root);
    auto* logger = elysia::tools::Logger::instance();
    logger->error("bootstrap",formatted,failure.diagnostic.origin);
    logger->terminating(
        "application",
        "Application terminating during startup after a fatal failure",
        failure.diagnostic.origin);

    std::string dialog = "The game could not start.\nError code: ";
    dialog += failure.error_code();
    if (failure.code == elysia::bootstrap::BootstrapFailure::Code::ProjectRoot)
        dialog += "\nRequired marker: assets/.elysia_root";
#if !defined(NDEBUG)
    dialog += "\n\n" + formatted;
#endif
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_ERROR,"Game Start Error",dialog.c_str(),_window);
    shutdown();
    return false;
}

bool Application::startup_fail(
    const elysia::localization::LocalizationFailure& failure)
{
    std::filesystem::path project_root;
    if (const auto* paths = elysia::io::PathManager::instance();
        paths && paths->is_initialized())
        project_root = paths->root();
    const std::string formatted = elysia::core::format_failure_diagnostic(
        failure.diagnostic,failure.error_code(),"localization",project_root);
    auto* logger = elysia::tools::Logger::instance();
    logger->error("localization",formatted,failure.diagnostic.origin);
    logger->terminating(
        "application",
        "Application terminating during startup after a fatal failure",
        failure.diagnostic.origin);
    std::string dialog = "The game could not start.\nError code: ";
    dialog += failure.error_code();
#if !defined(NDEBUG)
    dialog += "\n\n" + formatted;
#endif
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_ERROR,"Game Start Error",dialog.c_str(),_window);
    shutdown();
    return false;
}

bool Application::initialize(
    int argc,
    char** argv,
    const IGameModule& game_module)
{
    try
    {
        return initialize_impl(argc, argv, game_module);
    }
    catch (const std::bad_alloc&)
    {
        constexpr std::string_view message = "Out of memory during application startup.";
        elysia::tools::Logger::instance()->error("startup", message);
        elysia::tools::TerminationManager::instance()->request_termination(
            elysia::tools::TerminationReason::UnhandledException, "startup", message);
    }
    catch (const std::exception& error)
    {
        elysia::tools::Logger::instance()->error("startup", error.what());
        elysia::tools::TerminationManager::instance()->request_termination(
            elysia::tools::TerminationReason::UnhandledException, "startup", error.what());
    }
    catch (...)
    {
        constexpr std::string_view message = "Unknown exception during application startup.";
        elysia::tools::Logger::instance()->error("startup", message);
        elysia::tools::TerminationManager::instance()->request_termination(
            elysia::tools::TerminationReason::UnhandledException, "startup", message);
    }
    constexpr const char* fallback = "The game could not start. See the log for details.";
    elysia::tools::Logger::instance()->error("startup",fallback);
    if (!SDL_GetHintBoolean("ELYSIA_SUPPRESS_ERROR_DIALOGS",false))
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Game Start Error",fallback,_window);
    (void)shutdown();
    return false;
}

bool Application::initialize_impl(
    int argc,
    char** argv,
    const IGameModule& game_module)
{
    elysia::tools::Logger::instance()->initialize_console();

    _active = true;
    _normal_exit_requested = false;
    _has_shutdown = false;
    _shutdown_succeeded = true;

    elysia::tools::TerminationManager::instance()->initialize_lifecycle();

    ApplicationDescriptor descriptor;
    try
    {
        descriptor = describe_game_module(game_module);
    }
    catch (const std::bad_alloc&) { throw; }
    catch (const std::exception& error)
    {
        return startup_fail(
            "game_module",
            std::string("Game module descriptor failed: ") + error.what());
    }
    catch (...)
    {
        return startup_fail(
            "game_module",
            "Game module descriptor failed with an unknown exception.");
    }

    if (descriptor.logical_width <= 0 || descriptor.logical_height <= 0)
        return startup_fail("game_module","Game module logical viewport must be positive.");

    const auto resolved_font_settings =
        elysia::typography::resolve_font_settings(
            descriptor.presentation.fonts);
    if (!resolved_font_settings)
        return startup_fail("typography",resolved_font_settings.error());

    if (!elysia::ui::UiThemeDefaults::set_builtin_theme(
            descriptor.presentation.ui.default_theme))
    {
        return startup_fail(
            "ui",
            "Application default UI theme is invalid.");
    }

    const std::filesystem::path executable_path =
        argc > 0 && argv && argv[0]
            ? std::filesystem::path(argv[0])
            : std::filesystem::path{};
    auto parse_result =
        elysia::bootstrap::Bootstrapper::instance()->parse_runtime_settings(
            executable_path);

    if (!parse_result)
        return startup_fail(parse_result.error());

    elysia::bootstrap::BootstrapOutput bootstrap_output =
        std::move(*parse_result);
    _content_registry = std::move(bootstrap_output.content_registry);
    elysia::tools::Logger::instance()->initialize_file();

    if (const auto save_result = ELYSIA_SAVE->initialize(
            elysia::io::PathManager::instance()->saves());
        !save_result)
    {
        return startup_fail("save",elysia::core::format_failure_diagnostic(
            save_result.error().diagnostic,"SAVE-INITIALIZE","save"),save_result.error().diagnostic.origin);
    }

    if (bootstrap_output.warning)
        ELYSIA_LOG_WARN(
            "application",
            elysia::core::format_failure_diagnostic(
                bootstrap_output.warning->diagnostic,
                "BOOTSTRAP-USER-CONFIG","user-config",
                elysia::io::PathManager::instance()->root()));

    elysia::bootstrap::RuntimeSettings runtime_settings =
        std::move(bootstrap_output.runtime_settings);
    if (!initialize_runtime(runtime_settings,descriptor))
        return false;

#if ELYSIA_ENABLE_IMGUI
    try
    {
        _development_overlay_host.set_overlay(
            game_module.create_development_overlay());
    }
    catch (const std::bad_alloc&) { throw; }
    catch (const std::exception& error)
    {
        return startup_fail(
            "development_overlay",
            std::string("Development overlay creation failed: ")
                + error.what());
    }
    catch (...)
    {
        return startup_fail(
            "development_overlay",
            "Development overlay creation failed with an unknown exception.");
    }
    if (_development_overlay_host.configured())
    {
        try
        {
            auto overlay_result = _development_overlay_host.initialize(
                *_window, *_renderer);
            if (!overlay_result)
            {
                return startup_fail(
                    "development_overlay", overlay_result.error());
            }
        }
        catch (const std::bad_alloc&) { throw; }
        catch (const std::exception& error)
        {
            return startup_fail(
                "development_overlay",
                std::string("Development overlay initialization failed: ")
                    + error.what());
        }
        catch (...)
        {
            return startup_fail(
                "development_overlay",
                "Development overlay initialization failed with an unknown exception.");
        }
    }
#endif

    const elysia::builtin::BuiltinAssetCatalog builtin_asset_catalog(
        *elysia::io::PathManager::instance());
    if (const auto builtin_asset_result = elysia::builtin::BuiltinResources::instance()->initialize(
            _renderer,
            builtin_asset_catalog,
            resolved_font_settings->engine_point_sizes());
        !builtin_asset_result)
    {
        return startup_fail(
            "builtin",
            "Built-in asset initialization failed: " + builtin_asset_result.error());
    }
    if (!check_startup_step(
            elysia::builtin::BuiltinMusicPlayer::instance()->initialize(runtime_settings.user.audio),
            "builtin", "Built-in music player initialization failed"))
        return false;
    if (auto localization_result =
        elysia::localization::LocalizationManager::instance()->initialize(
        _renderer,
        bootstrap_output.i18n_manifest_path,
        runtime_settings.user.language,
        &_font_resolver);
        !localization_result)
    {
        return startup_fail(localization_result.error());
    }

    if (const auto font_result = _font_resolver.configure(
            *resolved_font_settings,
            *elysia::resources::ResourceService::instance(),
            ELYSIA_LOCALIZATION->supported_languages());
        !font_result)
    {
        return startup_fail("typography",font_result.error().message);
    }
    elysia::effects::EffectManager::instance()->set_runtime_dependencies(
        _renderer,
        &_font_resolver);

    elysia::config::UserConfigService::instance()->register_user_config_change_handler(*this);
    _user_config_handler_registered = true;

    elysia::config::UserConfig& user_config =
        elysia::config::UserConfigService::instance()->user_config();
    if (user_config.language()
        != ELYSIA_LOCALIZATION->current_language())
    {
        const auto language_result = user_config.set_language(
            ELYSIA_LOCALIZATION->current_language());
        if (!language_result)
        {
            ELYSIA_LOG_WARN("application",
                "Localization warning: normalize language in config failed: "
                << elysia::core::format_failure_diagnostic(language_result.error().diagnostic,"CONFIG-APPLY","config"));
        }
        else if (const auto save_result =
            elysia::config::UserConfigService::instance()->save_user_config();
            !save_result)
        {
            ELYSIA_LOG_WARN("application",
                "Localization warning: save normalized language failed: "
                << elysia::core::format_failure_diagnostic(save_result.error().diagnostic,"CONFIG-SAVE","config"));
        }
    }

    _input_system.initialize();
    _input_system.set_renderer(_renderer);

    if (const auto preload_result =
            elysia::bootstrap::Bootstrapper::instance()
                ->preload_startup_resources(_renderer);
        !preload_result)
        return startup_fail(preload_result.error());

    elysia::tools::IDevelopmentPanelRegistry* development_panels = nullptr;
#if ELYSIA_ENABLE_IMGUI
    development_panels = _development_overlay_host.panel_registry();
#endif
    _scene_runtime_context.emplace(
        _renderer,
        _content_registry,
        descriptor.logical_width,
        descriptor.logical_height,
        &_font_resolver,
        development_panels);
    _scene_manager.initialize(
        *_scene_runtime_context,
        [](const elysia::scene::SceneBoundaryFailure& failure) {
            auto route = elysia::builtin::make_application_failure_route(failure);
            route.reload_mode = elysia::scene::SceneReloadMode::Recreate;
            return route;
        });

    return enter_initial_scene(game_module,descriptor);
}

bool Application::initialize_runtime(
    const elysia::bootstrap::RuntimeSettings& settings,
    const ApplicationDescriptor& descriptor)
{
    const elysia::config::UserConfigData& user_settings = settings.user;
    const bool sdl_initialized = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD);
    _sdl_initialized = sdl_initialized;
    if (!check_startup_step(sdl_initialized,"platform","SDL3 Error"))
        return false;

    if (const auto presentation_result =
            detail::configure_sdl_render_hints(descriptor.presentation.render);
        !presentation_result)
    {
        return startup_fail("platform",presentation_result.error());
    }

    _mixer_initialized = MIX_Init();
    if (!check_startup_step(_mixer_initialized,"audio","SDL_mixer Error"))
        return false;

    const bool ttf_initialized = TTF_Init();
    _ttf_initialized = ttf_initialized;
    if (!check_startup_step(ttf_initialized,"platform","SDL_ttf Error"))
        return false;

    if (!check_startup_step(
        elysia::audio::AudioService::instance()->initialize(user_settings.audio),
        "audio",
        "AudioService initialization failed"))
    {
        return false;
    }



    _window = SDL_CreateWindow(settings.window_title.c_str(), user_settings.window.windowed_size.width, user_settings.window.windowed_size.height, 0);
    if (!check_startup_step(_window != nullptr,"platform","SDL_CreateWindow Error"))
        return false;

    const auto startup_window = detail::apply_startup_window_settings(
        user_settings.window,detail::make_sdl_window_operations(_window));
    if (!startup_window)
        return startup_fail("platform",elysia::core::format_failure_diagnostic(
            startup_window.error(),"STARTUP-WINDOW","platform"),startup_window.error().origin);
    if (*startup_window)
        elysia::tools::Logger::instance()->warn("application",elysia::core::format_failure_diagnostic(
            **startup_window,"STARTUP-WINDOW","platform"),(**startup_window).origin);

    _renderer = SDL_CreateGPURenderer(nullptr,_window);
    if (!check_startup_step(_renderer != nullptr,"platform","SDL GPU renderer creation failed"))
        return false;
    ELYSIA_LOG("application","SDL3 GPU backend: " << SDL_GetGPUDeviceDriver(SDL_GetGPURendererDevice(_renderer)));
    if (!check_startup_step(SDL_SetRenderVSync(_renderer,user_settings.vsync ? 1 : 0),"platform","SDL VSync configuration failed"))
        return false;
    if (auto filter_result = detail::configure_sdl_texture_filter(_renderer,descriptor.presentation.render); !filter_result)
        return startup_fail("platform",filter_result.error());

    if (const auto presentation_result =
            detail::configure_sdl_renderer_presentation(
            _renderer,
            descriptor.logical_width,
            descriptor.logical_height);
        !presentation_result)
    {
        return startup_fail("platform",presentation_result.error());
    }

    _target_fps = user_settings.target_fps;
    return true;
}

bool Application::enter_initial_scene(
    const IGameModule& game_module,
    const ApplicationDescriptor& descriptor)
{
    _scene_manager.attach(this);

    try
    {
        compose_application_scenes(
            _scene_manager,
            game_module,
            descriptor);
    }
    catch (const elysia::core::RenderBackendError&)
    {
        const auto failure = std::current_exception();
        (void)run_event_boundary("scene",[&] { std::rethrow_exception(failure); });
        log_published_termination(elysia::tools::TerminationManager::instance()->termination_info());
        (void)shutdown();
        return false;
    }
    catch (const std::bad_alloc&) { throw; }
    catch (const std::exception& error)
    {
        return startup_fail(
            "scene",
            std::string("Scene composition failed: ") + error.what());
    }
    catch (...)
    {
        return startup_fail(
            "scene",
            "Scene composition failed with an unknown exception.");
    }

    return true;
}

ApplicationRunResult Application::run()
{
    std::uint64_t last_frame_start = SDL_GetPerformanceCounter();
    const std::uint64_t counter_freq = SDL_GetPerformanceFrequency();
    elysia::core::Time::instance()->reset();

    ApplicationRunResult run_result = ApplicationRunResult::NormalExit;
    auto resolve_exit = [this,&run_result]()
    {
        auto* termination_manager = elysia::tools::TerminationManager::instance();
        const ApplicationExitDecision decision =
            resolve_application_exit(_normal_exit_requested,*termination_manager);
        if (decision == ApplicationExitDecision::Continue)
            return false;

        _active = false;
        run_result = to_application_run_result(decision);
        log_fault_exit_if_needed(decision,termination_manager->termination_info());
        return true;
    };
    auto stop_after_boundary_failure = [this,&run_result,&resolve_exit]()
    {
        if (resolve_exit())
            return;

        _active = false;
        run_result = ApplicationRunResult::FaultExit;
        log_published_termination(std::nullopt);
    };

    while (_active)
    {
        const std::uint64_t frame_start = SDL_GetPerformanceCounter();
        const double delta =
            static_cast<double>(frame_start - last_frame_start) / counter_freq;
        last_frame_start = frame_start;
        elysia::core::Time::instance()->begin_frame(delta);

        if (!run_event_boundary("events",[this]()
        {
#if ELYSIA_ENABLE_IMGUI
            _input_system.set_development_input_capture(
                _development_overlay_host.captured_input());
#endif
            _input_system.begin_frame();
            while (SDL_PollEvent(&_event))
            {
                bool development_event_consumed = false;
#if ELYSIA_ENABLE_IMGUI
                development_event_consumed =
                    _development_overlay_host.process_event(_event);
#endif
                if (!development_event_consumed)
                    _input_system.process_event(_event);
                if (_event.type == SDL_EVENT_QUIT)
                    _normal_exit_requested = true;
            }

        }))
        {
            stop_after_boundary_failure();
            break;
        }
        if (resolve_exit())
            break;

        if (!run_event_boundary("input", [this]() { _scene_manager.on_input(_input_system.snapshot()); }))
        {
            stop_after_boundary_failure();
            break;
        }
        if (resolve_exit())
            break;

        if (!run_event_boundary("update",[this]()
        {
            const double frame_delta = elysia::core::Time::instance()->delta();
#if ELYSIA_ENABLE_IMGUI
            _development_overlay_host.begin_frame(frame_delta);
#endif
            _scene_manager.on_update(frame_delta);
            elysia::audio::AudioService::instance()->update(frame_delta);
        }))
        {
            stop_after_boundary_failure();
            break;
        }
        if (resolve_exit())
            break;

        if (!run_event_boundary("render_begin",[this] {
            elysia::core::require_render_success(elysia::core::begin_render_frame(_renderer));
        }))
        {
            stop_after_boundary_failure();
            break;
        }

        if (!run_event_boundary("render",[this]()
        {
            _scene_manager.on_render(_renderer);
#if ELYSIA_ENABLE_IMGUI
            _development_overlay_host.render(*_renderer);
#endif
        }))
        {
            stop_after_boundary_failure();
            break;
        }
        if (resolve_exit())
            break;

        if (!run_event_boundary("render_present",[this] {
            elysia::core::require_render_success(elysia::core::present_render_frame(_renderer));
        }))
        {
            stop_after_boundary_failure();
            break;
        }
        if (resolve_exit())
            break;

        detail::wait_for_frame(_target_fps,
            [&] { return static_cast<double>(SDL_GetPerformanceCounter() - frame_start) / counter_freq; },
            [this]
            {
                SDL_PumpEvents();
                return _normal_exit_requested || SDL_HasEvent(SDL_EVENT_QUIT)
                    || elysia::tools::TerminationManager::instance()->termination_requested();
            },
            [](std::uint64_t ns,bool precise)
            {
                if (precise) SDL_DelayPrecise(ns);
                else SDL_DelayNS(ns);
            });
    }

    if (!shutdown())
        run_result = ApplicationRunResult::FaultExit;
    return run_result;
}

bool Application::shutdown() noexcept
{
    if (_has_shutdown)
        return _shutdown_succeeded;

    _has_shutdown = true;
    _active = false;

    auto cleanup = [this](const char* phase,auto&& action)
    {
        if (!run_shutdown_boundary(phase,action))
            _shutdown_succeeded = false;
    };
    cleanup("application_shutdown",[&] { _input_system.shutdown(); });
    cleanup("application_shutdown",[&] { _input_system.set_renderer(nullptr); });
    cleanup("application_shutdown",[&] { _scene_manager.detach(this); });
    if (!_scene_manager.shutdown())
        _shutdown_succeeded = false;
#if ELYSIA_ENABLE_IMGUI
    cleanup("application_shutdown",[&] { _development_overlay_host.shutdown(); });
#endif
    cleanup("application_shutdown",[&] { _scene_runtime_context.reset(); });
    cleanup("application_shutdown",[&] { ELYSIA_SAVE->shutdown(); });

    cleanup("application_shutdown",[&] { elysia::localization::LocalizationManager::instance()->shutdown(); });
    cleanup("application_shutdown",[&] { elysia::bootstrap::Bootstrapper::instance()->release_preload_textures(); });
    cleanup("application_shutdown",[&] { _font_resolver.deactivate_project_fonts(); });
    cleanup("application_shutdown",[&] { elysia::effects::EffectManager::instance()->set_runtime_dependencies(nullptr,nullptr); });
    cleanup("application_shutdown",[&] { elysia::audio::AudioService::instance()->shutdown(); });
    cleanup("application_shutdown",[&] { elysia::loading::clear_loaded_content(); });
    cleanup("application_shutdown",[&] { _font_resolver.shutdown(); });
    cleanup("application_shutdown",[&] { elysia::builtin::BuiltinMusicPlayer::instance()->shutdown(); });
    cleanup("application_shutdown",[&] { elysia::builtin::BuiltinResources::instance()->shutdown(); });
    if (_user_config_handler_registered)
    {
        cleanup("config_shutdown",[&] { elysia::config::UserConfigService::instance()->unregister_user_config_change_handler(*this); });
        _user_config_handler_registered = false;
    }
    cleanup("application_shutdown",[&] { elysia::config::UserConfigService::instance()->shutdown(); });

    SDL_DestroyRenderer(_renderer);
    _renderer = nullptr;
    SDL_DestroyWindow(_window);
    _window = nullptr;

    if (_ttf_initialized)
    {
        TTF_Quit();
        _ttf_initialized = false;
    }
    if (_mixer_initialized)
    {
        cleanup("application_shutdown",[&] { elysia::audio::detail::mixer_backend().shutdown(); });
        MIX_Quit();
        _mixer_initialized = false;
    }
    if (_sdl_initialized)
    {
        SDL_Quit();
        _sdl_initialized = false;
    }

    ELYSIA_LOG("application","Application shutdown complete");
    elysia::tools::Logger::instance()->shutdown();
    return _shutdown_succeeded;
}

void Application::on_scene_manager_quit_requested()
{
    _normal_exit_requested = true;
}

void Application::on_scene_manager_fault(
    const elysia::scene::SceneBoundaryFailure& failure)
{
    try
    {
        elysia::tools::TerminationManager::instance()->request_termination(
            elysia::tools::TerminationReason::FatalRuntimeFailure,"scene",
            elysia::core::format_failure_diagnostic(elysia::scene::to_failure_diagnostic(failure),"APPLICATION-FATAL","scene"),
            failure.diagnostic.origin);
    }
    catch (...)
    {
        elysia::tools::Logger::instance()->error(
            "scene",failure.diagnostic.message,failure.diagnostic.origin);
        elysia::tools::TerminationManager::instance()->request_termination(
            elysia::tools::TerminationReason::FatalRuntimeFailure,"scene",
            failure.diagnostic.message,failure.diagnostic.origin);
    }
}

namespace
{
std::unexpected<elysia::config::UserConfigFailure> runtime_apply_failure(
    const char* setting,
    const std::string& message,
    std::source_location origin = std::source_location::current())
{
    return std::unexpected(elysia::config::make_user_config_failure(
        elysia::config::UserConfigError::RuntimeApplyFailed,
        setting,
        elysia::core::make_failure_diagnostic(message,{},{},origin)
    ));
}
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_master_volume(int value)
{
    elysia::audio::AudioService::instance()->set_master_volume(value);
    elysia::builtin::BuiltinMusicPlayer::instance()->set_master_volume(value);
    return {};
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_music_volume(int value)
{
    elysia::audio::AudioService::instance()->set_music_volume(value);
    elysia::builtin::BuiltinMusicPlayer::instance()->set_music_volume(value);
    return {};
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_sound_volume(int value)
{
    elysia::audio::AudioService::instance()->set_sound_volume(value);
    return {};
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_language(std::string_view language)
{
    if (auto result = ELYSIA_LOCALIZATION->set_language(std::string(language));
        !result)
    {
        return std::unexpected(elysia::config::make_user_config_failure(
            elysia::config::UserConfigError::RuntimeApplyFailed,"language",
            result.error().diagnostic));
    }

    return {};
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_target_fps(double value)
{
    if (!std::isfinite(value) || value <= 0.0)
    {
        return runtime_apply_failure(
            "target_fps",
            "Target FPS must be finite and positive.");
    }
    _target_fps = value;
    return {};
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_window_settings(
    const elysia::config::WindowSettings& settings)
{
    if (!_window)
        return runtime_apply_failure("window_settings","Application window is unavailable.");

    const auto operations = detail::make_sdl_window_operations(_window);
    if (auto valid = detail::validate_window_settings(settings,operations); !valid)
        return std::unexpected(elysia::config::make_user_config_failure(
            elysia::config::UserConfigError::RuntimeApplyFailed,"window_settings",valid.error()));
    auto previous = detail::capture_window_snapshot(
        elysia::config::UserConfigService::instance()->user_config().window_settings(),operations);
    if (!previous)
        return std::unexpected(elysia::config::make_user_config_failure(
            elysia::config::UserConfigError::RuntimeApplyFailed,"window_settings",previous.error()));
    const auto result = detail::apply_window_settings_transactional(settings,*previous,operations);
    if (!result)
    {
        return std::unexpected(elysia::config::make_user_config_failure(
            elysia::config::UserConfigError::RuntimeApplyFailed,"window_settings",
            result.error()));
    }
    return {};
}
}
