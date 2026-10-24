#pragma once

#include "../collision/collision_event.h"

namespace elysia::physics
{
class ICollisionListener
{
public:
    virtual ~ICollisionListener() = default;

    virtual void on_collision_event(const CollisionEvent& event)
    {
        (void)event;
    }
};
}
