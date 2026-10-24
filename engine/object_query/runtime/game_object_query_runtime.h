#pragma once

#include "../game_object_query_types.h"
#include "../../core/depth_layer.h"

namespace elysia::object_query
{
class IGameObjectQueryRuntime
{
public:
    virtual ~IGameObjectQueryRuntime() = default;

    virtual void visit_game_objects(
        elysia::core::DepthLayerMask layers,
        const GameObjectVisitor& visitor) const = 0;
};
}
