#include "scene_input_router.h"
#include "../scene.h"
#include "../detail/scene_failure_boundary.h"
#include <algorithm>
#include <exception>
namespace elysia::scene
{
using namespace elysia::input;
InputCapture SceneInputRouter::gameplay_capture(InputSourceId source, InputCapture external, bool focus_lost,
                                                InputCapture additional) const
{
    if (_scene._paused || _block_gameplay || focus_lost)
        return AllInputCapture;
    return external | (ui_source(source) ? (ui_capture() | additional) : InputCapture::None);
}
void SceneInputRouter::cancel(LocalPlayerId player, InputCancelReason reason)
{
    if (!_routing)
    {
        if (_cancel)
            _cancel(player, reason);
        return;
    }
    auto priority = [](InputCancelReason r) {
        return r == InputCancelReason::FocusLost ? 3 : r == InputCancelReason::SourceChanged ? 2 : 1;
    };
    auto [it, inserted] = _pending_cancellations.try_emplace(player, reason);
    if (!inserted && priority(reason) > priority(it->second))
        it->second = reason;
}
void SceneInputRouter::flush_cancellations()
{
    auto pending = std::exchange(_pending_cancellations, {});
    elysia::scene::detail::SceneFailureCollector failures;
    for (auto [player, reason] : pending)
        if (_cancel)
            failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Input,
                "Player input cancellation", [&] { _cancel(player, reason); });
    failures.rethrow_if_failed();
}
InputCapture SceneInputRouter::ui_capture() const
{
    auto mask = InputCapture::None;
    for (const auto &root : _scene._ui_roots)
        if (root && root->is_visible() && root->is_active() && !root->is_destroyed())
            mask = mask | root->input_capture();
    return mask;
}
void SceneInputRouter::suppress(LocalPlayerId player)
{
    for (const auto &source : _last_input.sources)
        if (_scene._players->owns(player, source.source))
            _suppression[source.source].block(
                source.source.is_keyboard() ? _scene._players->filter_keyboard(player, source.frame.state)
                                            : source.frame.state,
                AllInputCapture);
}
bool SceneInputRouter::ui_source(InputSourceId source) const
{
    return source.is_keyboard() || source.is_mouse() || (source.is_gamepad() && source == ui_gamepad());
}
bool SceneInputRouter::ui_enabled(InputSourceId source) const
{
    return ui_source(source) && (source.is_mouse() || _mode == UiInteractionMode::Navigation ||
                                 (source.is_keyboard() && captured(ui_capture(), InputDevice::Keyboard)));
}
void SceneInputRouter::cancel_source(InputSourceId source, InputCancelReason reason)
{
    const auto players = _scene._players->players();
    elysia::scene::detail::SceneFailureCollector failures;
    for (auto player : players)
        if (_scene._players->owns(player, source))
            failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Input,
                "Source input cancellation", [&] { cancel(player, reason); });
    failures.rethrow_if_failed();
}
void SceneInputRouter::reset_ui_interaction()
{
    _ui_input_router.reset_transient_state();
    elysia::scene::detail::SceneFailureCollector failures;
    failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Input,
        "UI interaction cancellation", [this] { cancel_ui_interactions(); });
    failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Input,
        "UI input reset", [this] { dispatch_ui_frame({}); });
    failures.rethrow_if_failed();
}
void SceneInputRouter::cancel_ui_interactions()
{
    using Lifetime = elysia::ui::UiElement::CancellationLifetime;
    std::vector<std::shared_ptr<Lifetime>> snapshot;
    for (const auto& root : _scene._ui_roots)
        if (root)
        {
            if (!root->_cancellation_lifetime)
                root->_cancellation_lifetime = std::make_shared<Lifetime>(root.get());
            snapshot.push_back(root->_cancellation_lifetime);
        }
    elysia::scene::detail::SceneFailureCollector failures;
    for (const auto& lifetime : snapshot)
        if (auto* root = lifetime->element; root && _scene.contains_object_address(root))
            failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Input,
                "UI root cancellation", [&] { root->cancel_input_interaction(); });
    failures.rethrow_if_failed();
}
void SceneInputRouter::set_ui_gamepad(InputSourceId source)
{
    if ((source.value && !source.is_gamepad()) || source == ui_gamepad())
        return;
    elysia::scene::detail::SceneFailureCollector failures;
    for (auto id : {ui_gamepad(), source})
        if (const auto *state = _last_input.find(id))
        {
            _ui_suppression[id].block(state->frame.state, AllInputCapture);
            _suppression[id].block(state->frame.state, AllInputCapture);
            failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Input,
                "UI gamepad cancellation", [&] { cancel_source(id); });
        }
    _ui_access->gamepad = source;
    failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Input,
        "UI gamepad reset", [this] { reset_ui_interaction(); });
    failures.rethrow_if_failed();
}
void SceneInputRouter::set_ui_interaction_mode(UiInteractionMode mode)
{
    if (_mode == mode)
        return;
    _mode = mode;
    for (const auto &source : _last_input.sources)
        if (ui_source(source.source))
            _ui_suppression[source.source].block(source.frame.state, AllInputCapture);
    reset_ui_interaction();
}
void SceneInputRouter::set_all_gameplay_input_blocked(bool blocked)
{
    if (blocked == _block_gameplay)
        return;
    const auto players = _scene._players->players();
    _block_gameplay = blocked;
    elysia::scene::detail::SceneFailureCollector failures;
    for (auto player : players)
    {
        failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Input,
            "Player input suppression", [&] { suppress(player); });
        failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Input,
            "Player input cancellation", [&] { cancel(player); });
    }
    failures.rethrow_if_failed();
}
void SceneInputRouter::reset()
{
    elysia::scene::detail::SceneFailureCollector failures;
    for (auto player : _scene._players->players())
        failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Input,
            "Player input suppression", [&] { suppress(player); });
    _await_initial_input = true;
    _last_ui_state = {};
    _ui_input_router.reset_transient_state();
    _consumed.clear();
    if (!_routing)
        _pending_cancellations.clear();
    _block_gameplay = false;
    failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Input,
        "UI interaction cancellation", [this] { cancel_ui_interactions(); });
    failures.rethrow_if_failed();
}
void SceneInputRouter::consume_input(const RawInputEvent &event)
{
    _consumed.push_back(event);
    _suppression[event.source].block(event);
}
void SceneInputRouter::route(const InputSnapshot &snapshot)
{
    _routing = true;
    struct Guard
    {
        bool &routing;
        ~Guard()
        {
            routing = false;
        }
    } guard{_routing};
    InputSnapshot routed;
    elysia::scene::detail::SceneFailureCollector failures;
    failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Input,
        "Scene input routing", [&] {
        InputSnapshot input = snapshot;
        for (std::size_t index = 0; index < input.events.size(); ++index)
            input.events[index].routing_id = index + 1;
        auto update_capture = [&](InputSourceId source, InputCapture mask) {
            mask = mask & (source.is_keyboard() ? InputCapture::Keyboard
                           : source.is_mouse()  ? InputCapture::Pointer
                                                : InputCapture::Gamepad);
            auto &previous = _previous_capture[source];
            if (!_scene._paused && !_block_gameplay && previous != mask && mask != InputCapture::None)
                cancel_source(source);
            previous = mask;
        };
        _last_input = input;
        _consumed.clear();
        elysia::scene::detail::SceneFailureCollector removal_failures;
        for (auto source : input.removed)
        {
            const auto player = _scene._players->owner(source);
            if (player.value)
                cancel(player, InputCancelReason::SourceChanged);
            if (source == ui_gamepad())
                removal_failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Input,
                    "Removed UI gamepad reset", [this] { set_ui_gamepad({}); });
            (void)_scene._players->unbind_source(source);
            _previous_capture.erase(source);
            _suppression.erase(source);
            _ui_suppression.erase(source);
        }
        removal_failures.rethrow_if_failed();
        if (input.focus_lost)
        {
            for (auto player : _scene._players->players())
            {
                suppress(player);
                cancel(player, InputCancelReason::FocusLost);
            }
            for (const auto &source : input.sources)
                _ui_suppression[source.source].block(source.frame.state, AllInputCapture);
            reset_ui_interaction();
        }
        std::vector<InputSourceId> ui_sources{InputSourceId::keyboard(), InputSourceId::mouse()};
        if (ui_gamepad().value)
            ui_sources.push_back(ui_gamepad());
        if (_await_initial_input || ui_sources != _previous_ui_sources)
        {
            _ui_input_router.reset_transient_state();
            cancel_ui_interactions();
            for (const auto &source : input.sources)
            {
                if (_await_initial_input)
                    _suppression[source.source].block(source.initial_state, AllInputCapture);
                if (ui_source(source.source))
                    _ui_suppression[source.source].block(source.initial_state, AllInputCapture);
            }
            _previous_ui_sources = ui_sources;
            _await_initial_input = false;
        }
        routed = input;
        routed.events.clear();
        std::map<InputSourceId, RawInputState> physical;
        RawInputFrame ui_frame;
        for (auto &source : routed.sources)
        {
            physical[source.source] = source.initial_state;
            auto mask = gameplay_capture(source.source, input.capture, input.focus_lost);
            update_capture(source.source, mask);
            _suppression[source.source].observe(source.initial_state, mask);
            _ui_suppression[source.source].observe(source.initial_state, input.capture);
            source.initial_state = _suppression[source.source].filter(source.initial_state, mask);
            if (ui_enabled(source.source))
            {
                auto preview = _ui_suppression[source.source];
                auto preview_state = physical[source.source];
                for (const auto &event : input.events)
                    if (event.source == source.source)
                    {
                        apply_raw_event(preview_state, event);
                        preview.observe(preview_state, input.capture);
                    }
                preview.observe(source.frame.state, input.capture);
                auto state = preview.filter(source.frame.state, input.capture);
                merge_raw_state(ui_frame.state, state);

                if (source.source.is_mouse())
                {
                    ui_frame.mouse_x = source.frame.mouse_x;
                    ui_frame.mouse_y = source.frame.mouse_y;
                }
            }
        }
        const auto previous_ui_device = _ui_active_device;
        for (const auto &event : input.events)
            if (ui_enabled(event.source) && !captured(input.capture, event.device))
                _ui_active_device = event.device;
        ui_frame.active_device = _ui_active_device;
        ui_frame.device_switched_this_frame = previous_ui_device != _ui_active_device;
        dispatch_ui_frame(_ui_input_router.route_frame(ui_frame));
        std::vector<RawInputEvent> shortcut_events;
        for (const auto &event : input.events)
        {
            auto &state = physical[event.source];
            apply_raw_event(state, event);
            auto before = ui_capture();
            bool consumed =
                std::ranges::any_of(_consumed, [&](const auto &e) { return e.routing_id == event.routing_id; });
            _ui_suppression[event.source].observe(state, input.capture);
            const auto ui_state = _ui_suppression[event.source].filter(state, input.capture);
            bool suppressed =
                (event.type == RawInputEventType::ControlPressed && !ui_state.is_pressed(event.control)) ||
                (event.type == RawInputEventType::AxisChanged &&
                 ui_state.axis_value(event.axis) != event.axis_value);
            if (!consumed && _auto_claim_ui_gamepad && !ui_gamepad().value && event.source.is_gamepad() &&
                event.type == RawInputEventType::ControlPressed && !captured(input.capture, event.device) &&
                !input.focus_lost)
            {
                set_ui_gamepad(event.source);
                consumed = true;
            }
            if (!consumed && ui_enabled(event.source) && !captured(input.capture, event.device) && !suppressed &&
                !input.focus_lost)
                consumed = _scene.dispatch_ui_events(_ui_input_router.route_event(event));
            auto mask = input.capture | (ui_source(event.source) ? before | ui_capture() : InputCapture::None);
            if (!consumed && !captured(mask, event.device) && !input.focus_lost && event.source.is_gamepad() &&
                !_scene._players->owner(event.source).value)
                consumed = _scene.on_unassigned_input(event);
            if (!consumed && ui_source(event.source) && captured(_shortcut_devices, event.device) &&
                !captured(mask, event.device) && !suppressed && !input.focus_lost)
                shortcut_events.push_back(event);
            mask = gameplay_capture(event.source, input.capture, input.focus_lost, before);
            if (consumed)
                consume_input(event);
            update_capture(event.source, mask);
            _suppression[event.source].observe(state, mask);
            auto filtered = _suppression[event.source].filter(state, mask);
            bool allowed = !consumed && !captured(mask, event.device);
            if (event.type == RawInputEventType::ControlPressed)
                allowed = allowed && filtered.is_pressed(event.control);
            if (event.type == RawInputEventType::AxisChanged)
                allowed = allowed && filtered.axis_value(event.axis) == event.axis_value;
            if (allowed)
                routed.events.push_back(event);
            else if (captured(mask, event.device))
            {
                std::erase_if(routed.events, [&](const auto &previous) {
                    return previous.source == event.source && captured(mask, previous.device);
                });
            }
        }
        if (_mode == UiInteractionMode::Navigation &&
            _scene.dispatch_ui_events(_ui_input_router.synthesize_events(ui_frame)))
        {
            // A synthetic scroll originates from the owner's left stick; retain that
            // attribution so a custom gameplay axis cannot consume the same operation.
            for (const auto &source : input.sources)
                if (source.source.is_gamepad() && ui_source(source.source))
                {
                    for (auto axis : {RawInputAxis::GamepadLeftX, RawInputAxis::GamepadLeftY})
                    {
                        RawInputEvent operation;
                        operation.source = source.source;
                        operation.device = InputDevice::Gamepad;
                        operation.type = RawInputEventType::AxisChanged;
                        operation.axis = axis;
                        operation.axis_value = source.frame.state.axis_value(axis);
                        _suppression[source.source].block(operation);
                    }
                    cancel_source(source.source);
                    std::erase_if(routed.events, [&](const auto &event) {
                        return event.source == source.source && event.type == RawInputEventType::AxisChanged &&
                               (event.axis == RawInputAxis::GamepadLeftX ||
                                event.axis == RawInputAxis::GamepadLeftY);
                    });
                }
        }
        RawInputFrame shortcuts = ui_frame;
        shortcuts.state.clear();
        for (auto &source : routed.sources)
        {
            auto mask = gameplay_capture(source.source, input.capture, input.focus_lost);
            _suppression[source.source].observe(source.frame.state, mask);
            _ui_suppression[source.source].observe(source.frame.state, input.capture);
            source.frame.state = _suppression[source.source].filter(source.frame.state, mask);
            if (ui_source(source.source) &&
                ((source.source.is_keyboard() && captured(_shortcut_devices, InputDevice::Keyboard)) ||
                 (source.source.is_mouse() && captured(_shortcut_devices, InputDevice::Mouse)) ||
                 (source.source.is_gamepad() && captured(_shortcut_devices, InputDevice::Gamepad))))
                merge_raw_state(shortcuts.state, source.frame.state);
        }
        _scene.on_shortcuts(shortcuts, shortcut_events);
        std::erase_if(routed.events, [&](const auto &event) {
            return std::ranges::any_of(_consumed,
                                       [&](const auto &e) { return e.routing_id == event.routing_id; });
        });
        for (auto &source : routed.sources)
        {
            auto mask = gameplay_capture(source.source, input.capture, input.focus_lost);
            // Always observe physical state, never feed already filtered zeros back into a latch.
            if (const auto *physical_source = input.find(source.source))
            {
                _suppression[source.source].observe(physical_source->frame.state, mask);
                source.frame.state = _suppression[source.source].filter(physical_source->frame.state, mask);
            }
            update_capture(source.source, mask);
            if (mask != InputCapture::None)
            {
                std::erase_if(routed.events, [&](const auto &event) {
                    return event.source == source.source && captured(mask, event.device);
                });
            }
            if (_scene._paused || _block_gameplay)
            {
                source.frame.state.clear();
                source.initial_state.clear();
            }
        }
        if (_scene._paused || _block_gameplay)
            routed.events.clear();
    });
    _routing = false;
    failures.attempt(elysia::scene::SceneKeys::Invalid, elysia::scene::SceneBoundary::Input,
        "Pending input cancellation", [this] { flush_cancellations(); });
    failures.rethrow_if_failed();
    _scene.on_routed_input(routed);
}

void SceneInputRouter::dispatch_ui_frame(const elysia::ui::UiInputFrame &input)
{
    auto frame = input;
    for (int index = 1; index < int(elysia::ui::UiAction::Count); ++index)
    {
        const auto action = static_cast<elysia::ui::UiAction>(index);
        const bool current = input.state.is_pressed(action), previous = _last_ui_state.is_pressed(action);
        frame.state.set(action, current, current && !previous, !current && previous);
    }
    _last_ui_state = frame.state;

    _scene.dispatch_ui_frame(frame);
}
} // namespace elysia::scene
