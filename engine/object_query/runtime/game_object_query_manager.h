#pragma once

#include "game_object_query_runtime.h"
#include "../../tools/singleton.h"

namespace elysia::scene
{
class SceneManager;
}

namespace elysia::object_query
{
class GameObjectQueryService;

class GameObjectQueryManager final
    : public elysia::tools::Singleton<GameObjectQueryManager>
{
    friend elysia::tools::Singleton<GameObjectQueryManager>;
    friend class GameObjectQueryService;
    friend class elysia::scene::SceneManager;

private:
    GameObjectQueryManager() = default;

    void bind_active_runtime(IGameObjectQueryRuntime& runtime) noexcept;
    void unbind_active_runtime(const IGameObjectQueryRuntime& runtime) noexcept;
    [[nodiscard]] bool is_available() const noexcept;
    void visit_game_objects(
        elysia::core::DepthLayerMask layers,
        const GameObjectVisitor& visitor) const;

    IGameObjectQueryRuntime* _active_runtime = nullptr;
};
}
