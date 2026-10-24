#include "gameplay_collision_service.h"

#include "../../tools/logger.h"

namespace elysia::gameplay::collision
{
bool GameplayCollisionService::attach_runtime(IGameplayCollisionRuntime& runtime) noexcept
{
    if (!_active_runtime)
    {
        _active_runtime = &runtime;
        return true;
    }

    if (_active_runtime == &runtime)
        return true;

    ELYSIA_LOG_ERROR(
        "collision",
        "Attach gameplay collision runtime failed: another runtime is already active."
    );
    return false;
}

bool GameplayCollisionService::detach_runtime(const IGameplayCollisionRuntime& runtime) noexcept
{
    if (!_active_runtime)
        return false;

    if (_active_runtime != &runtime)
    {
        ELYSIA_LOG_ERROR(
            "collision",
            "Detach gameplay collision runtime failed: runtime is not active."
        );
        return false;
    }

    _active_runtime = nullptr;
    return true;
}

bool GameplayCollisionService::has_active_runtime() const noexcept
{
    return _active_runtime != nullptr;
}

bool GameplayCollisionService::bind_actor(const ActorCollisionRig& rig)
{
    IGameplayCollisionRuntime* runtime = runtime_or_log("bind actor");
    return runtime && runtime->bind_actor(rig);
}

bool GameplayCollisionService::bind_collider(const ColliderBinding& binding)
{
    IGameplayCollisionRuntime* runtime = runtime_or_log("bind collider");
    return runtime && runtime->bind_collider(binding);
}

bool GameplayCollisionService::bind_hit_box(const HitBoxBinding& binding)
{
    IGameplayCollisionRuntime* runtime = runtime_or_log("bind hit box");
    return runtime && runtime->bind_hit_box(binding);
}

bool GameplayCollisionService::unbind_collider(elysia::physics::ColliderId collider)
{
    IGameplayCollisionRuntime* runtime = runtime_or_log("unbind collider");
    return runtime && runtime->unbind_collider(collider);
}

bool GameplayCollisionService::request_drop_through(const DropThroughRequest& request)
{
    if (request.actor == elysia::physics::InvalidColliderId
        || !request.target.is_valid()
        || (request.target.kind == elysia::physics::CollisionTargetKind::Collider
            && request.actor == request.target.collider))
    {
        ELYSIA_LOG_ERROR(
            "collision",
            "Drop-through request failed: actor and target must be valid and cannot identify the same collider."
        );
        return false;
    }

    IGameplayCollisionRuntime* runtime = runtime_or_log("request drop-through");
    return runtime && runtime->request_drop_through(request);
}

bool GameplayCollisionService::unbind_actor(ActorId actor)
{
    IGameplayCollisionRuntime* runtime = runtime_or_log("unbind actor");
    return runtime && runtime->unbind_actor(actor);
}

bool GameplayCollisionService::add_listener(GameplayCollisionListener& listener)
{
    IGameplayCollisionRuntime* runtime = runtime_or_log("add listener");
    return runtime && runtime->add_listener(listener);
}

bool GameplayCollisionService::remove_listener(
    const GameplayCollisionListener& listener)
{
    IGameplayCollisionRuntime* runtime = runtime_or_log("remove listener");
    return runtime && runtime->remove_listener(listener);
}

void GameplayCollisionService::end_attack_instance(AttackInstanceId attack_instance)
{
    if (attack_instance == InvalidAttackInstanceId)
        return;
    IGameplayCollisionRuntime* runtime = runtime_or_log("end attack instance");
    if (runtime)
        runtime->end_attack_instance(attack_instance);
}

IGameplayCollisionRuntime* GameplayCollisionService::runtime_or_log(
    std::string_view operation
) const noexcept
{
    if (_active_runtime)
        return _active_runtime;

    ELYSIA_LOG_ERROR(
        "collision",
        "Gameplay collision operation failed without an active runtime: " << operation
    );
    return nullptr;
}
}
