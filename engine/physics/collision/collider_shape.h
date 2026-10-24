#pragma once

#include "../../core/geometry/rect.h"
#include "../../core/geometry/vector2.h"

#include <variant>

namespace elysia::physics
{
struct AabbShape
{
    elysia::core::Rect local_rect{};
    bool operator==(const AabbShape&) const noexcept = default;
};

struct CircleShape
{
    elysia::core::Vector2 local_center{};
    float radius = 0.0f;
    bool operator==(const CircleShape&) const noexcept = default;
};

using ColliderShape = std::variant<AabbShape, CircleShape>;
}
