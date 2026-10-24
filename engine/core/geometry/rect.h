#pragma once

#include "vector2.h"

#include <algorithm>
#include <cmath>

namespace elysia::core
{
struct Rect
{
    static constexpr float kEpsilon = Vector2::kEpsilon;

    constexpr Rect() noexcept = default;

    constexpr Rect(
        float rect_x,
        float rect_y,
        float rect_width,
        float rect_height
    ) noexcept
        : _x(rect_x),
          _y(rect_y),
          _width(clamp_non_negative(rect_width)),
          _height(clamp_non_negative(rect_height))
    {
    }

    constexpr Rect(const Vector2& position, const Vector2& size) noexcept
        : Rect(position.x, position.y, size.x, size.y){}

    [[nodiscard]] static constexpr Rect from_points(
        const Vector2& a,
        const Vector2& b
    ) noexcept
    {
        const float rect_left = a.x < b.x ? a.x : b.x;
        const float rect_top = a.y < b.y ? a.y : b.y;
        const float rect_right = a.x > b.x ? a.x : b.x;
        const float rect_bottom = a.y > b.y ? a.y : b.y;

        return Rect(
            rect_left,
            rect_top,
            rect_right - rect_left,
            rect_bottom - rect_top
        );
    }

    [[nodiscard]] static constexpr Rect zero() noexcept
    {
        return Rect();
    }

    [[nodiscard]] static constexpr Rect from_position_size(
        const Vector2& position,
        const Vector2& size
    ) noexcept
    {
        return Rect(position, size);
    }

    [[nodiscard]] static constexpr Rect from_center(
        const Vector2& center,
        const Vector2& size
    ) noexcept
    {
        const float rect_width = clamp_non_negative(size.x);
        const float rect_height = clamp_non_negative(size.y);

        return Rect(
            center.x - rect_width * 0.5f,
            center.y - rect_height * 0.5f,
            rect_width,
            rect_height
        );
    }

    [[nodiscard]] constexpr bool operator==(const Rect& rect) const noexcept
    {
        return _x == rect._x
            && _y == rect._y
            && _width == rect._width
            && _height == rect._height;
    }

    [[nodiscard]] constexpr bool operator!=(const Rect& rect) const noexcept
    {
        return !(*this == rect);
    }

    [[nodiscard]] bool nearly_equals(
        const Rect& rect,
        float epsilon = kEpsilon
    ) const noexcept
    {
        return std::fabs(_x - rect._x) <= epsilon
            && std::fabs(_y - rect._y) <= epsilon
            && std::fabs(_width - rect._width) <= epsilon
            && std::fabs(_height - rect._height) <= epsilon;
    }

    [[nodiscard]] constexpr float x() const noexcept
    {
        return _x;
    }

    [[nodiscard]] constexpr float y() const noexcept
    {
        return _y;
    }

    [[nodiscard]] constexpr float width() const noexcept
    {
        return _width;
    }

    [[nodiscard]] constexpr float height() const noexcept
    {
        return _height;
    }

    [[nodiscard]] constexpr Vector2 position() const noexcept
    {
        return Vector2(_x, _y);
    }

    [[nodiscard]] constexpr Vector2 size() const noexcept
    {
        return Vector2(_width, _height);
    }

    [[nodiscard]] constexpr Vector2 center() const noexcept
    {
        return Vector2(_x + _width * 0.5f, _y + _height * 0.5f);
    }

    void set_x(float x) noexcept
    {
        _x = x;
    }

    void set_y(float y) noexcept
    {
        _y = y;
    }

    void set_position(const Vector2& position) noexcept
    {
        _x = position.x;
        _y = position.y;
    }

    void set_width(float width) noexcept
    {
        _width = clamp_non_negative(width);
    }

    void set_height(float height) noexcept
    {
        _height = clamp_non_negative(height);
    }

    void set_size(const Vector2& size) noexcept
    {
        _width = clamp_non_negative(size.x);
        _height = clamp_non_negative(size.y);
    }

    void set_center(const Vector2& center) noexcept
    {
        _x = center.x - _width * 0.5f;
        _y = center.y - _height * 0.5f;
    }

    void set_top_left(const Vector2& point) noexcept
    {
        _x = point.x;
        _y = point.y;
    }

    void set_bottom_center(const Vector2& point) noexcept
    {
        _x = point.x - _width * 0.5f;
        _y = point.y - _height;
    }

    [[nodiscard]] constexpr float left() const noexcept
    {
        return _x;
    }

    [[nodiscard]] constexpr float right() const noexcept
    {
        return _x + _width;
    }

