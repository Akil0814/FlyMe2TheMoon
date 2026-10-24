#pragma once
#include "../../core/geometry/rect.h"
#include <array>
#include <variant>
namespace elysia::physics {
struct WorldAabb { elysia::core::Rect rect{}; };
struct WorldCircle { elysia::core::Vector2 center{}; float radius=0; };
struct WorldPolygon { std::array<elysia::core::Vector2,4> vertices{}; };
using WorldColliderShape=std::variant<WorldAabb,WorldCircle,WorldPolygon>;
}
