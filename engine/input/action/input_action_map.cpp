#include "input_action_map.h"
#include <map>

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace elysia::input
{
namespace
{
constexpr float kValueEpsilon = 0.001f;

float clamp_axis(float value)
{
    return std::clamp(value, -1.0f, 1.0f);
}

bool value_is_zero(const InputActionValue &value)
{
    return std::fabs(value.x) <= kValueEpsilon && std::fabs(value.y) <= kValueEpsilon;
}

bool value_changed(const InputActionValue &lhs, const InputActionValue &rhs)
{
    return std::fabs(lhs.x - rhs.x) > kValueEpsilon || std::fabs(lhs.y - rhs.y) > kValueEpsilon;
}

void add_component(InputActionValue &value, InputActionComponent component, float contribution)
{
    if (component == InputActionComponent::X)
        value.x += contribution;
    else
        value.y += contribution;
}
} // namespace

InputActionMap::Registration *InputActionMap::find(const InputActionId &action)
{
    const auto iter =
        std::find_if(_registrations.begin(), _registrations.end(),
                     [&](const Registration &registration) { return registration.descriptor.id == action; });
    return iter == _registrations.end() ? nullptr : &*iter;
}

const InputActionMap::Registration *InputActionMap::find(const InputActionId &action) const
{
    const auto iter =
        std::find_if(_registrations.begin(), _registrations.end(),
                     [&](const Registration &registration) { return registration.descriptor.id == action; });
    return iter == _registrations.end() ? nullptr : &*iter;
}

bool InputActionMap::binding_valid_for(const InputBinding &binding, const InputActionDescriptor &descriptor)
{
    if (binding.action != descriptor.id)
        return false;

    if (descriptor.semantics == InputValueSemantics::Delta &&
        !std::holds_alternative<PointerDeltaBinding>(binding.source))
        return false;
    return std::visit(
        [&](const auto &source) {
            using Source = std::decay_t<decltype(source)>;
            if constexpr (requires { source.component; })
                if (source.component != InputActionComponent::X && source.component != InputActionComponent::Y)
                    return false;
            if constexpr (std::is_same_v<Source, PointerDeltaBinding>)
            {
                return source.axis >= PointerDeltaAxis::MouseX && source.axis <= PointerDeltaAxis::WheelY &&
                       descriptor.semantics == InputValueSemantics::Delta &&
                       descriptor.value_type != InputActionValueType::Button && std::isfinite(source.scale) &&
                       (descriptor.value_type == InputActionValueType::Axis2D ||
                        source.component == InputActionComponent::X);
            }
            else if constexpr (std::is_same_v<Source, ButtonInputBinding>)
            {
                if (!std::isfinite(source.scale))
                    return false;
                if (!is_keyboard_control(source.control) && !is_mouse_button_control(source.control) &&
                    !is_gamepad_button_control(source.control))
                    return false;
                return descriptor.value_type == InputActionValueType::Axis2D ||
                       source.component == InputActionComponent::X;
            }
            else if constexpr (std::is_same_v<Source, AxisInputBinding>)
            {
                return std::isfinite(source.scale) && descriptor.value_type != InputActionValueType::Button &&
                       source.axis > RawInputAxis::None && source.axis < RawInputAxis::Count &&
                       (descriptor.value_type == InputActionValueType::Axis2D ||
                        source.component == InputActionComponent::X);
            }
            else if constexpr (std::is_same_v<Source, Axis2DInputBinding>)
            {
                return std::isfinite(source.x_scale) && std::isfinite(source.y_scale) &&
                       descriptor.value_type == InputActionValueType::Axis2D &&
                       source.x_axis > RawInputAxis::None && source.y_axis > RawInputAxis::None &&
                       source.x_axis < RawInputAxis::Count && source.y_axis < RawInputAxis::Count;
            }
            else
            {
                const auto valid_control = [](RawInputControl control) {
                    return is_keyboard_control(control) || is_mouse_button_control(control) ||
                           is_gamepad_button_control(control);
                };
                return descriptor.value_type == InputActionValueType::Axis2D && valid_control(source.left) &&
                       valid_control(source.right) && valid_control(source.up) && valid_control(source.down);
            }
        },
        binding.source);
}

bool InputActionMap::register_action(InputActionDescriptor descriptor,
                                     std::vector<InputBinding> default_bindings)
{
    if (descriptor.value_type < InputActionValueType::Button || descriptor.value_type > InputActionValueType::Axis2D ||
        (descriptor.semantics != InputValueSemantics::State && descriptor.semantics != InputValueSemantics::Delta) ||
        !std::isfinite(descriptor.actuation_threshold) || !std::isfinite(descriptor.dead_zone) ||
        (descriptor.semantics == InputValueSemantics::Delta &&
         descriptor.value_type == InputActionValueType::Button))
        return false;
    if (!descriptor.id.valid() || contains(descriptor.id) || descriptor.actuation_threshold < 0.0f ||
        descriptor.actuation_threshold > 1.0f || descriptor.dead_zone < 0.0f || descriptor.dead_zone >= 1.0f)
        return false;

    if (!std::all_of(default_bindings.begin(), default_bindings.end(),
                     [&](const InputBinding &binding) { return binding_valid_for(binding, descriptor); }))
        return false;

    _registrations.push_back(
        Registration{std::move(descriptor), default_bindings, std::move(default_bindings), {}});
    return true;
}

bool InputActionMap::add_binding(InputBinding binding)
{
    Registration *registration = find(binding.action);
    if (!registration || !binding_valid_for(binding, registration->descriptor))
        return false;
    registration->current_bindings.push_back(std::move(binding));
    return true;
}

bool InputActionMap::replace_bindings(const InputActionId &action, std::vector<InputBinding> bindings)
{
    Registration *registration = find(action);
    if (!registration || !std::all_of(bindings.begin(), bindings.end(), [&](const InputBinding &binding) {
            return binding_valid_for(binding, registration->descriptor);
        }))
        return false;
    registration->current_bindings = std::move(bindings);
    reset_state();
    return true;
}

bool InputActionMap::clear_bindings(const InputActionId &action)
{
    return replace_bindings(action, {});
}

void InputActionMap::reset_defaults()
{
    for (Registration &registration : _registrations)
        registration.current_bindings = registration.default_bindings;
    reset_state();
}

void InputActionMap::reset_state()
{
    for (Registration &registration : _registrations)
        registration.previous_value = {};
}

bool InputActionMap::contains(const InputActionId &action) const
{
    return find(action) != nullptr;
}

std::span<const InputBinding> InputActionMap::bindings(const InputActionId &action) const
{
    const Registration *registration = find(action);
    return registration ? std::span<const InputBinding>(registration->current_bindings)
                        : std::span<const InputBinding>{};
}

InputActionValue InputActionMap::resolve_value(const Registration &registration,
                                               const RawInputState &raw_state)
{
    InputActionValue value;
    for (const InputBinding &binding : registration.current_bindings)
    {
        std::visit(
            [&](const auto &source) {
                using Source = std::decay_t<decltype(source)>;
                if constexpr (std::is_same_v<Source, ButtonInputBinding>)
                {
                    if (raw_state.is_pressed(source.control))
                        add_component(value, source.component, source.scale);
                }
                else if constexpr (std::is_same_v<Source, AxisInputBinding>)
                {
                    const float raw_value = raw_state.axis_value(source.axis);
                    if (std::fabs(raw_value) > registration.descriptor.dead_zone)
                        add_component(value, source.component, raw_value * source.scale);
                }
                else if constexpr (std::is_same_v<Source, Axis2DInputBinding>)
                {
                    elysia::core::Vector2 vector{raw_state.axis_value(source.x_axis),
                                                 raw_state.axis_value(source.y_axis)};
                    const float length = vector.length();
                    if (length > registration.descriptor.dead_zone)
                    {
                        if (length > 1.0f)
                            vector /= length;
                        value.x += vector.x * source.x_scale;
                        value.y += vector.y * source.y_scale;
                    }
                }
                else if constexpr (std::is_same_v<Source, Button2DInputBinding>)
                {
                    value.x += raw_state.is_pressed(source.right) ? 1.0f : 0.0f;
                    value.x -= raw_state.is_pressed(source.left) ? 1.0f : 0.0f;
                    value.y += raw_state.is_pressed(source.down) ? 1.0f : 0.0f;
                    value.y -= raw_state.is_pressed(source.up) ? 1.0f : 0.0f;
                }
            },
            binding.source);
    }

    value.x = clamp_axis(value.x);
    value.y = registration.descriptor.value_type == InputActionValueType::Axis2D ? clamp_axis(value.y) : 0.0f;
    return value;
}

ActionInputResult InputActionMap::evaluate_state(const RawInputFrame &raw_input)
{
    ActionInputResult result;
    for (Registration &registration : _registrations)
    {
        if (registration.descriptor.semantics == InputValueSemantics::Delta)
            continue;
        const InputActionValue current = resolve_value(registration, raw_input.state);
        result.frame._states.emplace(registration.descriptor.id,
                                     ActionInputFrame::State{registration.descriptor.value_type, current,
                                                             registration.previous_value,
                                                             registration.descriptor.actuation_threshold});

        const auto active = [&](const InputActionValue &value) {
            return registration.descriptor.value_type == InputActionValueType::Button
                       ? value.x >= registration.descriptor.actuation_threshold
                       : !value_is_zero(value);
        };
        const bool previous_zero = !active(registration.previous_value);
        const bool current_zero = !active(current);
        if (previous_zero && !current_zero)
        {
            result.events.push_back({registration.descriptor.id, registration.descriptor.value_type,
                                     ActionInputPhase::Started, current, registration.previous_value});
        }
        else if (!previous_zero && current_zero)
        {
            result.events.push_back({registration.descriptor.id, registration.descriptor.value_type,
                                     ActionInputPhase::Canceled, current, registration.previous_value});
        }
        else if (!current_zero && value_changed(current, registration.previous_value))
        {
            result.events.push_back({registration.descriptor.id, registration.descriptor.value_type,
                                     ActionInputPhase::Changed, current, registration.previous_value});
        }

        registration.previous_value = current;
    }
    return result;
}
} // namespace elysia::input

