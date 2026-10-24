#include "ui_control.h"

namespace elysia::ui
{
UiControl::UiControl(const elysia::core::Rect& rect,int order)
    : UiElement(rect, order){}

UiControl::UiControl(elysia::core::Vector2 position,elysia::core::Vector2 size,int order)
    : UiElement(position, size, order){}

UiControl::UiControl(const elysia::core::Vector2& center,const elysia::core::Vector2& size,
    UiFromCenterTag,int order)
    : UiElement(center, size, from_center, order){}

void UiControl::reset() noexcept
{
    UiElement::reset();
    _enabled = true;
    _is_focused = false;
}

void UiControl::set_enabled(bool enabled)
{
    _enabled = enabled;
    if (!_enabled)
    {
        _is_focused = false;
    }
}

bool UiControl::is_enabled() const
{
    return _enabled;
}

void UiControl::set_focused(bool focused)
{
    _is_focused = _enabled && focused;
}

bool UiControl::is_focused() const
{
    return _is_focused;
}

}