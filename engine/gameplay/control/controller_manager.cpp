#include "controller_manager.h"
#include "../../tools/logger.h"
#include "../scene/gameplay_scene.h"
#include <algorithm>
#include <cmath>
namespace elysia::gameplay
{
using namespace elysia::input;
namespace
{
bool has_pointer_position(const RawInputEvent &event)
{
    return event.source.is_mouse() && event.device == InputDevice::Mouse &&
           (event.type == RawInputEventType::MouseMoved ||
            ((event.type == RawInputEventType::ControlPressed ||
              event.type == RawInputEventType::ControlReleased) && is_mouse_button_control(event.control)));
}
} // namespace
void ControllerManager::initialize()
{
    if (!_initialized)
    {
        _initialized = true;
        ++_runtime;
    }
}
void ControllerManager::shutdown()
{
    elysia::scene::detail::SceneFailureCollector failures;
    failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Exit,
        "Controller session shutdown", [this] { end_session(); });
    _initialized = false;
    ++_runtime;
    failures.rethrow_if_failed();
}
std::expected<void, ControllerError> ControllerManager::begin_session()
{
    if (!_initialized)
        return std::unexpected(ControllerError::NotInitialized);
    if (_ending_session)
        return std::unexpected(ControllerError::SessionEnding);
    if (_session)
        return std::unexpected(ControllerError::SessionAlreadyActive);
    ++_runtime;
    _session = true;
    return {};
}
void ControllerManager::end_session()
{
    if (_ending_session)
        return;
    dispatch(elysia::scene::SceneBoundary::Exit, [this] {
        _ending_session = true;
        struct Guard
        {
            bool& flag;
            ~Guard() noexcept { flag = false; }
        } guard{_ending_session};
        _session = false;
        for (const auto &r : _requests)
        {
            r->operation.finish(ControllerError::NoSession);
            release_reservation(*r);
        }
        for (auto& [id, entry] : _entries)
        {
            entry.session_retiring = !entry.removed;
            entry.removed = true;
            entry.available = false;
            entry.reserved_target = nullptr;
            entry.reserved_scene = {};
        }
        ++_runtime;
        elysia::scene::detail::SceneFailureCollector failures;
        for (auto& [id, entry] : _entries)
            if (std::exchange(entry.session_retiring, false))
                failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Exit,
                    "Controller session cancellation", [&] { detach(entry, InputCancelReason::Unbound); });
        failures.rethrow_if_failed();
    });
}
ControllerManager::Entry *ControllerManager::find(ControllerHandle h)
{
    auto it = _entries.find(h.instance);
    if (!_session || h.runtime != _runtime || it == _entries.end() || it->second.removed ||
        it->second.command.controller != h)
        return nullptr;
    return &it->second;
}
SceneControlContext *ControllerManager::context(SceneControlToken token)
{
    auto it = _contexts.find(token.instance);
    return it != _contexts.end() && it->second->token() == token && !it->second->_retiring ? it->second
                                                                                           : nullptr;
}
std::expected<ControllerHandle, ControllerError> ControllerManager::add(
    ControllerCreateInfo info, std::unique_ptr<Controller> controller)
{
    if (!_session)
        return std::unexpected(ControllerError::NoSession);
    if (info.scope == ControllerScope::Scene && !context(info.scene))
        return std::unexpected(ControllerError::InvalidContext);
    if (auto *local = dynamic_cast<LocalPlayerController *>(controller.get()))
    {
        if (!local->player().value)
            return std::unexpected(ControllerError::InvalidPlayer);
        if (!local->input_map().valid())
            return std::unexpected(ControllerError::InvalidMap);
    }
    auto h = ControllerHandle{_runtime, _next_id++};
    controller->_handle = h;
    controller->_submit = [this, h](ActionInputResult input) {
        if (auto *e = find(h))
            ingest(*e, std::move(input));
    };
    Entry e;
    e.scope = info.scope;
    e.owner = info.scope == ControllerScope::Scene ? info.scene : SceneControlToken{};
    e.controller = std::move(controller);
    e.command.controller = h;
    e.eligible_dispatch = _dispatch + 1;
    _entries.emplace(h.instance, std::move(e));
    return h;
}
void ControllerManager::flush()
{
    if (_flushing)
        return;
    _flushing = true;
    struct Guard
    {
        bool &flag;
        ~Guard()
        {
            flag = false;
        }
    } guard{_flushing};
    elysia::scene::detail::SceneFailureCollector failures;
    while (!_pending.empty())
    {
        auto operation = std::move(_pending.front());
        _pending.pop_front();
        if (operation->operation.pending())
        {
            try { commit(operation); }
            catch (...)
            {
                failures.capture(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Update,
                    "Controller operation callback");
                operation->operation.finish(ControllerError::CallbackFailed);
                release_reservation(*operation);
                fail_requests(operation->handle, ControllerError::CallbackFailed);
                if (Entry* entry = find(operation->handle); entry && entry->target)
                    failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Detach,
                        "Failed controller operation rollback", [&] { detach(*entry, InputCancelReason::Unbound); });
            }
        }
    }
    std::erase_if(_requests, [](const auto &r) { return !r->operation.pending(); });
    std::erase_if(_entries, [](auto &item) { return item.second.removed; });
    failures.rethrow_if_failed();
}
void ControllerManager::cancel(Entry &e, InputCancelReason reason)
{
    ++e.cancel_generation;
    e.command.state = {};
    e.command.events.clear();
    e.command.deltas.clear();
    e.command.pointer_position.reset();
    e.command.pointer_updated = false;
    e.command.input_device = InputDevice::Unknown;
    if (e.cancelling)
        return;
    e.cancelling = true;
    struct Guard
    {
        bool &flag;
        ~Guard()
        {
            flag = false;
        }
    } guard{e.cancelling};
    if (auto *local = dynamic_cast<LocalPlayerController *>(e.controller.get()))
        local->_map.reset_state();
    elysia::scene::detail::SceneFailureCollector failures;
    failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Update,
        "Controller cancellation", [&] { e.controller->cancelled(reason); });
    if (e.receiver)
        failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Update,
            "Control target cancellation", [&] { e.receiver->on_control_cancelled(reason); });
    failures.rethrow_if_failed();
}
void ControllerManager::detach(Entry &e, InputCancelReason reason)
{
    if (auto *local = dynamic_cast<LocalPlayerController *>(e.controller.get()))
        if (auto it = _contexts.find(e.bound_scene.instance);
            it != _contexts.end() && it->second->token() == e.bound_scene)
            it->second->_scene.local_players().clear_keyboard_requirement(local->player());
    // Detach before callbacks so reentrant operations cannot deliver to this target.
    auto *receiver = std::exchange(e.receiver, nullptr);
    auto *old_target = e.target;
    const auto old_scene = e.bound_scene;
    e.target = nullptr;
    e.bound_scene = {};
    e.available = false;
    ++e.command.binding_generation;
    elysia::scene::detail::SceneFailureCollector failures;
    failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Detach,
        "Controller detachment", [&] { cancel(e, reason); });
    // A controller callback may have released the old receiver.
    auto old_context = _contexts.find(old_scene.instance);
    if (receiver && old_context != _contexts.end() && old_context->second->token() == old_scene &&
        old_context->second->_scene.contains_control_target(old_target))
        failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Detach,
            "Detached target cancellation", [&] { receiver->on_control_cancelled(reason); });
    failures.rethrow_if_failed();
}
std::expected<void, ControllerError> ControllerManager::remove(ControllerHandle h)
{
    return dispatch(elysia::scene::SceneBoundary::Detach, [&]() -> std::expected<void, ControllerError> {
        auto *e = find(h);
        if (!e)
            return std::unexpected(ControllerError::InvalidHandle);
        fail_requests(h, ControllerError::InvalidHandle);
        e->removed = true;
        detach(*e, InputCancelReason::Unbound);
        return {};
    });
}
ControllerOperation ControllerManager::unbind(ControllerHandle h)
{
    auto r = std::make_shared<Request>();
    r->kind = RequestKind::Unbind;
    r->handle = h;
    return request(std::move(r));
}
std::expected<void, ControllerError> ControllerManager::validate_binding(Entry &e, SceneControlToken token,
                                                                         elysia::core::GameObject &target)
{
    auto *ctx = context(token);
    if (!ctx || !ctx->_active || (e.scope == ControllerScope::Scene && e.owner != token))
        return std::unexpected(ControllerError::InvalidContext);
    if (!ctx->_scene.owns_object(target) || target.is_destroyed() ||
        !dynamic_cast<ControlCommandReceiver *>(&target))
        return std::unexpected(ControllerError::InvalidTarget);
    auto *local = dynamic_cast<LocalPlayerController *>(e.controller.get());
    if (local && !ctx->_scene.local_players().contains(local->player()))
        return std::unexpected(ControllerError::InvalidPlayer);
    if (local &&
        !ctx->_scene.local_players().accepts_keyboard_map(local->player(), local->_map.keyboard_controls()))
        return std::unexpected(ControllerError::InvalidMap);
    for (auto &[id, other] : _entries)
    {
        if (&e == &other || other.removed || (!other.target && !other.reserved_target))
            continue;
        if (other.target == &target || other.reserved_target == &target)
            return std::unexpected(ControllerError::TargetBusy);
        auto *other_local = dynamic_cast<LocalPlayerController *>(other.controller.get());
        if (local && other_local && (other.bound_scene == token || other.reserved_scene == token) &&
            local->player() == other_local->player())
            return std::unexpected(ControllerError::PlayerBusy);
    }
    return {};
}
ControllerOperation ControllerManager::bind(ControllerHandle h, SceneControlToken token,
                                            elysia::core::GameObject &target)
{
    auto r = std::make_shared<Request>();
    r->kind = RequestKind::Bind;
    r->handle = h;
    r->scene = token;
    r->target = &target;
    return request(std::move(r));
}
void ControllerManager::release_reservation(const Request &r)
{
    if (r.kind == RequestKind::Map)
        return;
    if (auto *e = find(r.handle); e && e->reserved_target == r.target && e->reserved_scene == r.scene)
    {
        e->reserved_target = nullptr;
        e->reserved_scene = {};
    }
}
void ControllerManager::fail_requests(ControllerHandle handle, ControllerError error)
{
    for (const auto &r : _requests)
        if (r->handle == handle && r->operation.pending())
        {
            r->operation.finish(error);
            release_reservation(*r);
        }
}
ControllerOperation ControllerManager::request(std::shared_ptr<Request> r)
{
    return dispatch(elysia::scene::SceneBoundary::Update, [&] {
        auto *e = find(r->handle);
        auto reject = [&](ControllerError error) {
            r->operation.finish(error);
            return r->operation;
        };
        if (!e)
            return reject(ControllerError::InvalidHandle);
        if (r->kind == RequestKind::Bind)
        {
            auto result = validate_binding(*e, r->scene, *r->target);
            if (!result)
                return reject(result.error());
        }
        if (r->kind == RequestKind::Map)
        {
            auto *local = dynamic_cast<LocalPlayerController *>(e->controller.get());
            if (!local || !r->map.valid())
                return reject(ControllerError::InvalidMap);
            if (auto *ctx = context(e->bound_scene))
                if (!ctx->_scene.local_players().accepts_keyboard_map(local->player(),
                                                                      r->map.keyboard_controls()))
                    return reject(ControllerError::InvalidMap);
            r->scene = e->bound_scene;
        }
        else if (r->kind == RequestKind::Unbind)
            r->scene = e->bound_scene;
        // Prepare queue capacity before changing existing results or reservations.
        _requests.reserve(_requests.size() + 1);
        _pending.push_back(r);
        if (r->kind != RequestKind::Map)
        {
            // Only accepted requests supersede older target operations.
            for (const auto &old : _requests)
                if (old->handle == r->handle && old->kind != RequestKind::Map && old->operation.pending())
                {
                    old->operation.finish(ControllerError::Superseded);
                    release_reservation(*old);
                }
            e->reserved_target = r->target;
            e->reserved_scene = r->scene;
        }
        _requests.push_back(r);
        return r->operation;
    });
}
void ControllerManager::commit(const std::shared_ptr<Request> &r)
{
    auto fail = [&](ControllerError error) {
        r->operation.finish(error);
        release_reservation(*r);
    };
    auto *e = find(r->handle);
    if (!e)
    {
        fail(ControllerError::InvalidHandle);
        return;
    }
    if (r->kind == RequestKind::Unbind)
    {
        if (e->target)
            detach(*e, InputCancelReason::Unbound);
        release_reservation(*r);
        r->operation.finish();
        return;
    }
    if (r->kind == RequestKind::Map)
    {
        auto *local = dynamic_cast<LocalPlayerController *>(e->controller.get());
        auto valid = [&] {
            auto *ctx = context(e->bound_scene);
            return !ctx || ctx->_scene.local_players().accepts_keyboard_map(local->player(),
                                                                            r->map.keyboard_controls());
        };
        if (!valid())
        {
            fail(ControllerError::InvalidMap);
            return;
        }
        cancel(*e, InputCancelReason::SourceChanged);
        if (!r->operation.pending())
            return;
        if (!valid())
        {
            fail(ControllerError::InvalidMap);
            return;
        }
        r->map.reset_state();
        local->_map = std::move(r->map);
        if (auto *ctx = context(e->bound_scene))
        {
            ctx->_scene.local_players().set_keyboard_requirement(local->player(),
                                                                 local->_map.keyboard_controls());
            ctx->_scene.input_router().suppress(local->player());
        }
        r->operation.finish();
        return;
    }
    auto validate = [&]() -> std::expected<void, ControllerError> {
        auto *ctx = context(r->scene);
        if (!ctx)
            return std::unexpected(ControllerError::InvalidContext);
        if (!ctx->_scene.contains_control_target(r->target))
            return std::unexpected(ControllerError::InvalidTarget);
        return validate_binding(*e, r->scene, *r->target);
    };
    auto valid = validate();
    if (!valid)
    {
        fail(valid.error());
        return;
    }
    if (e->target == r->target && e->bound_scene == r->scene)
    {
        release_reservation(*r);
        r->operation.finish();
        return;
    }
    detach(*e, InputCancelReason::Unbound);
    if (!r->operation.pending())
        return;
    valid = validate();
    if (!valid)
    {
        fail(valid.error());
        return;
    }
    e->target = r->target;
    e->receiver = dynamic_cast<ControlCommandReceiver *>(r->target);
    e->bound_scene = r->scene;
    e->available = r->target->is_active();
    if (auto *local = dynamic_cast<LocalPlayerController *>(e->controller.get()))
    {
        auto &scene = context(r->scene)->_scene;
        scene.local_players().set_keyboard_requirement(local->player(), local->_map.keyboard_controls());
        local->_sources = scene.local_players().sources(local->player());
        local->_binding_version = scene.local_players().binding_version(local->player());
        scene.input_router().suppress(local->player());
    }
    release_reservation(*r);
    r->operation.finish();
}
void ControllerManager::ingest(Entry &e, ActionInputResult input)
{
    if (!_session || e.command.controller.runtime != _runtime || !e.target || !e.available || e.removed || e.cancelling ||
        (e.producing_generation && *e.producing_generation != e.cancel_generation))
        return;
    auto *ctx = context(e.bound_scene);
    if (!ctx || !ctx->_active || ctx->_scene._paused || !e.target->is_active() || e.target->is_destroyed())
        return;
    bool finite = input.frame.finite();
    for (auto &[id, v] : input.deltas)
        finite = finite && std::isfinite(v.x) && std::isfinite(v.y);
    for (auto &event : input.events)
        finite = finite && std::isfinite(event.value.x) && std::isfinite(event.value.y) &&
                 std::isfinite(event.previous_value.x) && std::isfinite(event.previous_value.y);
    if (!finite || e.command.events.size() + input.events.size() > 1024)
    {
        cancel(e, InputCancelReason::Overflow);
        if (auto *local = dynamic_cast<LocalPlayerController *>(e.controller.get()))
            ctx->_scene.input_router().suppress(local->player());
        ELYSIA_LOG_ERROR("input", "Controller input rejected: non-finite value or event queue overflow.");
        return;
    }
    e.command.state = std::move(input.frame);
    e.command.events.insert(e.command.events.end(), input.events.begin(), input.events.end());
    for (auto &[id, v] : input.deltas)
    {
        auto &sum = e.command.deltas[id];
        sum.x += v.x;
        sum.y += v.y;
        if (!std::isfinite(sum.x) || !std::isfinite(sum.y))
        {
            cancel(e, InputCancelReason::Overflow);
            return;
        }
    }
}
void ControllerManager::input(SceneControlContext &ctx, const InputSnapshot &input)
{
    dispatch(elysia::scene::SceneBoundary::Input, [&] {
        std::vector<ControllerHandle> handles;
        for (auto &[id, e] : _entries)
            if (!e.removed && e.bound_scene == ctx.token())
                handles.push_back(e.command.controller);
        for (auto h : handles)
        {
            auto *e = find(h);
            if (!e)
                continue;
            auto *local = dynamic_cast<LocalPlayerController *>(e->controller.get());
            if (!local)
                continue;
            const bool already_cancelled = std::exchange(e->source_cancelled, false);
            auto &players = ctx._scene.local_players();
            auto player = local->player();
            bool available = ctx._active && !ctx._scene._paused && e->target && e->target->is_active() &&
                             !e->target->is_destroyed() && players.contains(player);
            if (!available || !e->available)
            {
                const bool became_unavailable = e->available && !available;
                e->available = available;
                if (became_unavailable && !ctx._scene._paused)
                    cancel(*e, InputCancelReason::Unavailable);
                ctx._scene.input_router().suppress(player);
                continue;
            }
            auto sources = players.sources(player);
            if (sources != local->_sources || players.binding_version(player) != local->_binding_version)
            {
                bool added = sources == local->_sources || std::ranges::any_of(sources, [&](auto source) {
                                 return std::ranges::find(local->_sources, source) == local->_sources.end();
                             });
                local->_sources = sources;
                local->_binding_version = players.binding_version(player);
                if (!already_cancelled)
                    cancel(*e, InputCancelReason::SourceChanged);
                if (added)
                {
                    ctx._scene.input_router().suppress(player);
                    continue;
                }
            }
            // Consumption also discards matching deltas queued during earlier zero-tick frames.
            // Do not discard unrelated buttons or wheel/motion actions from the same player.
            std::erase_if(e->command.deltas, [&](const auto &pending) {
                for (const auto &operation : ctx._scene.input_router().consumed_operations())
                {
                    if (!players.owns(player, operation))
                        continue;
                    for (const auto &binding : local->_map.bindings(pending.first))
                    {
                        const auto *delta = std::get_if<PointerDeltaBinding>(&binding.source);
                        if (!delta)
                            continue;
                        const bool motion =
                            delta->axis == PointerDeltaAxis::MouseX || delta->axis == PointerDeltaAxis::MouseY;
                        if ((motion && operation.type == RawInputEventType::MouseMoved) ||
                            (!motion && operation.type == RawInputEventType::MouseWheel))
                            return true;
                    }
                }
                return false;
            });
            std::erase_if(e->command.events, [&](const auto &pending) {
                for (const auto &operation : ctx._scene.input_router().consumed_operations())
                {
                    if (!players.owns(player, operation))
                        continue;
                    for (const auto &binding : local->_map.bindings(pending.action))
                    {
                        if (auto *button = std::get_if<ButtonInputBinding>(&binding.source);
                            button && button->control == operation.control &&
                            operation.control != RawInputControl::None)
                            return true;
                        if (auto *buttons = std::get_if<Button2DInputBinding>(&binding.source);
                            buttons && operation.control != RawInputControl::None)
                            for (auto key : {buttons->left, buttons->right, buttons->up, buttons->down})
                                if (key == operation.control)
                                    return true;
                        if (auto *axis = std::get_if<AxisInputBinding>(&binding.source);
                            axis && axis->axis == operation.axis && operation.axis != RawInputAxis::None)
                            return true;
                        if (auto *axes = std::get_if<Axis2DInputBinding>(&binding.source);
                            axes && operation.axis != RawInputAxis::None &&
                            (axes->x_axis == operation.axis || axes->y_axis == operation.axis))
                            return true;
                    }
                }
                return false;
            });
            auto filtered = players.for_player(player, input);
            // A consumed position invalidates older samples, including those buffered before a tick.
            // Only a later routed position in this snapshot may restore it.
            std::uint64_t consumed_position = 0;
            for (const auto &operation : ctx._scene.input_router().consumed_operations())
                if (players.owns(player, operation) && has_pointer_position(operation))
                {
                    consumed_position = std::max(consumed_position, operation.routing_id);
                    e->command.pointer_position.reset();
                    e->command.pointer_updated = false;
                }
            for (const auto &event : filtered.events)
            {
                if (event.device != InputDevice::Unknown)
                    e->command.input_device = event.device;
                if (has_pointer_position(event) && event.routing_id > consumed_position)
                {
                    e->command.pointer_position = {static_cast<float>(event.mouse_x),
                                                   static_cast<float>(event.mouse_y)};
                    e->command.pointer_updated = true;
                }
            }
            e->producing_generation = e->cancel_generation;
            struct Guard
            {
                Entry& entry;
                ~Guard() noexcept { entry.producing_generation.reset(); }
            } guard{*e};
            local->process_input(filtered);
        }
    });
}
void ControllerManager::cancel_local(SceneControlContext &ctx, LocalPlayerId player, InputCancelReason reason)
{
    dispatch(elysia::scene::SceneBoundary::Input, [&] {
        elysia::scene::detail::SceneFailureCollector failures;
        for (auto &[id, e] : _entries)
            if (!e.removed && e.bound_scene == ctx.token())
                if (auto *local = dynamic_cast<LocalPlayerController *>(e.controller.get());
                    local && local->player() == player)
                {
                    e.source_cancelled = true;
                    const auto &players = ctx._scene.local_players();
                    const auto effective_reason =
                        reason == InputCancelReason::Suppressed &&
                                (players.sources(player) != local->_sources ||
                                 players.binding_version(player) != local->_binding_version)
                            ? InputCancelReason::SourceChanged
                            : reason;
                    failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Input,
                        "Local controller cancellation", [&] { cancel(e, effective_reason); });
                }
        failures.rethrow_if_failed();
    });
}
void ControllerManager::pause(SceneControlContext &ctx)
{
    dispatch(elysia::scene::SceneBoundary::Update, [&] {
        elysia::scene::detail::SceneFailureCollector failures;
        for (auto &[id, e] : _entries)
            if (!e.removed && e.bound_scene == ctx.token())
                failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Update,
                    "Paused controller cancellation", [&] { cancel(e, InputCancelReason::Paused); });
        failures.rethrow_if_failed();
    });
}
void ControllerManager::leave(SceneControlContext &ctx, bool release)
{
    dispatch(elysia::scene::SceneBoundary::Detach, [&] {
        std::vector<Entry*> retiring;
        for (auto& [id, entry] : _entries)
            if (!entry.removed && (entry.bound_scene == ctx.token() ||
                (release && entry.scope == ControllerScope::Scene && entry.owner == ctx.token())))
                retiring.push_back(&entry);
        for (const auto &r : _requests)
            if (r->scene == ctx.token() && r->operation.pending())
            {
                r->operation.finish(ControllerError::InvalidContext);
                release_reservation(*r);
            }
        for (Entry* entry : retiring)
        {
            Entry& e = *entry;
            e.available = false;
            if (release && e.scope == ControllerScope::Scene && e.owner == ctx.token())
            {
                fail_requests(e.command.controller, ControllerError::InvalidContext);
                e.removed = true;
            }
        }
        elysia::scene::detail::SceneFailureCollector failures;
        for (Entry* entry : retiring)
            failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Detach,
                "Leaving controller cancellation", [&] { detach(*entry, InputCancelReason::Unbound); });
        failures.rethrow_if_failed();
    });
}
void ControllerManager::object_removed(SceneControlContext &ctx, elysia::core::GameObject &target)
{
    dispatch(elysia::scene::SceneBoundary::ObjectRemoval, [&] {
        elysia::scene::detail::SceneFailureCollector failures;
        for (const auto &r : _requests)
            if (r->scene == ctx.token() && r->target == &target && r->operation.pending())
            {
                r->operation.finish(ControllerError::InvalidTarget);
                release_reservation(*r);
            }
        for (auto &[id, e] : _entries)
            if (!e.removed && e.bound_scene == ctx.token() && e.target == &target)
                failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::ObjectRemoval,
                    "Removed control target", [&] { detach(e, InputCancelReason::Unbound); });
        failures.rethrow_if_failed();
    });
}
void ControllerManager::advance(SceneControlContext &ctx, std::uint64_t tick, double delta)
{
    if (!_session || !ctx._active || ctx._scene._paused)
        return;
    dispatch(elysia::scene::SceneBoundary::Update, [&] {
        ++_dispatch;
        std::vector<ControllerHandle> handles;
        for (auto &[id, e] : _entries)
            if (!e.removed && e.bound_scene == ctx.token() && e.eligible_dispatch <= _dispatch)
                handles.push_back(e.command.controller);
        for (auto h : handles)
        {
            if (!_session || !ctx._active || ctx._scene._paused)
                break;
            auto *e = find(h);
            if (!e)
                continue;
            if (!e->target || e->target->is_destroyed() || !e->target->is_active())
            {
                if (e->available)
                    cancel(*e, InputCancelReason::Unavailable);
                e->available = false;
                continue;
            }
            if (!e->available && dynamic_cast<LocalPlayerController *>(e->controller.get()))
                continue;
            if (auto *local = dynamic_cast<LocalPlayerController *>(e->controller.get()))
            {
                auto &players = ctx._scene.local_players();
                auto sources = players.sources(local->player());
                if (!players.contains(local->player()) || sources != local->_sources ||
                    players.binding_version(local->player()) != local->_binding_version)
                {
                    if (!players.contains(local->player()))
                        e->available = false;
                    local->_sources = std::move(sources);
                    local->_binding_version = players.binding_version(local->player());
                    cancel(*e, InputCancelReason::SourceChanged);
                    ctx._scene.input_router().suppress(local->player());
                    continue;
                }
            }
            e->available = true;
            e->producing_generation = e->cancel_generation;
            struct Guard
            {
                Entry& entry;
                ~Guard() noexcept { entry.producing_generation.reset(); }
            } guard{*e};
            e->controller->produce_intent(tick, delta);
            const bool cancelled_during_production = *e->producing_generation != e->cancel_generation;
            e->producing_generation.reset();
            if (cancelled_during_production)
                continue;
            if (!ctx._active || ctx._scene._paused || e->removed || !e->receiver || !e->target->is_active() ||
                e->target->is_destroyed())
                continue;
            e->command.tick = tick;
            ++e->command.sequence;
            auto command = e->command;
            e->command.events.clear();
            e->command.deltas.clear();
            e->command.state.clear_edges();
            e->command.pointer_updated = false;
            e->receiver->on_control_command(command, delta);
        }
    });
}
ControllerOperation ControllerManager::replace_map(ControllerHandle h, InputActionMap map)
{
    auto r = std::make_shared<Request>();
    r->kind = RequestKind::Map;
    r->handle = h;
    r->map = std::move(map);
    return request(std::move(r));
}
void ControllerManager::close_context_noexcept(SceneControlContext& ctx) noexcept
{
    // Destructor fallback: sever references without invoking user callbacks or allocating.
    Boundary boundary(*this);
    for (const auto& request : _requests)
        if (request->scene.instance == ctx._instance && request->operation.pending())
        {
            request->operation.finish(ControllerError::InvalidContext);
            release_reservation(*request);
        }
    for (auto& [id, entry] : _entries)
    {
        if (entry.bound_scene.instance == ctx._instance)
        {
            if (auto* local = dynamic_cast<LocalPlayerController*>(entry.controller.get()))
                ctx._scene.local_players().clear_keyboard_requirement(local->player());
            entry.target = nullptr;
            entry.receiver = nullptr;
            entry.bound_scene = {};
            entry.available = false;
            ++entry.command.binding_generation;
            ++entry.cancel_generation;
            entry.command.state = {};
            entry.command.events.clear();
            entry.command.deltas.clear();
            entry.command.pointer_position.reset();
            entry.command.pointer_updated = false;
            entry.command.input_device = InputDevice::Unknown;
            if (auto* local = dynamic_cast<LocalPlayerController*>(entry.controller.get()))
                local->_map.reset_state();
        }
        if (entry.reserved_scene.instance == ctx._instance)
        {
            entry.reserved_target = nullptr;
            entry.reserved_scene = {};
        }
        if (entry.scope == ControllerScope::Scene && entry.owner.instance == ctx._instance)
        {
            fail_requests(entry.command.controller, ControllerError::InvalidContext);
            entry.removed = true;
            entry.reserved_target = nullptr;
            entry.reserved_scene = {};
        }
    }
    if (_depth == 1 && !_flushing)
    {
        std::erase_if(_requests, [](const auto& request) { return !request->operation.pending(); });
        std::erase_if(_entries, [](const auto& item) { return item.second.removed; });
    }
}
} // namespace elysia::gameplay
