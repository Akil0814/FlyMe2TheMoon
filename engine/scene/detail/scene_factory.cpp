#include "scene_factory.h"

#include "../scene.h"
#include "scene_failure_boundary.h"
#include "scene_failure_log.h"

#include <stdexcept>

namespace elysia::scene
{
SceneFactory::~SceneFactory()
{
    (void)destroy_all_scene();
}

Scene* SceneFactory::find(SceneKey key) const noexcept
{
    const auto found = _scene_cache.find(key);
    return found == _scene_cache.end() ? nullptr : found->second.get();
}

void SceneFactory::store(SceneKey key, std::unique_ptr<Scene>& scene)
{
    if (!scene)
        throw std::invalid_argument("SceneFactory cannot store a null scene.");
    const auto [slot, inserted] = _scene_cache.try_emplace(key);
    if (!inserted)
        throw std::logic_error("SceneFactory already contains the SceneKey.");
    slot->second = std::move(scene);
}

std::unique_ptr<Scene> SceneFactory::take(SceneKey key) noexcept
{
    const auto found = _scene_cache.find(key);
    if (found == _scene_cache.end())
        return {};
    auto scene = std::move(found->second);
    _scene_cache.erase(found);
    return scene;
}

bool SceneFactory::destroy(SceneKey key)
{
    auto scene = take(key);
    if (!scene)
        return false;
    scene->prepare_for_destruction();
    return true;
}

bool SceneFactory::destroy_all_scene() noexcept
{
    bool succeeded = true;
    for (auto& [key, scene] : _scene_cache)
    {
        (void)key;
        if (!scene)
            continue;
        try
        {
            scene->prepare_for_destruction();
        }
        catch (...)
        {
            succeeded = false;
            try
            {
                detail::SceneFailureCollector failures;
                failures.capture(key,SceneBoundary::ObjectRemoval,"Shutdown scene destruction");
                if (auto result = failures.finish(); !result) detail::log_scene_failure(result.error());
            }
            catch (const elysia::core::RenderBackendError& error)
            {
                elysia::tools::Logger::instance()->log_stream(elysia::tools::LogLevel::Error,"scene_shutdown",
                    [&](std::ostream& output) {
                        output << elysia::core::format_failure_diagnostic(error.failure().diagnostic,"RENDER-BACKEND","render");
                    },error.failure().diagnostic.origin);
            }
            catch (...) { detail::log_cleanup_exception("Shutdown scene diagnostic"); }
        }
    }
    _scene_cache.clear();
    return succeeded;
}
} // namespace elysia::scene
