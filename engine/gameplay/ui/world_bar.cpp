#include "world_bar.h"

#include <cmath>

namespace elysia::gameplay::ui
{
void WorldBar::set_range(float minimum, float maximum) noexcept
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || maximum < minimum)
        return;
    _minimum = minimum;
    _maximum = maximum;
    set_value(_value);
}

void WorldBar::set_value(float value) noexcept
{
    if (std::isfinite(value))
        _value = std::clamp(value, _minimum, _maximum);
}

void WorldBar::set_ratio(float ratio) noexcept
{
    if (std::isfinite(ratio))
        _value = std::lerp(_minimum, _maximum, std::clamp(ratio, 0.0f, 1.0f));
}

float WorldBar::ratio() const noexcept
{
    if (_maximum == _minimum)
        return 0.0f;
    return static_cast<float>((static_cast<double>(_value) - _minimum)
        / (static_cast<double>(_maximum) - _minimum));
}

void WorldBar::submit_render_commands(std::vector<elysia::core::RenderCommand>& commands,
    const elysia::core::Rect& world_rect) const
{
    if (!_visible || world_rect.is_empty() || !std::isfinite(world_rect.x())
        || !std::isfinite(world_rect.y()) || !std::isfinite(world_rect.width())
        || !std::isfinite(world_rect.height()))
        return;
    using namespace elysia::core;
    if (_style.background.a != 0)
        commands.push_back(make_world_fill_rect_command(world_rect, _style.background));
    auto fill = world_rect;
    const float amount = ratio();
    switch (_direction)
    {
    case WorldBarFillDirection::LeftToRight: fill.set_width(fill.width() * amount); break;
    case WorldBarFillDirection::RightToLeft:
        fill.set_width(fill.width() * amount);
        fill.set_position({world_rect.right() - fill.width(), fill.y()});
        break;
    case WorldBarFillDirection::TopToBottom: fill.set_height(fill.height() * amount); break;
    case WorldBarFillDirection::BottomToTop:
        fill.set_height(fill.height() * amount);
        fill.set_position({fill.x(), world_rect.bottom() - fill.height()});
        break;
    }
    if (amount > 0.0f && _style.fill.a != 0)
        commands.push_back(make_world_fill_rect_command(fill, _style.fill));
    if (std::isfinite(_style.border_width) && _style.border_width > 0.0f && _style.border.a != 0)
        commands.push_back(make_world_draw_rect_command(world_rect, _style.border, _style.border_width));
}
}
