#include "ui_scroll_state.h"

#include <algorithm>

namespace elysia::ui
{
namespace
{
constexpr float ScrollEpsilon = 0.01f;

[[nodiscard]] float clamp01(float value) noexcept
{
    return std::clamp(value,0.0f,1.0f);
}

[[nodiscard]] bool axis_allows_horizontal(UiScrollAxis axis) noexcept
{
    return axis == UiScrollAxis::Horizontal || axis == UiScrollAxis::Both;
}

[[nodiscard]] bool axis_allows_vertical(UiScrollAxis axis) noexcept
{
    return axis == UiScrollAxis::Vertical || axis == UiScrollAxis::Both;
}
}

void UiScrollState::reset() noexcept
{
    _axis = UiScrollAxis::Vertical;
    _viewport_size = elysia::core::Vector2::zero();
    _content_size = elysia::core::Vector2::zero();
    _offset = elysia::core::Vector2::zero();
    _step = elysia::core::Vector2(24.0f,24.0f);
}

void UiScrollState::set_axis(UiScrollAxis axis) noexcept
{
    _axis = axis;
    _offset = clamp_offset(_offset);
}

UiScrollAxis UiScrollState::axis() const noexcept
{
    return _axis;
}

UiScrollAxis UiScrollState::resolved_axis() const noexcept
{
    if (_axis != UiScrollAxis::Auto)
        return _axis;

    const elysia::core::Vector2 content = effective_content_size();
    const bool overflow_x = content.x > _viewport_size.x + ScrollEpsilon;
    const bool overflow_y = content.y > _viewport_size.y + ScrollEpsilon;

    if (overflow_x && overflow_y)
        return UiScrollAxis::Both;
    if (overflow_x)
        return UiScrollAxis::Horizontal;
    return UiScrollAxis::Vertical;
}

void UiScrollState::set_viewport_size(const elysia::core::Vector2& viewport_size) noexcept
{
    _viewport_size = clamp_size(viewport_size);
    _offset = clamp_offset(_offset);
}

elysia::core::Vector2 UiScrollState::viewport_size() const noexcept
{
    return _viewport_size;
}

void UiScrollState::set_content_size(const elysia::core::Vector2& content_size) noexcept
{
    _content_size = clamp_size(content_size);
    _offset = clamp_offset(_offset);
}

elysia::core::Vector2 UiScrollState::content_size() const noexcept
{
    return _content_size;
}

elysia::core::Vector2 UiScrollState::effective_content_size() const noexcept
{
    return elysia::core::Vector2(
        _content_size.x > 0.0f ? _content_size.x : _viewport_size.x,
        _content_size.y > 0.0f ? _content_size.y : _viewport_size.y
    );
}

void UiScrollState::set_offset(const elysia::core::Vector2& offset) noexcept
{
    _offset = clamp_offset(offset);
}

elysia::core::Vector2 UiScrollState::offset() const noexcept
{
    return _offset;
}

elysia::core::Vector2 UiScrollState::max_offset() const noexcept
{
    const elysia::core::Vector2 content = effective_content_size();
    elysia::core::Vector2 max(
        std::max(0.0f,content.x - _viewport_size.x),
        std::max(0.0f,content.y - _viewport_size.y)
    );

    const UiScrollAxis axis = resolved_axis();
    if (!axis_allows_horizontal(axis))
        max.x = 0.0f;
    if (!axis_allows_vertical(axis))
        max.y = 0.0f;
    return max;
}

bool UiScrollState::can_scroll_horizontal() const noexcept
{
    return max_offset().x > ScrollEpsilon;
}

bool UiScrollState::can_scroll_vertical() const noexcept
{
    return max_offset().y > ScrollEpsilon;
}

float UiScrollState::horizontal_ratio() const noexcept
{
    const float max = max_offset().x;
    return max > ScrollEpsilon ? clamp01(_offset.x / max) : 0.0f;
}

float UiScrollState::vertical_ratio() const noexcept
{
    const float max = max_offset().y;
    return max > ScrollEpsilon ? clamp01(_offset.y / max) : 0.0f;
}

void UiScrollState::set_horizontal_ratio(float ratio) noexcept
{
    const float max = max_offset().x;
    set_offset(elysia::core::Vector2(max * clamp01(ratio),_offset.y));
}

void UiScrollState::set_vertical_ratio(float ratio) noexcept
{
    const float max = max_offset().y;
    set_offset(elysia::core::Vector2(_offset.x,max * clamp01(ratio)));
}

void UiScrollState::set_step(const elysia::core::Vector2& step) noexcept
{
    _step = clamp_size(step);
}

elysia::core::Vector2 UiScrollState::step() const noexcept
{
    return _step;
}

void UiScrollState::scroll_by(const elysia::core::Vector2& delta) noexcept
{
    const elysia::core::Vector2 filtered_delta = filter_axis(delta);
    set_offset(elysia::core::Vector2(_offset.x + filtered_delta.x,_offset.y + filtered_delta.y));
}

void UiScrollState::scroll_to_left() noexcept
{
    set_offset(elysia::core::Vector2(0.0f,_offset.y));
}

void UiScrollState::scroll_to_right() noexcept
{
    set_offset(elysia::core::Vector2(max_offset().x,_offset.y));
}

void UiScrollState::scroll_to_top() noexcept
{
    set_offset(elysia::core::Vector2(_offset.x,0.0f));
}

void UiScrollState::scroll_to_bottom() noexcept
{
    set_offset(elysia::core::Vector2(_offset.x,max_offset().y));
}

void UiScrollState::ensure_visible(const elysia::core::Rect& local_rect) noexcept
{
    elysia::core::Vector2 next = _offset;
    const UiScrollAxis axis = resolved_axis();

    if (axis_allows_horizontal(axis))
    {
        if (local_rect.x() < next.x)
            next.x = local_rect.x();
        else if (local_rect.right() > next.x + _viewport_size.x)
            next.x = local_rect.right() - _viewport_size.x;
    }

    if (axis_allows_vertical(axis))
    {
        if (local_rect.y() < next.y)
            next.y = local_rect.y();
        else if (local_rect.bottom() > next.y + _viewport_size.y)
            next.y = local_rect.bottom() - _viewport_size.y;
    }

    set_offset(next);
}

elysia::core::Vector2 UiScrollState::clamp_size(const elysia::core::Vector2& value) const noexcept
{
    return elysia::core::Vector2(std::max(0.0f,value.x),std::max(0.0f,value.y));
}

elysia::core::Vector2 UiScrollState::filter_axis(const elysia::core::Vector2& value) const noexcept
{
    switch (resolved_axis())
    {
    case UiScrollAxis::Horizontal:
        return elysia::core::Vector2(value.x,0.0f);
    case UiScrollAxis::Vertical:
        return elysia::core::Vector2(0.0f,value.y);
    case UiScrollAxis::Both:
    case UiScrollAxis::Auto:
    default:
        return value;
    }
}

elysia::core::Vector2 UiScrollState::clamp_offset(const elysia::core::Vector2& offset) const noexcept
{
    const elysia::core::Vector2 filtered = filter_axis(offset);
    const elysia::core::Vector2 max = max_offset();
    return elysia::core::Vector2(
        std::clamp(filtered.x,0.0f,max.x),
        std::clamp(filtered.y,0.0f,max.y)
    );
}
}
