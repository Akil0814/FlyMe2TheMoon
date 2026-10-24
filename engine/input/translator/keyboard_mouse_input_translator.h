#pragma once

#include "input_translator.h"

#include <cstdint>
#include <optional>

namespace elysia::input
{
class KeyboardMouseInputTranslator : public InputTranslator
{
public:
    std::vector<RawInputEvent> translate_event(const SDL_Event& event) override;

private:
    std::optional<RawInputControl> control_from_key(SDL_Keycode key) const;
    std::optional<RawInputControl> control_from_mouse_button(std::uint8_t button) const;
};

}
