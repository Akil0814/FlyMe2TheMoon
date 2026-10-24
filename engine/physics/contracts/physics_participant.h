#pragma once
#include "../physics_world.h"
namespace elysia::physics
{
// Scene registration consumes definitions once. All subsequent changes are explicit.
class PhysicsParticipant
{
  public:
    virtual ~PhysicsParticipant() = default;
    virtual BodyDefinition body_definition() const
    {
        BodyDefinition d;
        d.type = BodyType::Static;
        return d;
    }
    virtual std::span<const Collider> collider_definitions() const = 0;
    void bind_physics(PhysicsWorld &world, PhysicsObjectHandle handle)
    {
        _world = &world;
        _handle = handle;
    }
    void unbind_physics() noexcept
    {
        _world = nullptr;
        _handle = {};
    }
    PhysicsObjectHandle physics_handle() const noexcept
    {
        return _handle;
    }
    PhysicsWorld *physics_world() const noexcept
    {
        return _world;
    }
    ColliderId physics_collider(std::size_t i) const noexcept
    {
        return _world ? _world->collider_id(_handle, i) : InvalidColliderId;
    }
    std::optional<BodyState> physics_state() const noexcept
    {
        return _world ? _world->body_state(_handle) : std::nullopt;
    }
    elysia::core::Vector2 velocity() const noexcept
    {
        auto s = physics_state();
        return s ? s->velocity : elysia::core::Vector2{};
    }
    void set_velocity(elysia::core::Vector2 v)
    {
        if (_world)
            _world->set_velocity(_handle, v);
    }
    void set_velocity_x(float x)
    {
        if (_world)
            _world->set_velocity_x(_handle, x);
    }
    void set_velocity_y(float y)
    {
        if (_world)
            _world->set_velocity_y(_handle, y);
    }
    void update_physics_collider(std::size_t i, const Collider &c)
    {
        if (_world)
            _world->update_collider(physics_collider(i), c);
    }

  private:
    PhysicsWorld *_world = nullptr;
    PhysicsObjectHandle _handle{};
};
} // namespace elysia::physics