    [[nodiscard]] constexpr float top() const noexcept
    {
        return _y;
    }

    [[nodiscard]] constexpr float bottom() const noexcept
    {
        return _y + _height;
    }

    [[nodiscard]] constexpr Vector2 top_left() const noexcept
    {
        return Vector2(left(), top());
    }

    [[nodiscard]] constexpr Vector2 top_center() const noexcept
    {
        return Vector2(center().x, top());
    }

    [[nodiscard]] constexpr Vector2 top_right() const noexcept
    {
        return Vector2(right(), top());
    }

    [[nodiscard]] constexpr Vector2 center_left() const noexcept
    {
        return Vector2(left(), center().y);
    }

    [[nodiscard]] constexpr Vector2 center_right() const noexcept
    {
        return Vector2(right(), center().y);
    }

    [[nodiscard]] constexpr Vector2 bottom_left() const noexcept
    {
        return Vector2(left(), bottom());
    }

    [[nodiscard]] constexpr Vector2 bottom_center() const noexcept
    {
        return Vector2(center().x, bottom());
    }

    [[nodiscard]] constexpr Vector2 bottom_right() const noexcept
    {
        return Vector2(right(), bottom());
    }

    [[nodiscard]] constexpr float area() const noexcept
    {
        return _width * _height;
    }

    [[nodiscard]] constexpr bool is_empty(float epsilon = kEpsilon) const noexcept
    {
        return _width <= epsilon || _height <= epsilon;
    }

    [[nodiscard]] constexpr Rect translated(const Vector2& offset) const noexcept
    {
        return Rect(_x + offset.x, _y + offset.y, _width, _height);
    }

    [[nodiscard]] constexpr Rect expanded(float amount) const noexcept
    {
        return from_center(
            center(),
            Vector2(_width + amount * 2.0f, _height + amount * 2.0f)
        );
    }

    [[nodiscard]] constexpr Rect expanded(const Vector2& amount) const noexcept
    {
        return from_center(
            center(),
            Vector2(
                _width + amount.x * 2.0f,
                _height + amount.y * 2.0f
            )
        );
    }

    [[nodiscard]] constexpr bool contains(const Vector2& point) const noexcept
    {
        return point.x >= left()
            && point.x <= right()
            && point.y >= top()
            && point.y <= bottom();
    }

    [[nodiscard]] constexpr bool contains(const Rect& rect) const noexcept
    {
        return rect.left() >= left()
            && rect.right() <= right()
            && rect.top() >= top()
            && rect.bottom() <= bottom();
    }

    [[nodiscard]] constexpr bool contains_half_open(
        const Vector2& point
    ) const noexcept
    {
        return point.x >= left()
            && point.x < right()
            && point.y >= top()
            && point.y < bottom();
    }

    [[nodiscard]] constexpr bool intersects(const Rect& rect) const noexcept
    {
        return right() > rect.left()
            && left() < rect.right()
            && bottom() > rect.top()
            && top() < rect.bottom();
    }

    [[nodiscard]] constexpr bool touches_or_intersects(const Rect& rect) const noexcept
    {
        return right() >= rect.left()
            && left() <= rect.right()
            && bottom() >= rect.top()
            && top() <= rect.bottom();
    }

    [[nodiscard]] constexpr Rect merged(const Rect& rect) const noexcept
    {
        const float merged_left = std::min(left(), rect.left());
        const float merged_top = std::min(top(), rect.top());
        const float merged_right = std::max(right(), rect.right());
        const float merged_bottom = std::max(bottom(), rect.bottom());

        return Rect(
            merged_left,
            merged_top,
            merged_right - merged_left,
            merged_bottom - merged_top
        );
    }

    [[nodiscard]] constexpr Rect intersection(const Rect& rect) const noexcept
    {
        const float overlap_left = std::max(left(), rect.left());
        const float overlap_top = std::max(top(), rect.top());
        const float overlap_right = std::min(right(), rect.right());
        const float overlap_bottom = std::min(bottom(), rect.bottom());

        if (overlap_right <= overlap_left || overlap_bottom <= overlap_top)
            return Rect(overlap_left, overlap_top, 0.0f, 0.0f);

        return Rect(
            overlap_left,
            overlap_top,
            overlap_right - overlap_left,
            overlap_bottom - overlap_top
        );
    }

private:
    [[nodiscard]] static constexpr float clamp_non_negative(float value) noexcept
    {
        return value >= 0.0f ? value : 0.0f;
    }

private:
    float _x = 0.0f;
    float _y = 0.0f;
    float _width = 0.0f;
    float _height = 0.0f;
};

}