elysia::input::ActionInputResult elysia::input::InputActionMap::resolve(const InputSnapshot &input)
{
    std::map<InputSourceId, RawInputState> states;
    for (const auto &source : input.sources)
        states[source.source] = source.initial_state;
    auto merged = [&] {
        RawInputFrame frame;
        for (const auto &[id, state] : states)
            merge_raw_state(frame.state, state);
        return frame;
    };
    // The initial snapshot establishes held state, not a new edge after cancellation.
    ActionInputResult result;
    result.frame = evaluate_state(merged()).frame;
    const auto initial_frame = result.frame;
    auto append = [&] {
        auto next = evaluate_state(merged());
        result.frame = std::move(next.frame);
        result.events.insert(result.events.end(), next.events.begin(), next.events.end());
    };
    for (const auto &event : input.events)
    {
        auto it = states.find(event.source);
        if (it == states.end())
            continue;
        apply_raw_event(it->second, event);
        append();
    }
    for (const auto &source : input.sources)
        states[source.source] = source.frame.state;
    // Reconciliation may reflect cancellation/capture, never synthesize gameplay release events.
    result.frame = evaluate_state(merged()).frame;
    for (auto &[id, state] : result.frame._states)
        state.previous = initial_frame.value(id);
    for (const auto &event : input.events)
        for (const auto &registration : _registrations)
        {
            if (!states.contains(event.source))
                continue;
            if (registration.descriptor.semantics != InputValueSemantics::Delta)
                continue;
            for (const auto &binding : registration.current_bindings)
                if (const auto *delta = std::get_if<PointerDeltaBinding>(&binding.source))
                {
                    float value = 0;
                    if (event.type == RawInputEventType::MouseMoved)
                    {
                        if (delta->axis == PointerDeltaAxis::MouseX)
                            value = float(event.mouse_delta_x);
                        if (delta->axis == PointerDeltaAxis::MouseY)
                            value = float(event.mouse_delta_y);
                    }
                    if (event.type == RawInputEventType::MouseWheel)
                    {
                        if (delta->axis == PointerDeltaAxis::WheelX)
                            value = float(event.wheel_x);
                        if (delta->axis == PointerDeltaAxis::WheelY)
                            value = float(event.wheel_y);
                    }
                    auto &total = result.deltas[registration.descriptor.id];
                    if (delta->component == InputActionComponent::X)
                        total.x += value * delta->scale;
                    else
                        total.y += value * delta->scale;
                }
        }
    return result;
}

bool elysia::input::InputActionMap::valid() const
{
    return std::ranges::all_of(_registrations, [](const Registration &r) {
        return r.descriptor.id.valid() && std::isfinite(r.descriptor.actuation_threshold) &&
               std::isfinite(r.descriptor.dead_zone) &&
               std::ranges::all_of(r.current_bindings,
                                   [&](const InputBinding &b) { return binding_valid_for(b, r.descriptor); });
    });
}
