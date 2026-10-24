#pragma once

namespace elysia::physics
{
// Implement on a registered GameObject to author forces, velocity and collision
// state once per simulated step. Input should be latched outside this callback.
class PhysicsStepParticipant
{
public:
    virtual ~PhysicsStepParticipant() = default;
    virtual void fixed_update(double fixed_delta_seconds) = 0;
};
}
