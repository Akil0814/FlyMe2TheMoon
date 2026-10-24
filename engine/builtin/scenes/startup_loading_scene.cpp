#include "startup_loading_scene.h"
#include "application_failure_scene_payload.h"

#include "../resources/builtin_resources.h"
#include "../../bootstrap/bootstrapper.h"
#include "../../io/path/path_manager.h"
#include "../../tools/logger.h"
#include "../../typography/font_resolver.h"
#include "../../ui/widgets/image/ui_fade_image.h"
#include "../../ui/widgets/label/ui_blink_label.h"
#include "../../ui/widgets/ui_bar.h"
#include "../../scene/runtime/scene_runtime_context.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace elysia::builtin
{
using elysia::scene::Scene;
using elysia::scene::ScenePayload;
using elysia::scene::SceneRoute;
using elysia::scene::try_scene_payload;

namespace
{
constexpr double kEngineLogoFadeInSeconds = 1.0;
constexpr double kEngineLogoHoldSeconds = 1.0;
constexpr double kEngineLogoFadeOutSeconds = 1.0;

bool is_valid_route(const SceneRoute& route) noexcept
{
    return elysia::scene::SceneKeys::is_supported(route.target);
}
}

elysia::ui::UiTextContent
StartupLoadingScene::make_start_prompt_content()
{
    return elysia::ui::ui_text_key(
        "engine.startup_loading.press_any_button");
}

void StartupLoadingScene::on_enter(const ScenePayload& payload)
{
    clear_state();

    const StartupLoadingScenePayload* startup_payload =
        try_scene_payload<StartupLoadingScenePayload>(payload);
    if (!startup_payload)
    {
        throw std::logic_error(
            "StartupLoadingScene requires StartupLoadingScenePayload.");
    }
    if (!is_valid_route(startup_payload->success_route))
    {
        throw std::logic_error(
            "StartupLoadingScene requires a valid success route.");
    }
    if (startup_payload->failure_route
        && !is_valid_route(*startup_payload->failure_route))
    {
        throw std::logic_error(
            "StartupLoadingScene failure route is invalid.");
    }

    _startup_payload = *startup_payload;
    _completion.reset(
        _startup_payload.wait_for_confirmation,
        _startup_payload.wait_for_logo_sequence);
    _paused = false;

    elysia::typography::FontResolver* font_resolver =
        runtime_context().font_resolver();
    if (!font_resolver)
    {
        handle_failure("StartupLoadingScene requires FontResolver.");
        return;
    }
    font_resolver->deactivate_project_fonts();

    if (!create_presentation())
        return;

    begin_logo_sequence();
    const auto start_result = _content_loader.start(
        runtime_context().renderer(),
        runtime_context().content_registry(),
        font_resolver->project_point_sizes()
    );
    if (!start_result)
    {
        handle_failure(start_result.error());
    }
}

void StartupLoadingScene::on_exit()
{
    _paused = false;
    _content_loader.reset();
    destroy_ui();
    elysia::bootstrap::Bootstrapper::instance()->release_preload_textures();
    clear_state();
}

void StartupLoadingScene::on_reset()
{
    _paused = false;
    _content_loader.reset();
    destroy_ui();
    clear_state();
}

void StartupLoadingScene::on_after_update(double delta)
{
    if (_completion.transitioning())
        return;

    _content_loader.update();

    if (_loading_bar)
        _loading_bar->set_ratio(_content_loader.progress());

    if (_content_loader.has_failed())
    {
        if (const auto* failure = _content_loader.failure())
            handle_failure(*failure);
        return;
    }

    if (!_completion.loading_finished() && _content_loader.is_finished())
        mark_loading_finished();
}

void StartupLoadingScene::on_shortcuts(const elysia::input::RawInputFrame &input,
                                       const std::vector<elysia::input::RawInputEvent> &events)
{

    if (!_completion.waiting_for_confirmation()
        || _completion.transitioning())
        return;

    for (const elysia::input::RawInputEvent& event : events)
    {
        if (event.type == elysia::input::RawInputEventType::ControlPressed
            && elysia::input::matches_control(
                elysia::input::RawInputControl::AnyControl,
                event.control))
        {
            consume_input(event);
            handle_completion_action(_completion.confirm());
            return;
        }
    }
}

bool StartupLoadingScene::create_presentation()
{
    SDL_Texture* engine_texture = elysia::builtin::BuiltinResources::instance()->find_texture(BuiltinTextureId::ElysiaWhite);
    if (!engine_texture)
    {
        handle_failure("Required Elysia startup logo is not available.");
        return false;
    }

    const float logical_width =
        static_cast<float>(runtime_context().logical_width());
    const float logical_height =
        static_cast<float>(runtime_context().logical_height());
    const float logo_size = std::max(
        1.0f,
        std::min({ 200.0f,logical_width * 0.5f,logical_height * 0.5f })
    );
    const elysia::core::Vector2 logo_center{
        logical_width * 0.5f,
        logical_height * 0.5f
    };

    _engine_logo = create_and_add_object<elysia::ui::UiFadeImage>(
        engine_texture,
        logo_center,
        elysia::core::Vector2{ logo_size,logo_size },
        elysia::ui::from_center
    );
    if (!_engine_logo)
    {
        handle_failure("StartupLoadingScene could not create the Elysia logo widget.");
        return false;
    }

    _engine_logo->configure_playback(
        elysia::ui::effects::UiOpacityFadeMode::FadeInOut,
        kEngineLogoHoldSeconds,
        kEngineLogoFadeInSeconds,
        kEngineLogoFadeOutSeconds
    );
    _engine_logo->set_on_end([this] { on_engine_logo_finished(); });

    if (_startup_payload.project_logo)
    {
        const StartupLogoSlot& slot = *_startup_payload.project_logo;
        if (slot.texture_key.empty())
        {
            ELYSIA_LOG_WARN("startup",
                "Project startup logo slot has an empty texture key and will be skipped.");
        }
        else
        {
            SDL_Texture* project_texture =
                elysia::bootstrap::Bootstrapper::instance()->find_preload_texture(
                    slot.texture_key);
            if (!project_texture)
            {
                ELYSIA_LOG_WARN("startup",
                    "Optional project startup logo is unavailable and will be skipped: "
                    << slot.texture_key);
            }
            else
            {
                _project_logo = create_and_add_object<elysia::ui::UiFadeImage>(
                    project_texture,
                    logo_center,
                    elysia::core::Vector2{ logo_size,logo_size },
                    elysia::ui::from_center
                );
                if (_project_logo)
                {
                    _project_logo->configure_playback(
                        elysia::ui::effects::UiOpacityFadeMode::FadeInOut,
                        slot.hold_seconds,
                        slot.fade_in_seconds,
                        slot.fade_out_seconds
                    );
                    _project_logo->set_visible(false);
                    _project_logo->set_on_end(
                        [this] { on_project_logo_finished(); });
                }
                else
                {
                    ELYSIA_LOG_WARN("startup",
                        "Optional project startup logo widget could not be created and will be skipped.");
                }
            }
        }
    }

    create_loading_ui();
    return !_completion.transitioning();
}

void StartupLoadingScene::create_loading_ui()
{
    const float logical_width =
        static_cast<float>(runtime_context().logical_width());
    const float logical_height =
        static_cast<float>(runtime_context().logical_height());
    const float margin = std::max(8.0f,std::min(20.0f,logical_width * 0.025f));
    const float bar_width = std::max(1.0f,logical_width - margin * 2.0f);
    const float bar_y = std::max(0.0f,logical_height - margin - 5.0f);

    _loading_bar = create_and_add_object<elysia::ui::UiBar>(
        elysia::core::Rect{ margin,bar_y,bar_width,5.0f });
    if (_loading_bar)
    {
        elysia::ui::UiBarStyleOverrides style{};
        style.draw_border = true;
        _loading_bar->set_style_overrides(style);
        _loading_bar->set_ratio(0.0f);
    }

    const float prompt_height = std::max(
        1.0f,
        std::min(40.0f,logical_height * 0.1f)
    );
    const float prompt_width = std::max(
        1.0f,
        std::min(400.0f,logical_width - margin * 2.0f)
    );
    const elysia::core::Rect prompt_rect{
        (logical_width - prompt_width) * 0.5f,
        std::max(0.0f,bar_y - prompt_height - 8.0f),
        prompt_width,
        prompt_height
    };
    _start_prompt = create_and_add_object<elysia::ui::UiBlinkLabel>(
        prompt_rect,0,make_start_prompt_content()
    );
    if (_start_prompt)
    {
        _start_prompt->configure_playback(elysia::ui::effects::UiOpacityBlinkMode::HiddenFirst, 0.0, 0.6, 0.6);
        _start_prompt->set_typography_role(
            elysia::typography::UiTypographyRole::Button);
        _start_prompt->set_horizontal_align(
            elysia::ui::TextHorizontalAlign::Center);
        _start_prompt->set_vertical_align(
            elysia::ui::TextVerticalAlign::Center);
        _start_prompt->set_visible(false);
    }
}

void StartupLoadingScene::begin_logo_sequence()
{
    _logo_sequence.reset(_project_logo != nullptr);
    handle_logo_action(_logo_sequence.start());
}

void StartupLoadingScene::on_engine_logo_finished()
{
    _engine_logo = nullptr;
    handle_logo_action(_logo_sequence.engine_logo_finished());
}

void StartupLoadingScene::on_project_logo_finished()
{
    _project_logo = nullptr;
    handle_logo_action(_logo_sequence.project_logo_finished());
}

void StartupLoadingScene::mark_intro_finished()
{
    handle_completion_action(_completion.mark_intro_finished());
}

void StartupLoadingScene::mark_loading_finished()
{
    elysia::typography::FontResolver* font_resolver =
        runtime_context().font_resolver();
    if (!activate_project_fonts(font_resolver))
        return;

    if (_loading_bar)
    {
        _loading_bar->destroy();
        _loading_bar = nullptr;
    }
    handle_completion_action(_completion.mark_loading_finished());
}

bool StartupLoadingScene::activate_project_fonts(
    elysia::typography::FontResolver* font_resolver)
{
    if (!font_resolver)
    {
        handle_failure("Startup font activation failed: FontResolver is unavailable.");
        return false;
    }

    if (const auto activation = font_resolver->activate_project_fonts();
        !activation)
    {
        handle_failure(
            "Startup font activation failed: " + activation.error().message);
        return false;
    }

    return true;
}

void StartupLoadingScene::handle_logo_action(StartupLogoAction action)
{
    switch (action)
    {
    case StartupLogoAction::PlayEngineLogo:
        if (_engine_logo)
            _engine_logo->play();
        break;
    case StartupLogoAction::PlayProjectLogo:
        if (_project_logo)
        {
            _project_logo->set_visible(true);
            _project_logo->play();
        }
        break;
    case StartupLogoAction::IntroFinished:
        mark_intro_finished();
        break;
    case StartupLogoAction::None:
    default:
        break;
    }
}

void StartupLoadingScene::handle_completion_action(
    StartupLoadingAction action)
{
    switch (action)
    {
    case StartupLoadingAction::WaitForConfirmation:
        if (_start_prompt)
        {
            _start_prompt->set_visible(true);
            _start_prompt->play();
        }
        break;
    case StartupLoadingAction::TransitionToSuccess:
        transition_to_success();
        break;
    case StartupLoadingAction::TransitionToFailure:
    case StartupLoadingAction::None:
    default:
        break;
    }
}

void StartupLoadingScene::transition_to_success()
{
    request_scene_switch(_startup_payload.success_route);
}

void StartupLoadingScene::handle_failure(
    std::string_view message,std::source_location origin)
{
    if (_completion.fail()
        != StartupLoadingAction::TransitionToFailure)
        return;

    elysia::tools::Logger::instance()->error("startup",message,origin);
    if (_startup_payload.failure_route)
    {
        request_scene_switch(*_startup_payload.failure_route);
        return;
    }
    request_scene_switch(elysia::scene::SceneRoute{
        .target = SceneKeys::ApplicationFailure,
        .payload = ApplicationFailureScenePayload{
            .presentation = ApplicationFailurePresentation::StartupLoading,
            .reason = ApplicationFailureReason::Startup,
            .error_code = "STARTUP-PRESENTATION",
            .category = "startup",
            .diagnostic = elysia::core::make_failure_diagnostic(
                std::string(message),{},{},origin)
        },
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    });
}

void StartupLoadingScene::handle_failure(
    const elysia::loading::ContentLoadFailure& failure)
{
    if (_completion.fail()
        != StartupLoadingAction::TransitionToFailure)
        return;

    std::filesystem::path project_root;
    if (const auto* paths = elysia::io::PathManager::instance();
        paths && paths->is_initialized())
        project_root = paths->root();
    const std::string formatted = elysia::core::format_failure_diagnostic(
        failure.diagnostic,failure.error_code(),"startup",project_root);
    elysia::tools::Logger::instance()->error(
        "startup",formatted,failure.diagnostic.origin);

    if (_startup_payload.failure_route)
    {
        request_scene_switch(*_startup_payload.failure_route);
        return;
    }

    request_scene_switch(make_application_failure_route(failure,"startup"));
}

void StartupLoadingScene::destroy_ui()
{
    const bool had_ui =
        _loading_bar || _engine_logo || _project_logo || _start_prompt;

    if (_loading_bar)
        _loading_bar->destroy();
    if (_engine_logo)
        _engine_logo->destroy();
    if (_project_logo)
        _project_logo->destroy();
    if (_start_prompt)
        _start_prompt->destroy();

    _loading_bar = nullptr;
    _engine_logo = nullptr;
    _project_logo = nullptr;
    _start_prompt = nullptr;

    // Purge immediately so a cached scene cannot accumulate retired widgets.
    if (had_ui)
        clear_scene_objects();
}

void StartupLoadingScene::clear_state() noexcept
{
    _startup_payload = StartupLoadingScenePayload{};
    _logo_sequence.reset(false);
    _completion.reset(false,true);
}
}
