#pragma once
#include "application_presentation_settings.h"
#include "../scene/routing/scene_route.h"
#include "../tools/development_overlay.h"

#include <memory>

namespace elysia::scene
{
class SceneManager;
}

namespace elysia::application
{
struct ApplicationDescriptor
{
    int logical_width = 1280;
    int logical_height = 720;
    elysia::scene::SceneRoute initial_route{};
    ApplicationPresentationSettings presentation{};
};

class IGameModule
{
public:
    virtual ~IGameModule() = default;

    [[nodiscard]] virtual ApplicationDescriptor descriptor() const = 0;
    virtual void register_scenes(
        elysia::scene::SceneManager& scene_manager) const = 0;
    [[nodiscard]] virtual std::unique_ptr<elysia::tools::IDevelopmentOverlay>
        create_development_overlay() const
    {
        return {};
    }
};
}
