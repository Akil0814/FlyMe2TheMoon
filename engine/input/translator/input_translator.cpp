#include "input_translator.h"

namespace elysia::input
{
RawInputEventType InputTranslator::input_event_type(bool pressed) const
{
    return pressed ? RawInputEventType::ControlPressed : RawInputEventType::ControlReleased;
}

void InputTranslator::append_event(
    std::vector<RawInputEvent>& events,
    RawInputControl control,
    RawInputEventType type,
    InputDevice device
) const
{
    RawInputEvent event;
    event.control = control;
    event.axis = RawInputAxis::None;
    event.type = type;
    event.device = device;
    events.push_back(event);
}

}
