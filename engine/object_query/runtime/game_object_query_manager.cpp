#include "game_object_query_manager.h"

#include "../../tools/logger.h"

namespace elysia::object_query
{
void GameObjectQueryManager::bind_active_runtime(
    IGameObjectQueryRuntime& runtime) noexcept
{
    _active_runtime = &runtime;
}

void GameObjectQueryManager::unbind_active_runtime(
    const IGameObjectQueryRuntime& runtime) noexcept
{
    if (_active_runtime == &runtime)
        _active_runtime = nullptr;
}

bool GameObjectQueryManager::is_available() const noexcept
{
    return _active_runtime != nullptr;
}

void GameObjectQueryManager::visit_game_objects(
    elysia::core::DepthLayerMask layers,
    const GameObjectVisitor& visitor) const
{
    if (!_active_runtime)
    {
        ELYSIA_LOG_WARN(
            "object_query",
            "Game object query failed: there is no active runtime."
        );
        return;
    }

    _active_runtime->visit_game_objects(layers,visitor);
}
}
