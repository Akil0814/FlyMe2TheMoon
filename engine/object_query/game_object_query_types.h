#pragma once

#include <functional>

namespace elysia::core
{
class GameObject;
}

namespace elysia::object_query
{
using GameObjectVisitor = std::function<bool(elysia::core::GameObject&)>;
}
