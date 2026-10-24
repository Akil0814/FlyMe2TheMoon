#pragma once

#include "input_translator.h"

#include <cstdint>

namespace elysia::input
{
class GamepadInputTranslator : public InputTranslator
{
public:
    std::vector<RawInputEvent> translate_event(const SDL_Event& event) override;

private:
    void append_controller_button_events(
        std::vector<RawInputEvent>& events,
        std::uint8_t button,
        RawInputEventType type
    ) const;
    void append_axis_event(
        std::vector<RawInputEvent>& events,
        RawInputAxis axis,
        float normalized_value
    ) const;
    void append_trigger_virtual_button_event(
        std::vector<RawInputEvent>& events,
        RawInputControl control,
        bool pressed
    ) const;
    float normalize_stick_axis(std::int16_t value) const;
    float normalize_trigger_axis(std::int16_t value) const;

private:
    bool _left_trigger_pressed = false;
    bool _right_trigger_pressed = false;
};

}
