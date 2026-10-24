#pragma once

#include "gameplay_collision_types.h"

namespace elysia::gameplay::collision
{
class TeamRelationResolver
{
public:
    virtual ~TeamRelationResolver() = default;

    [[nodiscard]] virtual TeamRelation relation(
        TeamId source,
        TeamId target) const noexcept = 0;
};
}
