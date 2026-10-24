#pragma once

#include "gameplay_collision_types.h"

#include <vector>

namespace elysia::gameplay::collision
{
struct ActorCollisionRig
{
    ActorId owner = InvalidActorId;
    TeamId team = InvalidTeamId;

    elysia::physics::ColliderId body = elysia::physics::InvalidColliderId;
    elysia::physics::ColliderId push_box = elysia::physics::InvalidColliderId;

    std::vector<elysia::physics::ColliderId> hurt_boxes;
    std::vector<elysia::physics::ColliderId> sensors;
};
}
