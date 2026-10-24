#pragma once
#include "body/body_definition.h"
#include "contracts/collision_listener.h"
#include "contracts/collision_query_service.h"
#include "joint_definition.h"
#include "physics_object_handle.h"
#include "physics_world_config.h"
#include "physics_world_stats.h"
#include "tile/tile_collision_world.h"
#include <memory>
#include <span>
namespace elysia::core
{
class GameObject;
}
namespace elysia::physics
{
enum class TeleportVelocityMode : std::uint8_t
{
    Preserve,
    Clear
};
struct PhysicsContactState
{
    bool grounded = false, ceiling = false, wall_left = false, wall_right = false;
};
class PhysicsWorld final : public ICollisionQueryService
{
  public:
    explicit PhysicsWorld(PhysicsWorldConfig config = {});
    ~PhysicsWorld() override;
    PhysicsWorld(const PhysicsWorld &) = delete;
    PhysicsWorld &operator=(const PhysicsWorld &) = delete;
    PhysicsObjectHandle register_object(elysia::core::GameObject &, const BodyDefinition &,
                                        std::span<const Collider>);
    bool unregister_object(PhysicsObjectHandle);
    bool contains_object(PhysicsObjectHandle) const noexcept;
    bool contains_object(const elysia::core::GameObject &) const noexcept;
    bool contains_collider(ColliderId) const noexcept;
    std::size_t registered_object_count() const noexcept;
    std::size_t registered_collider_count() const noexcept;
    std::optional<PhysicsObjectHandle> object_handle(
        const elysia::core::GameObject &) const noexcept;
    ColliderId collider_id(PhysicsObjectHandle, std::size_t index) const noexcept;
    std::optional<BodyState> body_state(PhysicsObjectHandle) const noexcept;
    std::optional<PhysicsPose> render_pose(PhysicsObjectHandle) const noexcept;
    bool set_velocity(PhysicsObjectHandle, elysia::core::Vector2);
    bool set_velocity_x(PhysicsObjectHandle, float);
    bool set_velocity_y(PhysicsObjectHandle, float);
    bool set_angular_velocity(PhysicsObjectHandle, float);
    bool set_gravity_scale(PhysicsObjectHandle, float);
    bool set_body_enabled(PhysicsObjectHandle, bool);
    bool set_awake(PhysicsObjectHandle, bool);
    bool apply_force(PhysicsObjectHandle, elysia::core::Vector2,
                     std::optional<elysia::core::Vector2> world_point = {});
    bool apply_impulse(PhysicsObjectHandle, elysia::core::Vector2,
                       std::optional<elysia::core::Vector2> world_point = {});
    bool apply_torque(PhysicsObjectHandle, float);
    bool apply_angular_impulse(PhysicsObjectHandle, float);
    bool teleport_object(PhysicsObjectHandle, elysia::core::Vector2,
                         TeleportVelocityMode = TeleportVelocityMode::Preserve);
    bool set_transform(PhysicsObjectHandle, PhysicsPose,
                       TeleportVelocityMode = TeleportVelocityMode::Preserve);
    bool update_collider(ColliderId, const Collider &);
    bool set_collider_enabled(ColliderId, bool);
    JointHandle create_distance_joint(const DistanceJointDefinition &);
    JointHandle create_revolute_joint(const RevoluteJointDefinition &);
    bool destroy_joint(JointHandle);
    std::optional<JointState> joint_state(JointHandle) const noexcept;
    bool set_tile_world(const ITileCollisionWorld &);
    bool clear_tile_world(const ITileCollisionWorld &);
    bool update_tiles(TileCoordinate begin, TileCoordinate end);
    const ITileCollisionWorld *tile_world() const noexcept;
    bool add_listener(ICollisionListener &);
    bool remove_listener(const ICollisionListener &);
    bool request_pass_through(ColliderId, CollisionTarget);
    void collect_contacts(CollisionTarget, std::vector<CollisionContact> &) const;
    PhysicsContactState contact_state(PhysicsObjectHandle) const noexcept;
    PhysicsContactState contact_state(CollisionTarget) const noexcept;
    void step(double fixed_delta_seconds);
    void finalize_frame(double interpolation_alpha);
    void reset() noexcept;
    const PhysicsWorldConfig &config() const noexcept;
    const PhysicsStepStats &last_step_stats() const noexcept;
    void set_debug_capture(PhysicsDebugCapture);
    PhysicsDebugCapture debug_capture() const noexcept;
    const PhysicsDebugSnapshot &debug_snapshot() const noexcept;
    std::optional<CollisionQueryHit> raycast(const RayCastQuery &) const override;
    std::optional<CollisionQueryHit> segment_cast(const SegmentCastQuery &) const override;
    void raycast_all(const RayCastQuery &, std::vector<CollisionQueryHit> &) const override;
    void segment_cast_all(const SegmentCastQuery &,
                          std::vector<CollisionQueryHit> &) const override;
    void overlap_aabb(const AabbOverlapQuery &,
                      std::vector<CollisionOverlapQueryHit> &) const override;
    void overlap_circle(const CircleOverlapQuery &,
                        std::vector<CollisionOverlapQueryHit> &) const override;
    std::optional<CollisionQueryHit> sweep_aabb(const AabbSweepQuery &) const override;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
} // namespace elysia::physics
