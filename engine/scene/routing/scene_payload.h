#pragma once
#include <any>

namespace elysia::scene
{
using ScenePayload = std::any;

template <typename T>
[[nodiscard]] const T* try_scene_payload(const ScenePayload& payload) noexcept
{
	return std::any_cast<T>(&payload);
}

}
