#pragma once

#include "gameplay_collision_events.h"

namespace elysia::gameplay::collision
{
class GameplayCollisionListener
{
public:
    virtual ~GameplayCollisionListener() = default;

    virtual void on_body_contact(const BodyContactEvent& event)
    {
        (void)event;
    }

    virtual void on_push_box_overlap(const PushBoxOverlapEvent& event)
    {
        (void)event;
    }

    virtual void on_hit_overlap(const HitOverlapEvent& event)
    {
        (void)event;
    }

    virtual void on_sensor_overlap(const SensorOverlapEvent& event)
    {
        (void)event;
    }
};
}
