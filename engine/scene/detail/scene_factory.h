#pragma once

#include "../routing/scene_key.h"

#include <memory>
#include <unordered_map>

namespace elysia::scene
{
class Scene;

class SceneFactory final
{
public:
    SceneFactory() = default;
    ~SceneFactory();

    SceneFactory(const SceneFactory&) = delete;
    SceneFactory& operator=(const SceneFactory&) = delete;

    [[nodiscard]] Scene* find(SceneKey key) const noexcept;
    // Ownership transfers only after the cache slot has been allocated successfully.
    void store(SceneKey key, std::unique_ptr<Scene>& scene);
    [[nodiscard]] std::unique_ptr<Scene> take(SceneKey key) noexcept;
    bool destroy(SceneKey key);
    bool destroy_all_scene() noexcept;

private:
    std::unordered_map<SceneKey, std::unique_ptr<Scene>> _scene_cache;
};
} // namespace elysia::scene
