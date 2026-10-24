#pragma once

#include "../../physics/collision/collision_target.h"

#include <cstdint>

namespace elysia::gameplay::collision
{
using ActorId = std::uint64_t;
using TeamId = std::uint32_t;
using AttackInstanceId = std::uint64_t;
using AttackDefinitionId = std::uint64_t;

inline constexpr ActorId InvalidActorId = 0;
inline constexpr TeamId InvalidTeamId = 0;
inline constexpr AttackInstanceId InvalidAttackInstanceId = 0;
inline constexpr AttackDefinitionId InvalidAttackDefinitionId = 0;

namespace teams
{
inline constexpr TeamId Neutral = 1;
inline constexpr TeamId Player = 2;
inline constexpr TeamId Enemy = 3;
}

enum class ColliderRole : std::uint8_t
{
    Body,
    PushBox,
    HurtBox,
    HitBox,
    Sensor
};

enum class TeamRelation : std::uint8_t
{
    Friendly,
    Neutral,
    Hostile
};

struct ColliderBinding
{
    elysia::physics::ColliderId collider = elysia::physics::InvalidColliderId;
    ActorId owner = InvalidActorId;
    TeamId team = InvalidTeamId;
    ColliderRole role = ColliderRole::Body;
};

struct HitBoxBinding
{
    ColliderBinding collider{};
    ActorId instigator = InvalidActorId;
    AttackInstanceId attack_instance = InvalidAttackInstanceId;
    AttackDefinitionId attack_definition = InvalidAttackDefinitionId;
};

struct DropThroughRequest
{
    elysia::physics::ColliderId actor = elysia::physics::InvalidColliderId;
    elysia::physics::CollisionTarget target{};
};
}
