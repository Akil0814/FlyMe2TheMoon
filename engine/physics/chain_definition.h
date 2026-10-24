#pragma once
#include "collision/collider.h"
#include "physics_object_handle.h"
#include <optional>
#include <string>
#include <vector>
namespace elysia::physics
{
struct ChainHandle
{
    std::uint64_t value = 0;
    bool is_valid() const noexcept
    {
        return value != 0;
    }
    bool operator==(const ChainHandle &) const noexcept = default;
};
struct ChainDefinition
{
    std::vector<elysia::core::Vector2> local_vertices = std::vector<elysia::core::Vector2>(std::size_t{0});
    bool loop = false;
    std::optional<elysia::core::Vector2> ghost_before{}, ghost_after{};
    CollisionFilter filter{};
    PhysicsMaterial material{};
    std::string tag = std::string(std::size_t{0}, char{});
};
struct ChainState
{
    PhysicsObjectHandle owner{};
    bool loop = false;
    std::size_t vertex_count = 0, segment_count = 0;
};
struct ChainSegmentInfo
{
    ChainHandle chain{};
    PhysicsObjectHandle owner{};
    std::size_t index = 0;
};
} // namespace elysia::physics
