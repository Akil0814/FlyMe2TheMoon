#include "scene.h"

#include "detail/scene_input_order.h"
#include "detail/scene_failure_boundary.h"

#include "../core/render/debug_draw_projection.h"
#include "../core/render/render_command_projection.h"
#include "../core/render/sdl_render_command_executor.h"
#include "../physics/contracts/physics_participant.h"
#include "../physics/physics_debug_draw.h"
#include "../tools/debug_draw.h"
#include "../ui/core/ui_render_command_range_utils.h"
#include "../effects/runtime/effect_manager.h"

#include <algorithm>
#include <cassert>
#include <exception>
#include <stdexcept>

namespace elysia::scene
{
namespace
{
template <typename Entry>
void erase_object_entries(std::vector<Entry>& entries, const elysia::core::SceneObject* object)
{
    std::erase_if(entries, [object](const Entry& entry) { return entry.object == object; });
}

constexpr auto physics_debug_categories=elysia::physics::kPhysicsDebugCategories;
} // namespace

Scene::Scene() : Scene(SceneRuntimeFeatures{}) {}

Scene::Scene(SceneRuntimeFeatures features)
{
    if (features.physics && !features.fixed_step)
        throw std::invalid_argument("Scene physics requires a fixed-step runtime.");
    if (features.camera)
        _camera_runtime = std::make_unique<SceneCameraRuntime>(*features.camera);
    if (features.fixed_step)
        _fixed_step = std::make_unique<FixedStepRuntime>(*features.fixed_step);
    if (features.physics)
        _physics_world = std::make_unique<elysia::physics::PhysicsWorld>(*features.physics);
}

Scene::~Scene()
{
    release_owned_objects_noexcept();
}

Scene::ObjectMutationScope::ObjectMutationScope(
    const Scene& scene, const char* phase, bool block_additions) noexcept
    : _scene(scene)
    , _previous_addition_phase(scene._object_addition_phase)
    , _previous_removal_phase(scene._object_removal_phase)
    , _block_additions(block_additions)
{
    ++_scene._object_removal_block_depth;
    _scene._object_removal_phase = phase;
    if (_block_additions)
    {
        ++_scene._object_addition_block_depth;
        _scene._object_addition_phase = phase;
    }
}

Scene::ObjectMutationScope::~ObjectMutationScope()
{
    --_scene._object_removal_block_depth;
    _scene._object_removal_phase = _previous_removal_phase;
    if (_block_additions)
    {
        --_scene._object_addition_block_depth;
        _scene._object_addition_phase = _previous_addition_phase;
    }
}

void Scene::require_object_addition_allowed() const
{
    const char* phase = nullptr;
    if (_retiring_objects)
        phase = "object retirement";
    else if (_lifecycle_state == SceneLifecycleState::PreparingDestruction ||
             _lifecycle_state == SceneLifecycleState::PreparedForDestruction)
        phase = "scene destruction";
    else if (_object_addition_block_depth != 0)
        phase = _object_addition_phase;
    if (phase)
        throw SceneBoundaryLogicError(SceneBoundary::ObjectRegistration,
            std::string("Scene objects cannot be added during ") + phase +
            ". Add objects from on_before_update(), on_after_update(), or on_fixed_update() before the physics step.");
}

void Scene::require_object_removal_allowed() const
{
    const char* phase = _retiring_objects ? "object retirement"
        : _object_removal_block_depth != 0 ? _object_removal_phase : nullptr;
    if (phase)
        throw SceneBoundaryLogicError(SceneBoundary::ObjectRemoval,
            std::string("Scene objects cannot be cleared or retired during ") + phase +
            ". Mark objects with destroy(), or clear them from on_before_update(), on_after_update(), or on_fixed_update() before the physics step.");
}

bool Scene::owns_object(const elysia::core::SceneObject& object) const
{
    return contains_object_address(&object);
}

bool Scene::contains_object_address(const elysia::core::SceneObject* object) const
{
    if (!object)
        return false;
    for (const auto& layer : _object_layers)
        for (const auto& candidate : layer)
            if (candidate.get() == object)
                return true;
    return std::ranges::any_of(_ui_roots, [object](const auto& candidate) {
        return candidate.get() == object;
    });
}

void Scene::set_all_gameplay_input_blocked(bool blocked)
{
    _input_router.set_all_gameplay_input_blocked(blocked);
}

void Scene::reset_input_routing()
{
    _input_router.reset();
}

void Scene::consume_input(const elysia::input::RawInputEvent& event)
{
    _input_router.consume_input(event);
}

void Scene::pause()
{
    if (_paused)
        return;
    _paused = true;
    _input_router.reset();
    on_pause_changed(true);
}

void Scene::resume()
{
    if (!_paused)
        return;
    _paused = false;
    on_pause_changed(false);
}

void Scene::lifecycle_enter(const ScenePayload& payload)
{
    if (_lifecycle_state != SceneLifecycleState::Inactive)
        throw std::logic_error("Scene can only enter from the inactive state.");
    _lifecycle_state = SceneLifecycleState::Entering;
    try
    {
        on_enter(payload);
        _lifecycle_state = SceneLifecycleState::Active;
    }
    catch (...)
    {
        cancel_camera_activity();
        _lifecycle_state = SceneLifecycleState::Inactive;
        throw;
    }
}

void Scene::lifecycle_exit()
{
    if (_lifecycle_state != SceneLifecycleState::Active)
        throw std::logic_error("Scene can only exit from the active state.");
    _lifecycle_state = SceneLifecycleState::Exiting;
    detail::SceneFailureCollector failures;
    try
    {
        on_exit();
    }
    catch (...)
    {
        failures.capture(SceneKeys::Invalid,SceneBoundary::Exit,"Scene exit");
    }
    try
    {
        remove_destroyed_objects();
    }
    catch (...)
    {
        failures.capture_cleanup(SceneKeys::Invalid,SceneBoundary::Exit,"Exit object retirement");
    }

    cancel_camera_activity();

    _lifecycle_state = SceneLifecycleState::Inactive;
    failures.rethrow_if_failed();
}

void Scene::lifecycle_reset()
{
    if (_lifecycle_state != SceneLifecycleState::Inactive)
        throw std::logic_error("Scene can only reset while inactive.");
    _lifecycle_state = SceneLifecycleState::Resetting;
    detail::SceneFailureCollector failures;
    try
    {
        reset_input_routing();
        _paused = false;
        if (_fixed_step)
            _fixed_step->reset();
        reset_camera_runtime();
        on_runtime_reset();
        on_reset();
    }
    catch (...)
    {
        failures.capture(SceneKeys::Invalid,SceneBoundary::Reset,"Scene reset");
    }
    try
    {
        remove_destroyed_objects();
    }
    catch (...)
    {
        failures.capture_cleanup(SceneKeys::Invalid,SceneBoundary::Reset,"Reset object retirement");
    }

    _lifecycle_state = SceneLifecycleState::Inactive;
    failures.rethrow_if_failed();
}

void Scene::lifecycle_input(const elysia::input::InputSnapshot& input)
{
    if (_lifecycle_state != SceneLifecycleState::Active)
        return;
    _input_router.route(input);
}

void Scene::lifecycle_update(double delta)
{
    if (_lifecycle_state != SceneLifecycleState::Active)
        return;


    detail::SceneFailureCollector failures;
    try
    {
        auto* debug_draw = elysia::tools::DebugDraw::instance();
        debug_draw->clear_categories(physics_debug_categories);

        on_before_update(delta);

        {
            const ObjectMutationScope mutation_scope(*this, "object update");
            for (const UpdatableEntry& entry : _updatables)
            {
                auto* object = entry.object;
                if (!object || object->is_destroyed() || !object->is_active())
                    continue;
                if (_paused && !object->update_when_paused())
                    continue;
                entry.updatable->update(delta);
            }
        }

        {
            const ObjectMutationScope mutation_scope(*this, "UI presentation update");
            for (const auto& ui_root : _ui_roots)
            {
                if (!ui_root || ui_root->is_destroyed() || !ui_root->is_active())
                    continue;
                if (_paused && !ui_root->update_when_paused())
                    continue;
                ui_root->update_presentation_animations(delta);
            }
        }

        const auto debug_capture = elysia::physics::physics_debug_capture_for(*debug_draw);
        if (_physics_world)
            _physics_world->set_debug_capture(debug_capture);

        if (_fixed_step && !_paused)
        {
            _fixed_step->advance(fixed_step_frame_delta(delta), [this](std::uint64_t tick, double step_delta) {
                on_fixed_update(tick, step_delta);
                if (_physics_world)
                {
                    const ObjectMutationScope mutation_scope(*this, "physics step");
                    _physics_world->step(step_delta);
                }
            });
        }
        if (_physics_world)
        {
            const double alpha = _fixed_step ? _fixed_step->stats().interpolation_alpha : 0.0;
            _physics_world->finalize_frame(alpha);
        }

        if (_physics_world && debug_capture != elysia::physics::PhysicsDebugCapture::None)
        {
            elysia::physics::submit_physics_debug_snapshot(
                _physics_world->debug_snapshot(), *debug_draw);
        }

        if (_camera_runtime)
            _camera_runtime->advance(delta, _paused, *this);

        on_after_update(delta);
    }
    catch (...)
    {
        failures.capture(SceneKeys::Invalid,SceneBoundary::Update,"Scene update");
    }

    try
    {
        remove_destroyed_objects();
    }
    catch (...)
    {
        failures.capture_cleanup(SceneKeys::Invalid,SceneBoundary::Update,"Update object retirement");
    }

    failures.rethrow_if_failed();
}

void Scene::lifecycle_render(SDL_Renderer* renderer)
{
    if (_lifecycle_state != SceneLifecycleState::Active || !renderer)
        return;

    std::vector<elysia::core::RenderCommand> render_commands;
    std::vector<elysia::core::ScreenRenderCommand> projected_render_commands;
    std::vector<elysia::core::UiRenderCommand> ui_render_commands;
    render_commands.reserve(256);
    projected_render_commands.reserve(256);
    ui_render_commands.reserve(256);

    {
        const ObjectMutationScope mutation_scope(*this, "world render");
        for (const auto& layer : _object_layers)
        {
            render_commands.clear();
            projected_render_commands.clear();
            for (const auto& object : layer)
            {
                if (object && !object->is_destroyed() && object->is_visible())
                    object->submit_render_commands(render_commands);
            }
            if (render_commands.empty())
                continue;
            elysia::core::project_render_commands_to_screen(
                render_commands, camera(), projected_render_commands);
            elysia::core::require_render_success(elysia::core::execute_render_commands(renderer,projected_render_commands));
        }
    }

    auto* debug_draw = elysia::tools::DebugDraw::instance();
    const bool has_visible_debug_commands = debug_draw->enabled()
        && std::ranges::any_of(debug_draw->commands(), [debug_draw](const auto& command) {
               return debug_draw->is_enabled(command.category);
           });
    if (has_visible_debug_commands)
    {
        std::vector<elysia::core::UiRenderCommand> debug_commands;
        debug_commands.reserve(debug_draw->commands().size());
        elysia::core::append_projected_debug_draw_commands(
            debug_draw->commands(), debug_draw->enabled_categories(), camera(), debug_commands);
        elysia::core::require_render_success(elysia::core::execute_render_commands(renderer,debug_commands));
    }

    const elysia::core::Rect viewport(0, 0,
        _runtime_context ? static_cast<float>(_runtime_context->logical_width()) : 0.0f,
        _runtime_context ? static_cast<float>(_runtime_context->logical_height()) : 0.0f);
    auto* effects = elysia::effects::EffectManager::instance();
    elysia::core::require_render_success(effects->render_screen_effects(renderer, elysia::effects::ScreenEffectLayer::BeforeUi, viewport));
    {
        const ObjectMutationScope mutation_scope(*this, "UI render");
        for (const auto& ui_root : _ui_roots)
        {
            if (!ui_root || ui_root->is_destroyed() || !ui_root->is_visible())
                continue;
            const std::size_t begin = ui_render_commands.size();
            ui_root->submit_ui_render_commands(ui_render_commands);
            elysia::ui::render_command_range_utils::apply_translation_to_range(
                ui_render_commands, begin, ui_root->presentation_translation());
        }
    }
    elysia::core::require_render_success(elysia::core::execute_render_commands(renderer,ui_render_commands));
    elysia::core::require_render_success(effects->render_screen_effects(renderer, elysia::effects::ScreenEffectLayer::AfterUi, viewport));
}

void Scene::attach_runtime_services()
{
    if (_runtime_services_attached)
        throw std::logic_error("Scene runtime services are already attached.");
    _runtime_services_attached = true;
    try { on_runtime_attach(); }
    catch (...)
    {
        detail::SceneFailureCollector failures;
        failures.capture(SceneKeys::Invalid,SceneBoundary::Attach,"Runtime attach");
        failures.attempt_cleanup(SceneKeys::Invalid,SceneBoundary::Detach,"Runtime attach rollback",[&] { on_runtime_detach(); });
        _runtime_services_attached = false;
        failures.rethrow_if_failed();
    }
}

void Scene::detach_runtime_services()
{
    if (!_runtime_services_attached)
        return;
    _runtime_services_attached = false;
    on_runtime_detach();
}

void Scene::prepare_for_destruction()
{
    if (_lifecycle_state == SceneLifecycleState::PreparedForDestruction)
        return;
    if (_lifecycle_state != SceneLifecycleState::Inactive)
        throw std::logic_error("Scene must be inactive before destruction is prepared.");
    if (_runtime_services_attached)
        throw std::logic_error("Scene runtime services must be detached before destruction.");

    _lifecycle_state = SceneLifecycleState::PreparingDestruction;
    cancel_camera_activity();
    detail::SceneFailureCollector failures;
    try
    {
        clear_scene_objects();
    }
    catch (...)
    {
        failures.capture(SceneKeys::Invalid,SceneBoundary::ObjectRemoval,"Destruction object retirement");
    }
    try
    {
        reset_input_routing();
    }
    catch (...)
    {
        failures.capture(SceneKeys::Invalid,SceneBoundary::ObjectRemoval,"Destruction input reset");
    }
    clear_runtime_context();
    _lifecycle_state = SceneLifecycleState::PreparedForDestruction;
    failures.rethrow_if_failed();
}

void Scene::register_scene_object_interfaces(elysia::core::SceneObject* object)
{
    if (!object)
        return;

    auto* game_object = dynamic_cast<elysia::core::GameObject*>(object);
    auto* participant = dynamic_cast<elysia::physics::PhysicsParticipant*>(object);
    if (participant && !game_object)
        throw std::logic_error("PhysicsParticipant must also derive from GameObject.");
    if (participant && !_physics_world)
        throw std::logic_error("A PhysicsParticipant cannot be added to a scene without physics.");

    if (participant)
    {
        const auto colliders = participant->collider_definitions();
        const auto handle = _physics_world->register_object(
            *game_object, participant->body_definition(), colliders);
        if (!handle.is_valid())
            throw std::runtime_error("Scene physics registration failed for a GameObject.");
        try
        {
            PhysicsRegistrationEntry registration{object, handle,
                std::make_unique<std::vector<elysia::physics::ColliderId>>(std::size_t{0})};
            registration.colliders->reserve(colliders.size());
            for (std::size_t index = 0; index < colliders.size(); ++index)
                registration.colliders->push_back(_physics_world->collider_id(handle, index));
            _physics_registrations.push_back(std::move(registration));
        }
        catch (...)
        {
            (void)_physics_world->unregister_object(handle);
            throw;
        }
        participant->bind_physics(*_physics_world, handle);
    }

    if (auto* updatable = dynamic_cast<elysia::core::Updatable*>(object))
        _updatables.push_back({object, updatable});
    if (auto* receiver = dynamic_cast<elysia::ui::UiInputFrameReceiver*>(object))
    {
        scene_input_order::insert_receiver_entry_sorted(
            _ui_frame_receivers, UiInputFrameReceiverEntry{object, receiver, _next_input_registration++});
    }
    if (auto* receiver = dynamic_cast<elysia::ui::UiInputEventReceiver*>(object))
    {
        scene_input_order::insert_receiver_entry_sorted(
            _ui_event_receivers, UiInputEventReceiverEntry{object, receiver, _next_input_registration++});
    }
    try
    {
        on_scene_object_registered(*object);
    }
    catch (...)
    {
        detail::SceneFailureCollector failures;
        failures.capture(SceneKeys::Invalid,SceneBoundary::ObjectRegistration,"Object registration callback");
        failures.attempt_cleanup(SceneKeys::Invalid,SceneBoundary::ObjectRemoval,"ObjectRegistration rollback callback",
            [&] {
                const ObjectMutationScope mutation_scope(*this, "object registration rollback");
                on_scene_object_removing(*object);
            });
        failures.rethrow_if_failed();
    }
}

void Scene::visit_game_objects(
    elysia::core::DepthLayerMask layers,
    const elysia::object_query::GameObjectVisitor& visitor) const
{
    const ObjectMutationScope mutation_scope(*this, "object query");
    for (std::size_t index = 0; index < _object_layers.size(); ++index)
    {
        const auto depth_layer = static_cast<elysia::core::DepthLayer>(index);
        if (!layers.contains(depth_layer))
            continue;
        for (const auto& object : _object_layers[index])
            if (object && !visitor(*object))
                return;
    }
}

void Scene::dispatch_ui_frame(const elysia::ui::UiInputFrame& input)
{
    const auto receivers = _ui_frame_receivers;
    for (const auto& entry : receivers)
    {
        if (!std::ranges::any_of(_ui_frame_receivers, [&](const auto& current) {
                return current.registration == entry.registration;
            }))
            continue;
        auto* object = entry.object;
        if (!object || object->is_destroyed() || !object->is_active())
            continue;
        if (_paused && !object->receive_input_when_paused())
            continue;
        entry.receiver->on_ui_input_frame(input);
    }
}

bool Scene::dispatch_ui_events(const std::vector<elysia::ui::UiInputEvent>& events)
{
    bool consumed = false;
    const auto receivers = _ui_event_receivers;
    for (const auto& event : events)
    {
        for (const auto& entry : receivers)
        {
            if (!std::ranges::any_of(_ui_event_receivers, [&](const auto& current) {
                    return current.registration == entry.registration;
                }))
                continue;
            auto* object = entry.object;
            if (!object || object->is_destroyed() || !object->is_active())
                continue;
            if (_paused && !object->receive_input_when_paused())
                continue;
            if (entry.receiver->on_ui_input_event(event))
            {
                consumed = true;
                break;
            }
        }
    }
    return consumed;
}

void Scene::remove_destroyed_objects()
{
    require_object_removal_allowed();
    _retiring_objects = true;
    struct Guard
    {
        bool& value;
        ~Guard() { value = false; }
    } guard{_retiring_objects};

    detail::SceneFailureCollector failures;
    auto record_failure = [&](const char* stage) {
        failures.capture(SceneKeys::Invalid,SceneBoundary::ObjectRemoval,stage);
    };
    auto retire = [&](elysia::core::SceneObject& object) {
        try
        {
            on_scene_object_removing(object);
        }
        catch (...)
        {
            record_failure("ObjectRemoval callback");
        }

        const auto registration = std::ranges::find_if(
            _physics_registrations,
            [&object](const PhysicsRegistrationEntry& entry) { return entry.object == &object; });
        if (registration != _physics_registrations.end())
        {
            try
            {
                if (_physics_world)
                    (void)_physics_world->unregister_object(registration->handle);
            }
            catch (...)
            {
                record_failure("ObjectRemoval physics unregister");
            }
            if (auto* participant = dynamic_cast<elysia::physics::PhysicsParticipant*>(&object))
                participant->unbind_physics();
        }
    };

    // Keep the batch fixed: callbacks may destroy earlier objects or reset a
    // retiring object. Neither operation may bypass retirement or revive it.
    std::vector<elysia::core::SceneObject*> batch;
    for (;;)
    {
        batch.clear();
        for (auto& layer : _object_layers)
            for (auto& object : layer)
                if (object && object->is_destroyed())
                    batch.push_back(object.get());
        for (auto& object : _ui_roots)
            if (object && object->is_destroyed())
                batch.push_back(object.get());
        if (batch.empty())
            break;

        for (auto* object : batch)
            retire(*object);

        const auto in_batch = [&](const auto* object) {
            return std::ranges::find(batch, object) != batch.end();
        };
        const auto retired_entry = [&](const auto& entry) { return in_batch(entry.object); };
        std::erase_if(_updatables, retired_entry);
        std::erase_if(_ui_frame_receivers, retired_entry);
        std::erase_if(_ui_event_receivers, retired_entry);
        std::erase_if(_physics_registrations, retired_entry);
        // Restore the destruction flag before invoking any destructor.
        for (auto* object : batch)
            object->destroy();
        for (auto& layer : _object_layers)
            std::erase_if(layer, [&](const auto& object) {
                return !object || in_batch(object.get());
            });
        std::erase_if(_ui_roots, [&](const auto& object) {
            return !object || in_batch(object.get());
        });
    }

    failures.rethrow_if_failed();
}

std::span<const elysia::physics::ColliderId> Scene::registered_physics_colliders(
    const elysia::core::SceneObject& object) const noexcept
{
    const auto found = std::ranges::find_if(_physics_registrations,
        [&object](const auto& entry) { return entry.object == &object; });
    return found == _physics_registrations.end()
        ? std::span<const elysia::physics::ColliderId>{}
        : std::span<const elysia::physics::ColliderId>{*found->colliders};
}

void Scene::clear_scene_objects()
{
    require_object_removal_allowed();
    for (auto& layer : _object_layers)
        for (auto& object : layer)
            if (object)
                object->destroy();
    for (auto& object : _ui_roots)
        if (object)
            object->destroy();
    remove_destroyed_objects();
}

void Scene::rollback_owned_object(elysia::core::SceneObject* object) noexcept
{
    if (!object)
        return;
    const ObjectMutationScope mutation_scope(*this, "object registration rollback");
    const auto registration = std::ranges::find_if(
        _physics_registrations,
        [object](const PhysicsRegistrationEntry& entry) { return entry.object == object; });
    if (registration != _physics_registrations.end())
    {
        if (_physics_world)
            (void)_physics_world->unregister_object(registration->handle);
        if (auto* participant = dynamic_cast<elysia::physics::PhysicsParticipant*>(object))
            participant->unbind_physics();
    }
    erase_object_entries(_updatables, object);
    erase_object_entries(_ui_frame_receivers, object);
    erase_object_entries(_ui_event_receivers, object);
    erase_object_entries(_physics_registrations, object);
    for (auto& layer : _object_layers)
        std::erase_if(layer, [object](const auto& candidate) { return candidate.get() == object; });
    std::erase_if(_ui_roots, [object](const auto& candidate) { return candidate.get() == object; });
}

void Scene::release_owned_objects_noexcept() noexcept
{
    _retiring_objects = true;
    for (const auto& registration : _physics_registrations)
    {
        if (_physics_world)
            (void)_physics_world->unregister_object(registration.handle);
        if (auto* participant =
                dynamic_cast<elysia::physics::PhysicsParticipant*>(registration.object))
        {
            participant->unbind_physics();
        }
    }
    _physics_registrations.clear();
    _updatables.clear();
    _ui_frame_receivers.clear();
    _ui_event_receivers.clear();
    _ui_roots.clear();
    for (auto& layer : _object_layers)
        layer.clear();
}

void Scene::notify_scene_request(const SceneRequest& request)
{
    notify_observers([&](SceneRequestObserver& observer) { observer.on_scene_request(request); });
}

void Scene::request_scene_switch(const SceneRoute& route)
{
    notify_scene_request(SceneRequest{SceneRequestType::Switch, route});
}

void Scene::request_scene_switch(
    SceneKey target,
    const ScenePayload& payload,
    SceneReloadMode reload_mode)
{
    request_scene_switch(SceneRoute{target, payload, reload_mode});
}

void Scene::request_quit()
{
    notify_scene_request(SceneRequest{SceneRequestType::Quit, {}});
}

const SceneRuntimeContext& Scene::runtime_context() const
{
    if (!_runtime_context)
        throw std::logic_error("Scene::runtime_context called before a runtime context was bound.");
    return *_runtime_context;
}

void Scene::bind_runtime_context(const SceneRuntimeContext& context) noexcept
{
    _runtime_context = &context;
}

void Scene::clear_runtime_context() noexcept
{
    _runtime_context = nullptr;
}

const elysia::camera::Camera& Scene::camera() const
{
    return camera_runtime().presented_camera();
}

elysia::physics::PhysicsWorld& Scene::physics_world()
{
    if (!_physics_world)
        throw std::logic_error("Scene does not have a physics runtime.");
    return *_physics_world;
}

const elysia::physics::PhysicsWorld& Scene::physics_world() const
{
    if (!_physics_world)
        throw std::logic_error("Scene does not have a physics runtime.");
    return *_physics_world;
}

SceneCameraRuntime& Scene::camera_runtime()
{
    if (!_camera_runtime)
        throw std::logic_error("Scene does not have a camera runtime.");
    return *_camera_runtime;
}

const SceneCameraRuntime& Scene::camera_runtime() const
{
    if (!_camera_runtime)
        throw std::logic_error("Scene does not have a camera runtime.");
    return *_camera_runtime;
}

void Scene::cancel_camera_activity() noexcept
{
    if (_camera_runtime)
        _camera_runtime->cancel_activity();
}

void Scene::reset_camera_runtime() noexcept
{
    if (_camera_runtime)
        _camera_runtime->reset();
}

std::optional<elysia::camera::CameraFocus> Scene::resolve_camera_focus(
    [[maybe_unused]] elysia::camera::CameraSlot slot
) const
{
    return std::nullopt;
}

bool Scene::add_game_object(std::unique_ptr<elysia::core::GameObject> object)
{
    if (!object)
        return false;
    const auto index = static_cast<std::size_t>(object->depth_layer());
    if (index >= _object_layers.size())
        return false;
    auto& layer = _object_layers[index];
    const auto position = std::upper_bound(
        layer.begin(), layer.end(), object->order_in_layer(),
        [](int order, const auto& existing) { return order < existing->order_in_layer(); });
    layer.insert(position, std::move(object));
    return true;
}

bool Scene::add_ui_root(std::unique_ptr<elysia::ui::UiElement> object)
{
    if (!object)
        return false;
    const auto position = std::upper_bound(
        _ui_roots.begin(), _ui_roots.end(), object->order(),
        [](int order, const auto& existing) { return order < existing->order(); });
    _ui_roots.insert(position, std::move(object));
    return true;
}
} // namespace elysia::scene
