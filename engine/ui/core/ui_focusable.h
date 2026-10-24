#pragma once

#include "../input/contracts/ui_input_event_receiver.h"

namespace elysia::ui
{
class UiFocusable : public UiInputEventReceiver
{
public:
    virtual ~UiFocusable() = default;

    // Applies focus state managed by a containing focus scope or pointer policy.
    virtual void set_focused(bool focused) = 0;
    [[nodiscard]] virtual bool is_focused() const = 0;
};

}
