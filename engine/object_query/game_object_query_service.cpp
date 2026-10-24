#include "game_object_query_service.h"

#include "runtime/game_object_query_manager.h"

namespace elysia::object_query
{
bool GameObjectQueryService::is_available() const noexcept
{
    return GameObjectQueryManager::instance()->is_available();
}

void GameObjectQueryService::visit_game_objects(
    elysia::core::DepthLayerMask layers,
    const GameObjectVisitor& visitor) const
{
    GameObjectQueryManager::instance()->visit_game_objects(layers,visitor);
}
}
