#pragma once

#include "../raw_input_types.h"

#include <SDL3/SDL.h>
#include <vector>

namespace elysia::input
{
class InputTranslator
{
public:
    virtual ~InputTranslator() = default;
    virtual std::vector<RawInputEvent> translate_event(const SDL_Event& event) = 0;

protected:
    RawInputEventType input_event_type(bool pressed) const;
    void append_event(
        std::vector<RawInputEvent>& events,
        RawInputControl control,
        RawInputEventType type,
        InputDevice device
    ) const;
};

}
