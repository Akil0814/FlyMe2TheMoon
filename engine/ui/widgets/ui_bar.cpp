#include "ui_bar.h"

#include "../style/ui_style_defaults.h"
#include "../../core/render/render_command.h"

#include <algorithm>

namespace elysia::ui
{
UiBar::UiBar(const elysia::core::Rect& rect,int order) noexcept
    : UiElement(rect,order)
{
    reset();
}

UiBar::UiBar(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiElement(position,size,order)
{
    reset();
}

UiBar::UiBar(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiElement(center,size,from_center,order)
{
    reset();
}

void UiBar::reset() noexcept
{
    UiElement::reset();

    _min_value = 0.0f;
    _max_value = 1.0f;
    _value = 0.0f;
    _style_state.reset(UiStyleDefaults::bar());
    _visual_role = UiBarVisualRole::Default;
    _fill_direction = BarFillDirection::LeftToRight;
    _padding = 0;
}

void UiBar::set_range(float min_value,float max_value)
{
    if (max_value < min_value)
        std::swap(min_value, max_value);

    _min_value = min_value;
    _max_value = max_value;
    _value = std::clamp(_value, _min_value, _max_value);
}

void UiBar::set_value(float value)
{
    _value = std::clamp(value, _min_value, _max_value);
}

void UiBar::set_ratio(float ratio)
{
    const float clamped_ratio = std::clamp(ratio, 0.0f, 1.0f);
    _value = _min_value + (_max_value - _min_value) * clamped_ratio;
}

float UiBar::min_value() const
{
    return _min_value;
}

float UiBar::max_value() const
{
    return _max_value;
}

float UiBar::value() const
{
    return _value;
}

float UiBar::ratio() const
{
    const float range = _max_value - _min_value;
    if (range <= 0.0f)
        return 0.0f;

    return (_value - _min_value) / range;
}

void UiBar::set_base_style(const UiBarStyle& style) noexcept
{
    _style_state.set_base_style(style);
}

void UiBar::set_style_overrides(const UiBarStyleOverrides& overrides) noexcept
{
    _style_state.set_style_overrides(overrides);
}

const UiBarStyle& UiBar::style() const noexcept
{
    return _style_state.effective_style();
}

const UiBarStyleOverrides& UiBar::style_overrides() const noexcept { return _style_state.style_overrides(); }
bool UiBar::has_style_overrides() const noexcept
{
    return _style_state.has_style_overrides();
}

void UiBar::clear_style_overrides() noexcept
{
    _style_state.clear_style_overrides();
}

void UiBar::set_visual_role(UiBarVisualRole role) noexcept
{
    _visual_role = role;
    notify_base_style_invalidated();
}

UiBarVisualRole UiBar::visual_role() const noexcept
{
    return _visual_role;
}

void UiBar::set_fill_direction(BarFillDirection direction)
{
    _fill_direction = direction;
}

BarFillDirection UiBar::fill_direction() const
{
    return _fill_direction;
}

void UiBar::set_padding(int padding)
{
    _padding = std::max(0, padding);
}

int UiBar::padding() const
{
    return _padding;
}

void UiBar::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;

    const elysia::core::Rect& bar_rect = screen_rect();
    if (bar_rect.is_empty())
        return;

    const UiBarStyle& style = _style_state.effective_style();
    out_commands.push_back(elysia::core::make_ui_fill_rect_command(bar_rect,apply_opacity(style.background),style.corner_radius));

    const elysia::core::Rect fill = fill_rect(bar_rect);
    if (!fill.is_empty())
        out_commands.push_back(elysia::core::make_ui_fill_rect_command(fill,apply_opacity(style.fill),style.corner_radius));

    if (style.draw_border)
        out_commands.push_back(elysia::core::make_ui_draw_rect_command(
            bar_rect,apply_opacity(style.border),style.corner_radius,style.border_width));
}

elysia::core::Rect UiBar::content_rect(const elysia::core::Rect& rect) const
{
    const float width = std::max(0.0f, rect.width());
    const float height = std::max(0.0f, rect.height());

    const float padding = static_cast<float>(_padding);
    const float pad_x = std::min(padding, width * 0.5f);
    const float pad_y = std::min(padding, height * 0.5f);

    elysia::core::Rect content = rect;
    content.set_x(rect.x() + pad_x);
    content.set_y(rect.y() + pad_y);
    content.set_width(width - pad_x * 2.0f);
    content.set_height(height - pad_y * 2.0f);
    return content;
}

elysia::core::Rect UiBar::fill_rect(const elysia::core::Rect& rect) const
{
    elysia::core::Rect fill = content_rect(rect);
    const float clamped_ratio = std::clamp(ratio(), 0.0f, 1.0f);

    switch (_fill_direction)
    {
    case BarFillDirection::LeftToRight:
        fill.set_width(fill.width() * clamped_ratio);
        break;

    case BarFillDirection::RightToLeft:
    {
        const float new_width = fill.width() * clamped_ratio;
        fill.set_x(fill.x() + fill.width() - new_width);
        fill.set_width(new_width);
        break;
    }

    case BarFillDirection::TopToBottom:
        fill.set_height(fill.height() * clamped_ratio);
        break;

    case BarFillDirection::BottomToTop:
    {
        const float new_height = fill.height() * clamped_ratio;
        fill.set_y(fill.y() + fill.height() - new_height);
        fill.set_height(new_height);
        break;
    }
    }

    return fill;
}


}

