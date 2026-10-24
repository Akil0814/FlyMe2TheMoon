#pragma once

#include "actor_collision_rig.h"
#include "gameplay_collision_listener.h"
#include "gameplay_collision_types.h"
#include "team_relation_resolver.h"

#include "../../physics/contracts/collision_listener.h"
#include "../../physics/physics_world.h"

#include <unordered_map>
#include <map>
#include <unordered_set>
#include <vector>

namespace elysia::gameplay::collision
{
class IGameplayCollisionRuntime
{
public:
    virtual ~IGameplayCollisionRuntime() = default;

    [[nodiscard]] virtual bool bind_actor(const ActorCollisionRig& rig) = 0;
    [[nodiscard]] virtual bool bind_collider(const ColliderBinding& binding) = 0;
    [[nodiscard]] virtual bool bind_hit_box(const HitBoxBinding& binding) = 0;
    [[nodiscard]] virtual bool unbind_actor(ActorId actor) = 0;
    [[nodiscard]] virtual bool unbind_collider(elysia::physics::ColliderId collider) = 0;
    [[nodiscard]] virtual bool request_drop_through(const DropThroughRequest& request) = 0;
    [[nodiscard]] virtual bool add_listener(GameplayCollisionListener& listener) = 0;
    [[nodiscard]] virtual bool remove_listener(const GameplayCollisionListener& listener) = 0;
    virtual void end_attack_instance(AttackInstanceId attack_instance) = 0;
};

class GameplayCollisionRuntime final
    : public IGameplayCollisionRuntime
    , private elysia::physics::ICollisionListener
{
public:
    explicit GameplayCollisionRuntime(elysia::physics::PhysicsWorld& world);
    ~GameplayCollisionRuntime() override;

    GameplayCollisionRuntime(const GameplayCollisionRuntime&) = delete;
    GameplayCollisionRuntime& operator=(const GameplayCollisionRuntime&) = delete;

    [[nodiscard]] bool bind_actor(const ActorCollisionRig& rig) override;
    [[nodiscard]] bool bind_collider(const ColliderBinding& binding) override;
    [[nodiscard]] bool bind_hit_box(const HitBoxBinding& binding) override;
    [[nodiscard]] bool unbind_actor(ActorId actor) override;
    [[nodiscard]] bool unbind_collider(elysia::physics::ColliderId collider) override;
    [[nodiscard]] bool request_drop_through(const DropThroughRequest& request) override;
    [[nodiscard]] bool add_listener(GameplayCollisionListener& listener) override;
    [[nodiscard]] bool remove_listener(const GameplayCollisionListener& listener) override;
    void end_attack_instance(AttackInstanceId attack_instance) override;

    void set_team_relation_resolver(const TeamRelationResolver* resolver) noexcept;
    [[nodiscard]] TeamRelation team_relation(TeamId source, TeamId target) const noexcept;
    void clear() noexcept;

private:
    struct ContactBindings
    {
        std::optional<ColliderBinding> first;
        std::optional<ColliderBinding> second;
    };
    // End may arrive after an actor has unbound or been destroyed. Keep only
    // value-semantic routing metadata for the lifetime of the core contact.
    std::map<elysia::physics::CollisionPair, ContactBindings> _contact_bindings;
    struct AttackHitKey
    {
        AttackInstanceId attack = InvalidAttackInstanceId;
        ActorId hurt_owner = InvalidActorId;
        [[nodiscard]] bool operator==(const AttackHitKey&) const noexcept = default;
    };

    struct AttackHitKeyHash
    {
        [[nodiscard]] std::size_t operator()(const AttackHitKey& key) const noexcept;
    };

    void on_collision_event(const elysia::physics::CollisionEvent& event) override;
    void flush_listener_operations();

    elysia::physics::PhysicsWorld* _world = nullptr;
    std::unordered_map<ActorId, ActorCollisionRig> _rigs;
    std::unordered_map<elysia::physics::ColliderId, ColliderBinding> _bindings;
    std::unordered_map<elysia::physics::ColliderId, HitBoxBinding> _hit_boxes;
    std::unordered_set<AttackHitKey, AttackHitKeyHash> _attack_hits;
    std::vector<GameplayCollisionListener*> _listeners;
    std::vector<std::pair<GameplayCollisionListener*, bool>> _pending_listener_operations;
    const TeamRelationResolver* _team_resolver = nullptr;
    bool _dispatching = false;
};
}
