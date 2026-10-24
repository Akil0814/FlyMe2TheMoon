#include "debug_draw.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace elysia::tools
{
namespace
{
[[nodiscard]] constexpr std::uint32_t category_bits(DebugDrawCategory category) noexcept
{
    return static_cast<std::uint32_t>(category);
}

[[nodiscard]] constexpr bool is_single_category(DebugDrawCategory category) noexcept
{
    const std::uint32_t bits = category_bits(category);
    return bits != 0
        && (bits & ~category_bits(DebugDrawCategory::All)) == 0
        && std::has_single_bit(bits);
}

[[nodiscard]] bool is_finite(const elysia::core::Vector2& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] bool is_finite(const elysia::core::Rect& rect) noexcept
{
    return std::isfinite(rect.x())
        && std::isfinite(rect.y())
        && std::isfinite(rect.width())
        && std::isfinite(rect.height());
}

[[nodiscard]] bool is_valid_thickness(float thickness) noexcept
{
    return std::isfinite(thickness) && thickness > 0.0f;
}
}

void DebugDraw::set_enabled(bool enabled) noexcept
{
    _enabled = enabled;
    if (!_enabled)
        clear();
}

bool DebugDraw::enabled() const noexcept
{
    return _enabled;
}

void DebugDraw::set_enabled_categories(DebugDrawCategory categories) noexcept
{
    const DebugDrawCategory sanitized = static_cast<DebugDrawCategory>(
        category_bits(categories) & category_bits(DebugDrawCategory::All)
    );
    const DebugDrawCategory disabled = static_cast<DebugDrawCategory>(
        category_bits(_enabled_categories) & ~category_bits(sanitized)
    );
    _enabled_categories = sanitized;
    clear_categories(disabled);
}

DebugDrawCategory DebugDraw::enabled_categories() const noexcept
{
    return _enabled_categories;
}

bool DebugDraw::is_enabled(DebugDrawCategory categories) const noexcept
{
    const std::uint32_t requested =
        category_bits(categories) & category_bits(DebugDrawCategory::All);
    if (!_enabled || requested == 0)
        return false;

    return (category_bits(_enabled_categories) & requested) == requested;
}

void DebugDraw::draw_rect(
    DebugDrawCategory category,
    const elysia::core::Rect& rect,
    elysia::core::Color color,
    float thickness)
{
    if (!is_enabled(category)
        || !is_single_category(category)
        || !is_finite(rect)
        || rect.is_empty()
        || !is_valid_thickness(thickness))
    {
        return;
    }

    _commands.push_back(DebugDrawCommand{
        category,
        DebugDrawRect{rect},
        color,
        thickness
    });
}

void DebugDraw::draw_circle(
    DebugDrawCategory category,
    elysia::core::Vector2 center,
    float radius,
    elysia::core::Color color,
    float thickness)
{
    if (!is_enabled(category)
        || !is_single_category(category)
        || !is_finite(center)
        || !std::isfinite(radius)
        || radius <= 0.0f
        || !is_valid_thickness(thickness))
    {
        return;
    }

    _commands.push_back(DebugDrawCommand{
        category,
        DebugDrawCircle{center, radius},
        color,
        thickness
    });
}

void DebugDraw::draw_line(
    DebugDrawCategory category,
    elysia::core::Vector2 start,
    elysia::core::Vector2 end,
    elysia::core::Color color,
    float thickness)
{
    if (!is_enabled(category)
        || !is_single_category(category)
        || !is_finite(start)
        || !is_finite(end)
        || start == end
        || !is_valid_thickness(thickness))
    {
        return;
    }

    _commands.push_back(DebugDrawCommand{
        category,
        DebugDrawLine{start, end},
        color,
        thickness
    });
}

void DebugDraw::draw_point(
    DebugDrawCategory category,
    elysia::core::Vector2 position,
    float size,
    elysia::core::Color color)
{
    if (!is_enabled(category)
        || !is_single_category(category)
        || !is_finite(position)
        || !std::isfinite(size)
        || size <= 0.0f)
    {
        return;
    }

    _commands.push_back(DebugDrawCommand{
        category,
        DebugDrawPoint{position, size},
        color,
        1.0f
    });
}

void DebugDraw::clear_categories(DebugDrawCategory categories) noexcept
{
    const std::uint32_t clear_bits =
        category_bits(categories) & category_bits(DebugDrawCategory::All);
    if (clear_bits == 0)
        return;

    std::erase_if(_commands, [clear_bits](const DebugDrawCommand& command)
    {
        return (category_bits(command.category) & clear_bits) != 0;
    });
}

void DebugDraw::clear() noexcept
{
    _commands.clear();
}

std::span<const DebugDrawCommand> DebugDraw::commands() const noexcept
{
    return _commands;
}
}
