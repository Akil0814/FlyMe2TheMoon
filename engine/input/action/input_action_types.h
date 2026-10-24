#pragma once

#include "../../core/geometry/vector2.h"
#include "../raw_input_types.h"

#include <functional>
#include <string>
#include <string_view>
#include <variant>

namespace elysia::input
{
class InputActionId
{
  public:
    InputActionId() = default;
    explicit InputActionId(std::string_view value);

    [[nodiscard]] const std::string &value() const noexcept
    {
        return _value;
    }
    [[nodiscard]] bool valid() const noexcept
    {
        return !_value.empty();
    }

    friend bool operator==(const InputActionId &, const InputActionId &) = default;

  private:
    std::string _value;
};

struct InputActionIdHash
{
    [[nodiscard]] std::size_t operator()(const InputActionId &id) const noexcept
    {
        return std::hash<std::string>{}(id.value());
    }
};

enum class InputActionValueType
{
    Button,
    Axis1D,
    Axis2D
};

enum class InputValueSemantics
{
    State,
    Delta
};
enum class PointerDeltaAxis
{
    MouseX,
    MouseY,
    WheelX,
    WheelY
};

struct InputActionDescriptor
{
    InputActionId id;
    InputActionValueType value_type = InputActionValueType::Button;
    float actuation_threshold = 0.5f;
    float dead_zone = 0.2f;
    InputValueSemantics semantics = InputValueSemantics::State;
};

enum class InputActionComponent
{
    X,
    Y
};

struct PointerDeltaBinding
{
    PointerDeltaAxis axis;
    InputActionComponent component = InputActionComponent::X;
    float scale = 1;
};

struct ButtonInputBinding
{
    RawInputControl control = RawInputControl::None;
    InputActionComponent component = InputActionComponent::X;
    float scale = 1.0f;
};

struct AxisInputBinding
{
    RawInputAxis axis = RawInputAxis::None;
    InputActionComponent component = InputActionComponent::X;
    float scale = 1.0f;
};

struct Axis2DInputBinding
{
    RawInputAxis x_axis = RawInputAxis::None;
    RawInputAxis y_axis = RawInputAxis::None;
    float x_scale = 1.0f;
    float y_scale = 1.0f;
};

struct Button2DInputBinding
{
    RawInputControl left = RawInputControl::None;
    RawInputControl right = RawInputControl::None;
    RawInputControl up = RawInputControl::None;
    RawInputControl down = RawInputControl::None;
};

using InputBindingSource = std::variant<ButtonInputBinding, AxisInputBinding, Axis2DInputBinding,
                                        Button2DInputBinding, PointerDeltaBinding>;

struct InputBinding
{
    InputActionId action;
    InputBindingSource source;
};

struct InputActionValue
{
    float x = 0.0f;
    float y = 0.0f;

    [[nodiscard]] elysia::core::Vector2 vector2() const noexcept
    {
        return {x, y};
    }
};

enum class ActionInputPhase
{
    Started,
    Changed,
    Canceled
};

struct ActionInputEvent
{
    InputActionId action;
    InputActionValueType value_type = InputActionValueType::Button;
    ActionInputPhase phase = ActionInputPhase::Changed;
    InputActionValue value;
    InputActionValue previous_value;
};
} // namespace elysia::input
