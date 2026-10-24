#pragma once

#include "gameplay_collision_runtime.h"

#include "../../tools/singleton.h"

#include <string_view>

#define ELYSIA_GAMEPLYA_COLLISION (::elysia::gameplay::collision::GameplayCollisionService::instance())

namespace elysia::gameplay::collision
{
class GameplayCollisionService final
    : public elysia::tools::Singleton<GameplayCollisionService>
{
    friend elysia::tools::Singleton<GameplayCollisionService>;

public:
    [[nodiscard]] bool attach_runtime(IGameplayCollisionRuntime& runtime) noexcept;
    [[nodiscard]] bool detach_runtime(const IGameplayCollisionRuntime& runtime) noexcept;
    [[nodiscard]] bool has_active_runtime() const noexcept;

    [[nodiscard]] bool bind_actor(const ActorCollisionRig& rig);
    [[nodiscard]] bool bind_collider(const ColliderBinding& binding);
    [[nodiscard]] bool bind_hit_box(const HitBoxBinding& binding);
    [[nodiscard]] bool unbind_actor(ActorId actor);
    [[nodiscard]] bool unbind_collider(elysia::physics::ColliderId collider);
    [[nodiscard]] bool request_drop_through(const DropThroughRequest& request);
    [[nodiscard]] bool add_listener(GameplayCollisionListener& listener);
    [[nodiscard]] bool remove_listener(const GameplayCollisionListener& listener);
    void end_attack_instance(AttackInstanceId attack_instance);

private:
    GameplayCollisionService() = default;

    [[nodiscard]] IGameplayCollisionRuntime* runtime_or_log(std::string_view operation) const noexcept;

    IGameplayCollisionRuntime* _active_runtime = nullptr;
};
}
