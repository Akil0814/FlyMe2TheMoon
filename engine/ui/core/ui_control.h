#pragma once

#include "ui_element.h"
#include "ui_focusable.h"

namespace elysia::ui
{
class UiControl : public UiElement, public UiFocusable
{
public:
    explicit UiControl(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0);
    UiControl(elysia::core::Vector2 position,elysia::core::Vector2 size,int order = 0);
    UiControl(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0);

    void reset() noexcept override;

    // Enables or suppresses interaction without removing the control from layout.
    virtual void set_enabled(bool enabled);
    [[nodiscard]] bool is_enabled() const;

    // Updates the control's focus state so widgets can refresh visuals or input mode.
    void set_focused(bool focused) override;
    [[nodiscard]] bool is_focused() const override;

protected:
    bool _enabled = true;
    bool _is_focused = false;
};

}
