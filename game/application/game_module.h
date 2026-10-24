#pragma once

#include "engine/application/game_module.h"

namespace game::application
{
    class GameModule final : public elysia::application::IGameModule
    {
    public:
        [[nodiscard]] elysia::application::ApplicationDescriptor descriptor() const override;

        void register_scenes(elysia::scene::SceneManager& scene_manager) const override;

        [[nodiscard]] std::unique_ptr<elysia::tools::IDevelopmentOverlay>create_development_overlay() const override;
    };
}