#pragma once
#include "body/body_definition.h"
#include "chain_definition.h"
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
    std::optional<MassProperties> mass_properties(PhysicsObjectHandle) const noexcept;
    bool set_mass_properties(PhysicsObjectHandle, const MassProperties &);
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
    bool set_target_transform(PhysicsObjectHandle, PhysicsPose, double fixed_delta_seconds);
    ChainHandle create_chain(PhysicsObjectHandle owner,const ChainDefinition&);
    bool destroy_chain(ChainHandle);
    std::optional<ChainState> chain_state(ChainHandle) const noexcept;
    ColliderId chain_collider(ChainHandle,std::size_t index) const noexcept;
    std::optional<ChainSegmentInfo> chain_segment_info(ColliderId) const noexcept;
    bool update_collider(ColliderId, const Collider &);
    bool set_collider_enabled(ColliderId, bool);
    JointHandle create_distance_joint(const DistanceJointDefinition &);
    JointHandle create_revolute_joint(const RevoluteJointDefinition &);
    bool update_distance_joint(JointHandle, const DistanceJointSettings&);
    bool update_revolute_joint(JointHandle, const RevoluteJointSettings&);
    JointHandle create_weld_joint(const WeldJointDefinition&);
    bool update_weld_joint(JointHandle,const WeldJointSettings&);
    JointHandle create_prismatic_joint(const PrismaticJointDefinition&);
    bool update_prismatic_joint(JointHandle,const PrismaticJointSettings&);
    JointHandle create_wheel_joint(const WheelJointDefinition&);
    bool update_wheel_joint(JointHandle,const WheelJointSettings&);
    JointHandle create_mouse_joint(const MouseJointDefinition&);
    bool update_mouse_joint(JointHandle,const MouseJointSettings&);
    JointHandle create_motor_joint(const MotorJointDefinition&);
    bool update_motor_joint(JointHandle,const MotorJointSettings&);
    JointHandle create_filter_joint(const FilterJointDefinition&);
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
    void overlap_shape(const ShapeOverlapQuery &, std::vector<CollisionOverlapQueryHit> &) const override;
    std::optional<CollisionQueryHit> shape_cast(const ShapeCastQuery &) const override;
    void shape_cast_all(const ShapeCastQuery &, std::vector<CollisionQueryHit> &) const override;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
} // namespace elysia::physics
