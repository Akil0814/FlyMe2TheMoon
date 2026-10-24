#pragma once

#include "../ui_input_types.h"

namespace elysia::ui
{
class UiInputEventReceiver
{
public:
    virtual ~UiInputEventReceiver() = default;
    // Consumes a discrete UI event and returns true when propagation should stop.
    virtual bool on_ui_input_event(const UiInputEvent& event) = 0;
};

}
