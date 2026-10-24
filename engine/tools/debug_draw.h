#pragma once

#include "singleton.h"

#include "../core/geometry/rect.h"
#include "../core/geometry/vector2.h"
#include "../core/render/color.h"

#include <cstdint>
#include <span>
#include <variant>
#include <vector>

#define ELYSIA_DEBUG_DRAW (::elysia::tools::DebugDraw::instance())

namespace elysia::tools
{
enum class DebugDrawCategory : std::uint32_t
{
    None = 0,
    PhysicsCollider = 1u << 0,
    PhysicsContact = 1u << 1,
    PhysicsContactNormal = 1u << 2,
    PhysicsBroadPhase = 1u << 3,
    PhysicsPoseHistory = 1u << 4,
    PhysicsVelocity = 1u << 5,
    Camera = 1u << 6,
    Gameplay = 1u << 7,
    PhysicsJoint = 1u << 8,
    All = (1u << 9) - 1u
};

[[nodiscard]] constexpr DebugDrawCategory operator|(DebugDrawCategory first,DebugDrawCategory second) noexcept
{
    return static_cast<DebugDrawCategory>(static_cast<std::uint32_t>(first) | static_cast<std::uint32_t>(second));
}

[[nodiscard]] constexpr DebugDrawCategory operator&(DebugDrawCategory first,DebugDrawCategory second) noexcept
{
    return static_cast<DebugDrawCategory>(static_cast<std::uint32_t>(first)& static_cast<std::uint32_t>(second));
}

constexpr DebugDrawCategory& operator|=(DebugDrawCategory& first,DebugDrawCategory second) noexcept
{
    first = first | second;
    return first;
}

struct DebugDrawRect
{
    elysia::core::Rect rect{};
};

struct DebugDrawCircle
{
    elysia::core::Vector2 center{};
    float radius = 0.0f;
};

struct DebugDrawLine
{
    elysia::core::Vector2 start{};
    elysia::core::Vector2 end{};
};

struct DebugDrawPoint
{
    elysia::core::Vector2 position{};

    // Screen-space logical diameter. It is intentionally independent of camera zoom.
    float size = 1.0f;
};

using DebugDrawPrimitive = std::variant<
    DebugDrawRect,
    DebugDrawCircle,
    DebugDrawLine,
    DebugDrawPoint
>;

struct DebugDrawCommand
{
    DebugDrawCategory category = DebugDrawCategory::None;
    DebugDrawPrimitive primitive{DebugDrawRect{}};
    elysia::core::Color color{};

    // Screen-space logical width. It is intentionally independent of camera zoom.
    float thickness = 1.0f;
};

class DebugDraw final : public Singleton<DebugDraw>
{
    friend Singleton<DebugDraw>;

public:
    void set_enabled(bool enabled) noexcept;
    [[nodiscard]] bool enabled() const noexcept;

    void set_enabled_categories(DebugDrawCategory categories) noexcept;
    [[nodiscard]] DebugDrawCategory enabled_categories() const noexcept;
    [[nodiscard]] bool is_enabled(DebugDrawCategory categories) const noexcept;

    void draw_rect(DebugDrawCategory category,const elysia::core::Rect& rect,
        elysia::core::Color color,float thickness = 1.0f);

    void draw_circle(DebugDrawCategory category,elysia::core::Vector2 center,
        float radius,elysia::core::Color color,float thickness = 1.0f );

    void draw_line(DebugDrawCategory category,elysia::core::Vector2 start,
        elysia::core::Vector2 end,elysia::core::Color color,float thickness = 1.0f);

    void draw_point(DebugDrawCategory category,elysia::core::Vector2 position,
        float size,elysia::core::Color color);

    void clear_categories(DebugDrawCategory categories) noexcept;
    void clear() noexcept;

    [[nodiscard]] std::span<const DebugDrawCommand> commands() const noexcept;

private:
    DebugDraw() = default;

    bool _enabled = false;
    DebugDrawCategory _enabled_categories = DebugDrawCategory::All;
    std::vector<DebugDrawCommand> _commands;
};
}
