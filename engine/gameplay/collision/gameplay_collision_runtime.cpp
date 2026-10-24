#include "gameplay_collision_runtime.h"
#include "../../tools/logger.h"

#include <algorithm>
#include <exception>
#include <optional>

namespace elysia::gameplay::collision
{
namespace
{
[[nodiscard]] TeamRelation default_relation(TeamId source, TeamId target) noexcept
{
    if (source != InvalidTeamId && source == target)
        return TeamRelation::Friendly;
    if (source == teams::Neutral || target == teams::Neutral
        || source == InvalidTeamId || target == InvalidTeamId)
    {
        return TeamRelation::Neutral;
    }
    return TeamRelation::Hostile;
}

[[nodiscard]] bool valid_binding(
    const ColliderBinding& binding,
    const elysia::physics::PhysicsWorld& world) noexcept
{
    return binding.collider != elysia::physics::InvalidColliderId
        && binding.owner != InvalidActorId
        && binding.team != InvalidTeamId
        && world.contains_collider(binding.collider);
}

[[nodiscard]] bool valid_role(ColliderRole role) noexcept
{
    switch (role)
    {
    case ColliderRole::Body:
    case ColliderRole::PushBox:
    case ColliderRole::HurtBox:
    case ColliderRole::HitBox:
    case ColliderRole::Sensor:
        return true;
    }
    return false;
}

[[nodiscard]] bool empty_rig(const ActorCollisionRig& rig) noexcept
{
    return rig.body == elysia::physics::InvalidColliderId
        && rig.push_box == elysia::physics::InvalidColliderId
        && rig.hurt_boxes.empty()
        && rig.sensors.empty();
}
}

GameplayCollisionRuntime::GameplayCollisionRuntime(elysia::physics::PhysicsWorld& world)
    : _world(&world)
{
    if (!_world->add_listener(*this))
        _world = nullptr;
}

GameplayCollisionRuntime::~GameplayCollisionRuntime()
{
    if (_world)
        (void)_world->remove_listener(*this);
}

std::size_t GameplayCollisionRuntime::AttackHitKeyHash::operator()(
    const AttackHitKey& key) const noexcept
{
    return std::hash<AttackInstanceId>{}(key.attack)
        ^ (std::hash<ActorId>{}(key.hurt_owner) << 1u);
}

bool GameplayCollisionRuntime::bind_actor(const ActorCollisionRig& rig)
{
    if (!_world || rig.owner == InvalidActorId || rig.team == InvalidTeamId
        || _rigs.contains(rig.owner))
        return false;
    std::vector<ColliderBinding> additions;
    const auto add = [&](elysia::physics::ColliderId id, ColliderRole role)
    {
        if (id != elysia::physics::InvalidColliderId)
            additions.push_back({id, rig.owner, rig.team, role});
    };
    add(rig.body, ColliderRole::Body);
    add(rig.push_box, ColliderRole::PushBox);
    for (const auto id : rig.hurt_boxes) add(id, ColliderRole::HurtBox);
    for (const auto id : rig.sensors) add(id, ColliderRole::Sensor);
    std::ranges::sort(additions, {}, &ColliderBinding::collider);
    if (additions.empty()
        || std::adjacent_find(additions.begin(), additions.end(),
            [](const auto& a, const auto& b) { return a.collider == b.collider; })
        != additions.end())
    {
        return false;
    }
    for (const ColliderBinding& binding : additions)
        if (!valid_binding(binding, *_world) || _bindings.contains(binding.collider))
            return false;
    bool rig_inserted = false;
    std::vector<elysia::physics::ColliderId> inserted_bindings;
    inserted_bindings.reserve(additions.size());
    try
    {
        rig_inserted = _rigs.emplace(rig.owner, rig).second;
        if (!rig_inserted)
            return false;
        for (const ColliderBinding& binding : additions)
        {
            if (!_bindings.emplace(binding.collider, binding).second)
            {
                for (const auto collider : inserted_bindings)
                    _bindings.erase(collider);
                _rigs.erase(rig.owner);
                return false;
            }
            inserted_bindings.push_back(binding.collider);
        }
    }
    catch (...)
    {
        for (const auto collider : inserted_bindings)
            _bindings.erase(collider);
        if (rig_inserted)
            _rigs.erase(rig.owner);
        throw;
    }
    return true;
}

bool GameplayCollisionRuntime::bind_collider(const ColliderBinding& binding)
{
    if (!_world || !valid_binding(binding, *_world)
        || !valid_role(binding.role) || binding.role == ColliderRole::HitBox)
        return false;
    const auto found = _bindings.find(binding.collider);
    if (found != _bindings.end())
        return found->second.collider == binding.collider
            && found->second.owner == binding.owner
            && found->second.team == binding.team
            && found->second.role == binding.role;
    _bindings.emplace(binding.collider, binding);
    return true;
}

bool GameplayCollisionRuntime::bind_hit_box(const HitBoxBinding& binding)
{
    if (!_world || binding.collider.role != ColliderRole::HitBox
        || !valid_binding(binding.collider, *_world)
        || binding.instigator == InvalidActorId
        || binding.attack_instance == InvalidAttackInstanceId
        || binding.attack_definition == InvalidAttackDefinitionId
        || _bindings.contains(binding.collider.collider)
        || _hit_boxes.contains(binding.collider.collider))
    {
        return false;
    }
    _bindings.emplace(binding.collider.collider, binding.collider);
    try
    {
        _hit_boxes.emplace(binding.collider.collider, binding);
    }
    catch (...)
    {
        _bindings.erase(binding.collider.collider);
        throw;
    }
    return true;
}

bool GameplayCollisionRuntime::unbind_actor(ActorId actor)
{
    if (actor == InvalidActorId)
        return false;
    const bool had_rig = _rigs.erase(actor) > 0;
    bool removed_binding = false;
    for (auto iterator = _bindings.begin(); iterator != _bindings.end();)
    {
        if (iterator->second.owner != actor)
        {
            ++iterator;
            continue;
        }
        _hit_boxes.erase(iterator->first);
        iterator = _bindings.erase(iterator);
        removed_binding = true;
    }
    std::erase_if(_attack_hits, [actor](const AttackHitKey& key)
    { return key.hurt_owner == actor; });
    return had_rig || removed_binding;
}

bool GameplayCollisionRuntime::unbind_collider(elysia::physics::ColliderId collider)
{
    if (collider == elysia::physics::InvalidColliderId)
        return false;
    const auto binding = _bindings.find(collider);
    if (binding == _bindings.end())
        return false;
    const ActorId owner = binding->second.owner;
    _bindings.erase(binding);
    _hit_boxes.erase(collider);
    const auto rig = _rigs.find(owner);
    if (rig != _rigs.end())
    {
        if (rig->second.body == collider)
            rig->second.body = elysia::physics::InvalidColliderId;
        if (rig->second.push_box == collider)
            rig->second.push_box = elysia::physics::InvalidColliderId;
        std::erase(rig->second.hurt_boxes, collider);
        std::erase(rig->second.sensors, collider);
        if (empty_rig(rig->second))
        {
            _rigs.erase(rig);
            std::erase_if(_attack_hits, [owner](const AttackHitKey& key)
            { return key.hurt_owner == owner; });
        }
    }
    return true;
}

bool GameplayCollisionRuntime::request_drop_through(const DropThroughRequest& request)
{
    return _world && _world->request_pass_through(request.actor, request.target);
}

bool GameplayCollisionRuntime::add_listener(GameplayCollisionListener& listener)
{
    if (_dispatching)
    {
        _pending_listener_operations.emplace_back(&listener, true);
        return true;
    }
    if (std::ranges::find(_listeners, &listener) == _listeners.end())
        _listeners.push_back(&listener);
    return true;
}

bool GameplayCollisionRuntime::remove_listener(const GameplayCollisionListener& listener)
{
    if (_dispatching)
    {
        _pending_listener_operations.emplace_back(
            const_cast<GameplayCollisionListener*>(&listener), false);
        return true;
    }
    const auto found = std::ranges::find(_listeners, &listener);
    if (found == _listeners.end())
        return false;
    _listeners.erase(found);
    return true;
}

void GameplayCollisionRuntime::end_attack_instance(AttackInstanceId attack_instance)
{
    if (attack_instance == InvalidAttackInstanceId)
        return;
    std::erase_if(_attack_hits, [attack_instance](const AttackHitKey& key)
    { return key.attack == attack_instance; });
    std::vector<elysia::physics::ColliderId> ended_hit_boxes;
    for (const auto& [collider, binding] : _hit_boxes)
        if (binding.attack_instance == attack_instance)
            ended_hit_boxes.push_back(collider);
    for (const auto collider : ended_hit_boxes)
    {
        _hit_boxes.erase(collider);
        _bindings.erase(collider);
    }
}

void GameplayCollisionRuntime::set_team_relation_resolver(
    const TeamRelationResolver* resolver) noexcept
{
    _team_resolver = resolver;
}

TeamRelation GameplayCollisionRuntime::team_relation(TeamId source, TeamId target) const noexcept
{
    if (source == InvalidTeamId || target == InvalidTeamId)
        return TeamRelation::Neutral;
    return _team_resolver ? _team_resolver->relation(source, target)
        : default_relation(source, target);
}

void GameplayCollisionRuntime::clear() noexcept
{
    _contact_bindings.clear();
    _rigs.clear();
    _bindings.clear();
    _hit_boxes.clear();
    _attack_hits.clear();
    _listeners.clear();
    _pending_listener_operations.clear();
    _team_resolver = nullptr;
}

void GameplayCollisionRuntime::on_collision_event(
    const elysia::physics::CollisionEvent& event)
{
    const auto first = event.contact.pair.first;
    const auto second = event.contact.pair.second;
    const auto binding_value = [&](elysia::physics::CollisionTarget target)
        -> std::optional<ColliderBinding>
    {
        if (target.kind != elysia::physics::CollisionTargetKind::Collider)
            return std::nullopt;
        const auto found = _bindings.find(target.collider);
        return found == _bindings.end()
            ? std::nullopt : std::optional<ColliderBinding>{found->second};
    };
    auto first_binding = binding_value(first);
    auto second_binding = binding_value(second);
    if (event.phase == elysia::physics::CollisionEventPhase::End)
    {
        if (const auto found = _contact_bindings.find(event.contact.pair); found != _contact_bindings.end())
        {
            first_binding = found->second.first;
            second_binding = found->second.second;
            _contact_bindings.erase(found);
        }
    }
    else if (first_binding || second_binding)
    {
        // Do not overwrite a previously known side merely because it unbound.
        auto& cached = _contact_bindings[event.contact.pair];
        if (first_binding) cached.first = first_binding;
        if (second_binding) cached.second = second_binding;
    }

    std::vector<BodyContactEvent> body_events;
    std::vector<PushBoxOverlapEvent> push_events;
    std::vector<SensorOverlapEvent> sensor_events;
    std::vector<HitOverlapEvent> hit_events;

    if (first_binding && first_binding->role == ColliderRole::Body)
        body_events.push_back({event.phase, *first_binding, second, event.contact});
    if (second_binding && second_binding->role == ColliderRole::Body)
        body_events.push_back({event.phase, *second_binding, first, event.contact});

    if (first_binding && second_binding
        && first_binding->role == ColliderRole::PushBox
        && second_binding->role == ColliderRole::PushBox)
    {
        push_events.push_back({
            event.phase, *first_binding, *second_binding,
            {event.contact.pair, event.contact.manifold}});
    }

    if (event.contact.response == elysia::physics::CollisionResponse::Overlap
        && first_binding && second_binding)
    {
        const auto route_sensor = [&](const ColliderBinding& sensor,
                                      const ColliderBinding& body)
        {
            if (sensor.role == ColliderRole::Sensor && body.role == ColliderRole::Body)
                sensor_events.push_back({
                    event.phase, sensor, body,
                    {event.contact.pair, event.contact.manifold}});
        };
        route_sensor(*first_binding, *second_binding);
        route_sensor(*second_binding, *first_binding);
    }

    if (event.phase == elysia::physics::CollisionEventPhase::Begin
        && first_binding && second_binding)
    {
        const auto route_hit = [&](const ColliderBinding& hit_binding,
                                   const ColliderBinding& hurt_binding)
        {
            if (hit_binding.role != ColliderRole::HitBox
                || hurt_binding.role != ColliderRole::HurtBox)
                return;
            const auto hit = _hit_boxes.find(hit_binding.collider);
            if (hit == _hit_boxes.end())
                return;
            const HitBoxBinding hit_value = hit->second;
            const auto instigator = _rigs.find(hit_value.instigator);
            if (instigator == _rigs.end() || instigator->second.team == InvalidTeamId
                || team_relation(instigator->second.team, hurt_binding.team)
                    != TeamRelation::Hostile)
                return;
            const AttackHitKey key{hit_value.attack_instance, hurt_binding.owner};
            if (!_attack_hits.insert(key).second)
                return;
            hit_events.push_back({
                event.phase, hit_value, hurt_binding,
                {event.contact.pair, event.contact.manifold}});
        };
        route_hit(*first_binding, *second_binding);
        route_hit(*second_binding, *first_binding);
    }

    const auto listener_snapshot = _listeners;
    _dispatching = true;
    try
    {
        for (const BodyContactEvent& routed : body_events)
            for (GameplayCollisionListener* listener : listener_snapshot)
                if (listener) listener->on_body_contact(routed);
        for (const PushBoxOverlapEvent& routed : push_events)
            for (GameplayCollisionListener* listener : listener_snapshot)
                if (listener) listener->on_push_box_overlap(routed);
        for (const SensorOverlapEvent& routed : sensor_events)
            for (GameplayCollisionListener* listener : listener_snapshot)
                if (listener) listener->on_sensor_overlap(routed);
        for (const HitOverlapEvent& routed : hit_events)
            for (GameplayCollisionListener* listener : listener_snapshot)
                if (listener) listener->on_hit_overlap(routed);
    }
    catch (...)
    {
        const auto original = std::current_exception();
        _dispatching = false;
        try { flush_listener_operations(); }
        catch (const std::exception& error)
        {
            elysia::tools::Logger::instance()->error("collision_cleanup", error.what());
        }
        catch (...)
        {
            elysia::tools::Logger::instance()->error("collision_cleanup", "Unknown listener flush failure");
        }
        std::rethrow_exception(original);
    }
    _dispatching = false;
    flush_listener_operations();
}

void GameplayCollisionRuntime::flush_listener_operations()
{
    if (_pending_listener_operations.empty())
        return;
    auto pending = std::move(_pending_listener_operations);
    _pending_listener_operations.clear();
    auto next = _listeners;
    for (const auto& [listener, add] : pending)
    {
        const auto found = std::ranges::find(next, listener);
        if (add && found == next.end())
            next.push_back(listener);
        else if (!add && found != next.end())
            next.erase(found);
    }
    _listeners.swap(next);
}
}
