#pragma once
#include "../../core/game_object.h"
#include "../collision/contact_cache.h"
#include "../physics_world.h"
#include "physics_units.h"
#include "shape_geometry.h"
#include <box2d/box2d.h>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>

namespace elysia::physics
{
namespace detail
{
enum class ColliderValidationFailure : std::uint8_t
{
    Friction,
    Restitution,
    Density,
    OneWayTolerance,
    Geometry
};

[[nodiscard]] std::optional<ColliderValidationFailure> collider_validation_failure(
    const Collider& collider, float units_per_meter);
} // namespace detail

struct PhysicsWorld::Impl
{
    struct Shape
    {
        b2ShapeId native{};
        PhysicsObjectHandle owner{};
        CollisionTarget target{};
        Collider definition{};
        ChainHandle chain{};
    };
    struct DebugTargetCommand
    {
        PhysicsPose pose_at_command{},requested_pose{},effective_pose{};
        double fixed_delta_seconds=0;
    };
    struct Object
    {
        b2BodyId native{};
        elysia::core::GameObject *owner = nullptr;
        BodyDefinition definition{};
        // The size constructor may propagate MSVC Debug proxy allocation failures.
        std::vector<ColliderId> shapes = std::vector<ColliderId>(std::size_t{0});
        PhysicsPose previous{}, current{};
        bool removed = false;
        bool pending_creation = false;
        std::optional<DebugTargetCommand> pending_debug_target{},completed_debug_target{};
        void clear_debug_target() noexcept {pending_debug_target.reset();completed_debug_target.reset();}
    };
    struct Mapping
    {
        CollisionTarget target{};
        std::uint64_t retire_after = 0;
    };
    struct Snapshot
    {
        CollisionTarget target{};
        std::optional<OneWayCollision> one_way;
        b2AABB bounds{};
        b2AABB previous_bounds{};
        b2Vec2 velocity{};
        unsigned internal_faces = 0; // left, right, top, bottom; frozen Tile adjacency
    };
    struct Chain
    {
        PhysicsObjectHandle owner{};
        ChainDefinition definition;
        b2ChainId native{};
        std::vector<ColliderId> colliders=std::vector<ColliderId>(std::size_t{0});
        std::vector<CollisionTarget> targets=std::vector<CollisionTarget>(std::size_t{0});
        std::vector<b2Vec2> points=std::vector<b2Vec2>(std::size_t{0});
        std::vector<b2ShapeId> native_segments=std::vector<b2ShapeId>(std::size_t{0});
        bool removed=false;
    };
    struct Joint
    {
        b2JointId native{};
        PhysicsObjectHandle first{}, second{};
        std::variant<DistanceJointSettings,RevoluteJointSettings,WeldJointSettings,PrismaticJointSettings,
            WheelJointSettings,MouseJointSettings,MotorJointSettings,std::monostate> settings{};
        bool removed = false;
    };
    PhysicsWorldConfig config;
    detail::PhysicsUnits units;
    b2WorldId world{};
    b2BodyId tile_body{};
    const ITileCollisionWorld *tiles = nullptr;
    std::map<std::uint64_t, Object> objects;
    std::map<ColliderId, Shape> shapes;
    std::map<TileCoordinate, Shape> tile_shapes;
    std::map<std::uint64_t, Joint> joints;
    std::map<std::uint64_t,std::shared_ptr<Chain>> chains;
    std::unordered_map<std::uint64_t, Mapping> mapping;
    std::unordered_map<std::uint64_t, Snapshot> snapshot;
    std::set<CollisionPair> previous_blocking_contacts;
    std::set<CollisionPair> ignored;
    std::set<std::pair<ColliderId, std::uint8_t>> logged_invalid_collider_updates;
    ContactCache cache;
    std::vector<ICollisionListener *> listeners;
    std::vector<std::function<void()>> commands;
    std::uint64_t next_object = 1, next_shape = 1, next_joint = 1, next_chain = 1, epoch = 0;
    double interpolation_alpha = 0.0;
    bool advancing = false, pending_reset = false;
    PhysicsStepStats stats{};
    PhysicsDebugCapture capture = PhysicsDebugCapture::None;
    PhysicsDebugSnapshot debug{};
    PhysicsDebugSnapshot debug_spare{};
    explicit Impl(PhysicsWorldConfig);
    ~Impl();
    b2Vec2 to(elysia::core::Vector2 p) const
    {
        auto v = units.to_length(p);
        return {v.x, v.y};
    }
    elysia::core::Vector2 from(b2Vec2 p) const
    {
        return units.from_length(elysia::core::Vector2{p.x, p.y});
    }
    Object *get(PhysicsObjectHandle h)
    {
        auto i = objects.find(h.value);
        return i == objects.end() || i->second.removed ? nullptr : &i->second;
    }
    Object *raw(PhysicsObjectHandle h)
    {
        auto i = objects.find(h.value);
        return i == objects.end() ? nullptr : &i->second;
    }
    const Object *get(PhysicsObjectHandle h) const
    {
        auto i = objects.find(h.value);
        return i == objects.end() || i->second.removed ? nullptr : &i->second;
    }
    void enqueue(std::function<void()> f)
    {
        if (advancing)
            commands.push_back(std::move(f));
        else
            f();
    }
    template<class Definition> JointHandle create_joint(const Definition&);
    template<class Settings> bool update_joint(JointHandle,const Settings&);
    std::optional<JointState> read_joint_state(JointHandle) const noexcept;
    std::optional<BodyState> read_body_state(PhysicsObjectHandle) const noexcept;
    bool debug_visible(const Object* o) const noexcept;
    WorldColliderShape debug_geometry(b2ShapeId,b2Transform) const;
    void refresh_joint_targets();
    void flush();
    void rollback_registration(std::uint64_t) noexcept;
    void rollback_aborted_registrations() noexcept;
    void clear();
    void create_shape(Shape &, b2BodyId);
    void retire(Shape &);
    void mass(Object &);
    std::optional<b2MassData> resolve_mass(const Object &, const MassProperties &,
        ColliderId replacing = InvalidColliderId, const Collider *replacement = nullptr) const;
    bool bullet(const Object &) const;
    void transform(Object &, PhysicsPose, TeleportVelocityMode);
    void destroy_object(std::uint64_t);
    void build_tiles(TileCoordinate, TileCoordinate);
    void clear_tiles();
    void destroy_chain_record(std::uint64_t,bool invalidate_contacts,bool discard_mapping=false);
    void prepare_snapshot();
    static bool pre_solve(b2ShapeId, b2ShapeId, b2Manifold *, void *);
    void collect(std::vector<CollisionContact> &);
    void capture_debug(PhysicsDebugCapture);
    void query(const b2ShapeProxy &, b2Vec2 translation, const CollisionQueryFilter &, bool sweep,
               bool ray, elysia::core::Vector2 origin, std::vector<CollisionQueryHit> &) const;
    std::optional<CollisionTarget> target(b2ShapeId id) const
    {
        auto it = mapping.find(b2StoreShapeId(id));
        return it == mapping.end() ? std::nullopt : std::optional(it->second.target);
    }
};
} // namespace elysia::physics
