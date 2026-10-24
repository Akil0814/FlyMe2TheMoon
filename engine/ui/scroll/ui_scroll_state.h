#pragma once

#include "../../core/geometry/rect.h"
#include "../../core/geometry/vector2.h"

namespace elysia::ui
{
enum class UiScrollAxis
{
    Auto,
    Vertical,
    Horizontal,
    Both
};

class UiScrollState
{
public:
    void reset() noexcept;

    void set_axis(UiScrollAxis axis) noexcept;
    [[nodiscard]] UiScrollAxis axis() const noexcept;
    // Resolves Auto into the axis or axes currently supported by viewport and content sizes.
    [[nodiscard]] UiScrollAxis resolved_axis() const noexcept;

    void set_viewport_size(const elysia::core::Vector2& viewport_size) noexcept;
    [[nodiscard]] elysia::core::Vector2 viewport_size() const noexcept;

    void set_content_size(const elysia::core::Vector2& content_size) noexcept;
    [[nodiscard]] elysia::core::Vector2 content_size() const noexcept;
    // Expands content metrics so auto axis resolution still accounts for the viewport floor.
    [[nodiscard]] elysia::core::Vector2 effective_content_size() const noexcept;

    void set_offset(const elysia::core::Vector2& offset) noexcept;
    [[nodiscard]] elysia::core::Vector2 offset() const noexcept;
    [[nodiscard]] elysia::core::Vector2 max_offset() const noexcept;
    [[nodiscard]] bool can_scroll_horizontal() const noexcept;
    [[nodiscard]] bool can_scroll_vertical() const noexcept;
    [[nodiscard]] float horizontal_ratio() const noexcept;
    [[nodiscard]] float vertical_ratio() const noexcept;
    void set_horizontal_ratio(float ratio) noexcept;
    void set_vertical_ratio(float ratio) noexcept;

    void set_step(const elysia::core::Vector2& step) noexcept;
    [[nodiscard]] elysia::core::Vector2 step() const noexcept;

    // Moves the viewport by a delta and clamps the result to the valid scroll range.
    void scroll_by(const elysia::core::Vector2& delta) noexcept;
    void scroll_to_left() noexcept;
    void scroll_to_right() noexcept;
    void scroll_to_top() noexcept;
    void scroll_to_bottom() noexcept;
    // Adjusts the offset just enough to bring a local content rect into view.
    void ensure_visible(const elysia::core::Rect& local_rect) noexcept;

private:
    [[nodiscard]] elysia::core::Vector2 clamp_size(const elysia::core::Vector2& value) const noexcept;
    [[nodiscard]] elysia::core::Vector2 filter_axis(const elysia::core::Vector2& value) const noexcept;
    [[nodiscard]] elysia::core::Vector2 clamp_offset(const elysia::core::Vector2& offset) const noexcept;

private:
    UiScrollAxis _axis = UiScrollAxis::Vertical;
    elysia::core::Vector2 _viewport_size{ 0.0f,0.0f };
    elysia::core::Vector2 _content_size{ 0.0f,0.0f };
    elysia::core::Vector2 _offset{ 0.0f,0.0f };
    elysia::core::Vector2 _step{ 24.0f,24.0f };
};
}
