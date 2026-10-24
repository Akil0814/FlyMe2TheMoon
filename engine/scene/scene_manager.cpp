#include "scene_manager.h"
#include "detail/scene_failure_log.h"
#include "detail/scene_failure_boundary.h"

#include "../camera/camera_manager.h"
#include "../effects/runtime/effect_manager.h"
#include "../gameplay/control/controller_manager.h"
#include "../object_query/runtime/game_object_query_manager.h"
#include "../tools/debug_draw.h"
#include "../tools/logger.h"
#include "../core/render/render_failure.h"
#include "../core/time.h"

#include <exception>
#include <stdexcept>
#include <utility>

namespace elysia::scene
{
SceneManager::~SceneManager()
{
    (void)shutdown();
}

void SceneManager::initialize(
    const SceneRuntimeContext& context,
    SceneFailureRouteFactory failure_route_factory)
{
    if (_state != SceneManagerState::Constructed && _state != SceneManagerState::Stopped)
        throw std::logic_error("SceneManager::initialize requires a constructed or stopped manager.");

    if (_state == SceneManagerState::Stopped)
        _local_players.reset_defaults();
    elysia::gameplay::ControllerManager::instance()->initialize();
    // Shutdown clears screen effects even when no scene has been entered. Construct
    // the manager here so that shutdown never performs its first allocation.
    (void)elysia::effects::EffectManager::instance();
    _runtime_context = &context;
    _failure_route_factory = std::move(failure_route_factory);
    _shutdown_succeeded = true;
    _shutdown_performed = false;
    _recovering_failure = false;
    _pending_request = {};
    _has_pending_request = false;
    _is_processing_request = false;

    const elysia::core::Vector2 viewport_size{
        static_cast<float>(context.logical_width()),
        static_cast<float>(context.logical_height())};
    auto* cameras = elysia::camera::CameraManager::instance();
    for (std::size_t index = 0;
         index < static_cast<std::size_t>(elysia::camera::CameraSlot::Count);
         ++index)
    {
        cameras->set_viewport_size(
            static_cast<elysia::camera::CameraSlot>(index), viewport_size);
    }
    _state = SceneManagerState::Ready;
}

void SceneManager::ensure_ready_for_registration() const
{
    if (_state != SceneManagerState::Ready)
        throw std::logic_error("Scene registration is only allowed while SceneManager is ready.");
}

void SceneManager::start(const SceneRoute& route)
{
    if (_state != SceneManagerState::Ready)
        throw std::logic_error("SceneManager::start requires the ready state.");
    if (_current_scene)
        throw std::logic_error("SceneManager::start called while a scene is already active.");

    _state = SceneManagerState::Running;
    try
    {
        if (auto result = switch_to_registered_scene(route); !result)
            recover_from_failure(result.error());
    }
    catch (...)
    {
        _state = SceneManagerState::Ready;
        throw;
    }
}

void SceneManager::on_input(const elysia::input::InputSnapshot& input)
{
    if (_state != SceneManagerState::Running || !_current_scene)
        return;
    try
    {
        _current_scene->lifecycle_input(input);
    }
    catch (const elysia::core::RenderBackendError&) { throw; }
    catch (...)
    {
        recover_from_failure(make_failure(_current_scene_key, SceneBoundary::Input));
        return;
    }
    process_pending_request();
}

void SceneManager::on_update(double delta)
{
    if (_state != SceneManagerState::Running || !_current_scene)
        return;
    try
    {
        elysia::effects::EffectManager::instance()->_screen_effects.update(
            elysia::core::Time::instance()->raw_delta(), delta, _current_scene->is_paused(),
            elysia::core::Time::instance()->frame_count());
        _current_scene->lifecycle_update(delta);
    }
    catch (const elysia::core::RenderBackendError&) { throw; }
    catch (...)
    {
        recover_from_failure(make_failure(_current_scene_key, SceneBoundary::Update));
        return;
    }
    process_pending_request();
}

void SceneManager::on_render(SDL_Renderer* renderer)
{
    if (_state != SceneManagerState::Running || !_current_scene)
        return;
    try
    {
        _current_scene->lifecycle_render(renderer);
    }
    catch (const elysia::core::RenderBackendError&) { throw; }
    catch (...)
    {
        recover_from_failure(make_failure(_current_scene_key, SceneBoundary::Render));
    }
}

void SceneManager::on_scene_request(const SceneRequest& request)
{
    if (_state != SceneManagerState::Running)
        throw std::logic_error("Scene requests require a running SceneManager.");
    if (_is_processing_request || _recovering_failure)
        throw std::logic_error("SceneManager received a reentrant scene request.");
    if (_has_pending_request)
        throw std::logic_error("SceneManager received multiple scene requests in one processing cycle.");
    _pending_request = request;
    _has_pending_request = true;
}

void SceneManager::notify_quit_requested()
{
    notify_observers([](SceneManagerObserver& observer) {
        observer.on_scene_manager_quit_requested();
    });
}

void SceneManager::notify_fault(const SceneBoundaryFailure& failure) noexcept
{
    _state = SceneManagerState::Faulted;
    try
    {
        notify_observers_isolated([&](SceneManagerObserver& observer) {
            observer.on_scene_manager_fault(failure);
        },[] { detail::log_cleanup_exception("Fault observer notification"); });
    }
    catch (...)
    {
        detail::log_cleanup_exception("Fault observer snapshot");
    }
}

void SceneManager::process_pending_request()
{
    if (!_has_pending_request || _state != SceneManagerState::Running)
        return;

    const SceneRequest request = _pending_request;
    _pending_request = {};
    _has_pending_request = false;
    _is_processing_request = true;
    struct Guard
    {
        bool& flag;
        ~Guard() { flag = false; }
    } guard{_is_processing_request};

    switch (request.type)
    {
    case SceneRequestType::Switch:
        if (auto result = switch_to_registered_scene(request.route); !result)
            recover_from_failure(result.error());
        break;
    case SceneRequestType::Quit:
        notify_quit_requested();
        break;
    case SceneRequestType::None:
    default:
        break;
    }
}

std::expected<void, SceneBoundaryFailure>
SceneManager::switch_to_registered_scene(const SceneRoute& route)
{
    if (!SceneKeys::is_supported(route.target))
        throw_invalid_route_key(route.target);
    const auto builder = _scene_builders.find(route.target);
    if (builder == _scene_builders.end())
    {
        if (SceneKeys::is_game(route.target))
            throw std::logic_error("SceneManager received a valid but unregistered game SceneKey.");
        throw std::logic_error("SceneManager received a valid but unregistered engine-owned SceneKey.");
    }

    ELYSIA_LOG("scene", "Switching to SceneKey " << route.target
        << " with reload mode " << static_cast<int>(route.reload_mode));

    Scene* next_scene = nullptr;
    std::unique_ptr<Scene> staged_scene;
    if (route.reload_mode != SceneReloadMode::Recreate)
        next_scene = _scene_factory.find(route.target);
    if (!next_scene)
    {
        try
        {
            staged_scene = builder->second();
            next_scene = staged_scene.get();
        }
        catch (const elysia::core::RenderBackendError&) { throw; }
        catch (...)
        {
            return std::unexpected(make_failure(route.target, SceneBoundary::Enter));
        }
    }
    if (!next_scene)
        return std::unexpected(make_scene_boundary_failure(route.target, SceneBoundary::Enter,
                                    "Scene builder returned a null scene."));

    return switch_to_scene(next_scene, std::move(staged_scene), route);
}

void SceneManager::leave_current_scene(detail::SceneFailureCollector& failures)
{
    if (!_current_scene) return;
    Scene* leaving = _current_scene;
    const SceneKey key = _current_scene_key;
    failures.attempt_cleanup(key,SceneBoundary::Detach,"Scene detach",[&] { detach_from_scene(*leaving,key); });
    failures.attempt_cleanup(key,SceneBoundary::Exit,"Scene exit",[&] { leaving->lifecycle_exit(); });
    _current_scene = nullptr;
    _current_scene_key = SceneKeys::Invalid;
}

std::expected<void, SceneBoundaryFailure> SceneManager::switch_to_scene(
    Scene* next_scene,
    std::unique_ptr<Scene> staged_scene,
    const SceneRoute& route)
{
    if (!next_scene)
        return std::unexpected(make_scene_boundary_failure(route.target, SceneBoundary::Enter,
                                    "SceneManager received a null candidate scene."));

    Scene* old_scene = _current_scene;
    const SceneKey old_key = _current_scene_key;
    detail::SceneFailureCollector leave_failures;
    leave_current_scene(leave_failures);
    if (!leave_failures.empty())
    {
        leave_failures.attempt_cleanup(old_key,SceneBoundary::ObjectRemoval,"Discard previous scene",
            [&] { discard_scene(old_key,old_scene); });
        if (staged_scene)
            leave_failures.attempt_cleanup(route.target,SceneBoundary::ObjectRemoval,"Discard staged scene after leave failure",
                [&] { staged_scene->prepare_for_destruction(); });
        return leave_failures.finish();
    }

    elysia::tools::DebugDraw::instance()->clear();
    if (old_scene != next_scene || route.reload_mode != SceneReloadMode::Reuse)
        next_scene->reset_camera_runtime();

    if (route.reload_mode == SceneReloadMode::Recreate)
    {
        try
        {
            (void)_scene_factory.destroy(route.target);
        }
        catch (...)
        {
            detail::SceneFailureCollector failures;
            failures.capture(route.target,SceneBoundary::ObjectRemoval,"Recreate previous scene");
            if (staged_scene)
                failures.attempt_cleanup(route.target,SceneBoundary::ObjectRemoval,"Discard staged scene after recreate failure",
                    [&] { staged_scene->prepare_for_destruction(); });
            return failures.finish();
        }
    }

    next_scene->set_local_players(_local_players);
    next_scene->set_ui_device_access(_ui_device_access);
    if (_runtime_context)
        next_scene->bind_runtime_context(*_runtime_context);

    auto fail_candidate = [&](SceneBoundary boundary) -> std::expected<void, SceneBoundaryFailure> {
        detail::SceneFailureCollector failures;
        failures.capture(route.target,boundary,"Candidate operation");
        failures.attempt_cleanup(route.target,SceneBoundary::Detach,"Candidate detach",
            [&] { detach_from_scene(*next_scene,route.target); });
        if (next_scene->lifecycle_state() == SceneLifecycleState::Active)
            failures.attempt_cleanup(route.target,SceneBoundary::Exit,"Candidate exit",[&] { next_scene->lifecycle_exit(); });
        failures.attempt_cleanup(route.target,SceneBoundary::ObjectRemoval,"Candidate destruction",[&] {
            if (staged_scene) staged_scene->prepare_for_destruction();
            else discard_scene(route.target,next_scene);
        });
        return failures.finish();
    };

    if (route.reload_mode == SceneReloadMode::Reset)
    {
        try
        {
            next_scene->lifecycle_reset();
        }
        catch (...)
        {
            return fail_candidate(SceneBoundary::Reset);
        }
    }

    try
    {
        attach_to_scene(*next_scene,route.target);
    }
    catch (...)
    {
        return fail_candidate(SceneBoundary::Attach);
    }

    try
    {
        next_scene->lifecycle_enter(route.payload);
    }
    catch (...)
    {
        return fail_candidate(SceneBoundary::Enter);
    }

    if (staged_scene)
    {
        try
        {
            _scene_factory.store(route.target, staged_scene);
        }
        catch (...)
        {
            return fail_candidate(SceneBoundary::Attach);
        }
    }
    _current_scene = next_scene;
    _current_scene_key = route.target;
    return {};
}

void SceneManager::attach_to_scene(Scene& scene,SceneKey key)
{
    bool effect_bound = false,query_bound = false,runtime_bound = false,observer_bound = false;
    try
    {
        elysia::effects::EffectManager::instance()->bind_active_scene(scene);
        effect_bound = true;
        elysia::object_query::GameObjectQueryManager::instance()->bind_active_runtime(scene);
        query_bound = true;
        scene.attach_runtime_services();
        runtime_bound = true;
        scene.attach(this);
        observer_bound = true;
    }
    catch (...)
    {
        detail::SceneFailureCollector failures;
        failures.capture(key,SceneBoundary::Attach,"Runtime attach");
        if (observer_bound)
            failures.attempt_cleanup(key,SceneBoundary::Detach,"Attach rollback observer detach",[&] { scene.detach(this); });
        if (runtime_bound)
            failures.attempt_cleanup(key,SceneBoundary::Detach,"Attach rollback runtime detach",[&] { scene.detach_runtime_services(); });
        if (query_bound)
            failures.attempt_cleanup(key,SceneBoundary::Detach,"Attach rollback query unbind",
                [&] { elysia::object_query::GameObjectQueryManager::instance()->unbind_active_runtime(scene); });
        if (effect_bound)
            failures.attempt_cleanup(key,SceneBoundary::Detach,"Attach rollback effect unbind",
                [&] { elysia::effects::EffectManager::instance()->unbind_active_scene(scene); });
        failures.rethrow_if_failed();
    }
}

void SceneManager::detach_from_scene(Scene& scene,SceneKey key)
{
    detail::SceneFailureCollector failures;
    failures.attempt_cleanup(key,SceneBoundary::Detach,"Observer detach",[&] { scene.detach(this); });
    failures.attempt_cleanup(key,SceneBoundary::Detach,"Input routing reset",[&] { scene.reset_input_routing(); });
    failures.attempt_cleanup(key,SceneBoundary::Detach,"Runtime detach",[&] { scene.detach_runtime_services(); });
    failures.attempt_cleanup(key,SceneBoundary::Detach,"Query unbind",
        [&] { elysia::object_query::GameObjectQueryManager::instance()->unbind_active_runtime(scene); });
    failures.attempt_cleanup(key,SceneBoundary::Detach,"Effect unbind",
        [&] { elysia::effects::EffectManager::instance()->unbind_active_scene(scene); });
    failures.rethrow_if_failed();
}

void SceneManager::discard_scene(SceneKey key, Scene* expected)
{
    if (key == SceneKeys::Invalid) return;
    Scene* cached = _scene_factory.find(key);
    if (!cached || (expected && cached != expected)) return;
    (void)_scene_factory.destroy(key);
}

SceneBoundaryFailure SceneManager::make_failure(SceneKey key,SceneBoundary boundary,
    std::source_location origin) const
{
    detail::SceneFailureCollector failures;
    failures.capture(key,boundary,"Scene boundary",std::current_exception(),origin);
    return std::move(failures.finish().error());
}

void SceneManager::recover_from_failure(const SceneBoundaryFailure& failure)
{
    _pending_request = {};
    _has_pending_request = false;
    if (_recovering_failure)
    {
        detail::log_scene_failure(failure);
        notify_fault(failure);
        return;
    }

    _recovering_failure = true;
    struct Guard
    {
        bool& flag;
        ~Guard() { flag = false; }
    } guard{_recovering_failure};

    // No recovery callback may observe or bind services alongside a failed scene.
    // A healthy source can still be current when the requested target's builder fails.
    Scene* previous_scene = _current_scene;
    const SceneKey previous_key = _current_scene_key;
    detail::SceneFailureCollector cleanup_failures;
    leave_current_scene(cleanup_failures);
    if (!cleanup_failures.empty() && previous_key != failure.scene)
        cleanup_failures.attempt_cleanup(previous_key,SceneBoundary::ObjectRemoval,"Discard previous scene during recovery",
            [&] { discard_scene(previous_key,previous_scene); });
    cleanup_failures.attempt_cleanup(failure.scene,SceneBoundary::ObjectRemoval,"Discard failed scene before recovery",
        [&] { discard_scene(failure.scene,nullptr); });
    elysia::effects::EffectManager::instance()->clear_screen_effects();
    elysia::tools::DebugDraw::instance()->clear();

    try
    {
        if (auto result = cleanup_failures.finish(); !result)
        {
            auto cleanup_failure = failure;
            cleanup_failure.cleanup_failed = true;
            detail::append_scene_failure_context(cleanup_failure.diagnostic,result.error(),"Recovery cleanup");
            detail::log_scene_failure(cleanup_failure);
            notify_fault(cleanup_failure);
            return;
        }
    }
    catch (const elysia::core::RenderBackendError& error)
    {
        auto backend = error.failure();
        detail::append_scene_failure_context(backend.diagnostic,failure,"Recovery trigger");
        throw elysia::core::RenderBackendError(std::move(backend));
    }

    detail::log_scene_failure(failure);
    if (failure.cleanup_failed || !_failure_route_factory || _state == SceneManagerState::Faulted)
    {
        notify_fault(failure);
        return;
    }

    SceneKey recovery_key = failure.scene;
    try
    {
        SceneRoute route = _failure_route_factory(failure);
        recovery_key = route.target;
        if (route.target == failure.scene)
        {
            notify_fault(failure);
            return;
        }
        route.reload_mode = SceneReloadMode::Recreate;
        if (auto result = switch_to_registered_scene(route); !result)
        {
            detail::append_scene_failure_context(result.error().diagnostic,failure,"Recovery trigger");
            detail::log_scene_failure(result.error());
            notify_fault(result.error());
        }
    }
    catch (...)
    {
        try
        {
            auto recovery_failure = make_failure(recovery_key,SceneBoundary::Enter);
            detail::append_scene_failure_context(recovery_failure.diagnostic,failure,"Recovery trigger");
            detail::log_scene_failure(recovery_failure);
            notify_fault(recovery_failure);
        }
        catch (const elysia::core::RenderBackendError& error)
        {
            auto backend = error.failure();
            detail::append_scene_failure_context(backend.diagnostic,failure,"Recovery trigger");
            throw elysia::core::RenderBackendError(std::move(backend));
        }
    }
}

void SceneManager::throw_invalid_route_key(SceneKey key)
{
    if (key == SceneKeys::Invalid)
        throw std::logic_error("SceneManager received SceneKeys::Invalid.");
    throw std::logic_error("SceneManager received a SceneKey in the reserved range.");
}

bool SceneManager::shutdown() noexcept
{
    if (_shutdown_performed)
        return _shutdown_succeeded;
    _shutdown_performed = true;
    const bool was_faulted = _state == SceneManagerState::Faulted;
    _state = SceneManagerState::ShuttingDown;

    auto cleanup = [this](auto&& action) {
        try { action(); }
        catch (...)
        {
            _shutdown_succeeded = false;
            try { detail::log_scene_failure(make_failure(_current_scene_key,SceneBoundary::Exit)); }
            catch (const elysia::core::RenderBackendError& error)
            {
                elysia::tools::Logger::instance()->log_stream(elysia::tools::LogLevel::Error,"scene_shutdown",
                    [&](std::ostream& output) {
                        output << elysia::core::format_failure_diagnostic(error.failure().diagnostic,"RENDER-BACKEND","render");
                    },error.failure().diagnostic.origin);
            }
            catch (...) { detail::log_cleanup_exception("Shutdown diagnostic"); }
        }
    };

    if (_current_scene)
    {
        Scene* exiting = _current_scene;
        const SceneKey exiting_key = _current_scene_key;
        cleanup([&] { detach_from_scene(*exiting,exiting_key); });
        cleanup([&] { exiting->lifecycle_exit(); });
        _current_scene = nullptr;
        _current_scene_key = SceneKeys::Invalid;
        (void)exiting_key;
    }
    cleanup([] { elysia::tools::DebugDraw::instance()->clear(); });
    cleanup([] { elysia::effects::EffectManager::instance()->clear_screen_effects(); });
    cleanup([] { elysia::camera::CameraManager::instance()->reset_all(); });
    cleanup([&] {
        if (!_scene_factory.destroy_all_scene())
            throw std::runtime_error("One or more scenes failed to prepare for destruction.");
    });
    cleanup([] { elysia::gameplay::ControllerManager::instance()->shutdown(); });

    _scene_builders.clear();
    _local_players.clear_for_shutdown();
    _ui_device_access = {};
    _runtime_context = nullptr;
    _failure_route_factory = {};
    _pending_request = {};
    _has_pending_request = false;
    _is_processing_request = false;
    _recovering_failure = false;

    if (was_faulted || !_shutdown_succeeded)
        _state = SceneManagerState::Faulted;
    else
        _state = SceneManagerState::Stopped;
    return _shutdown_succeeded;
}
} // namespace elysia::scene
