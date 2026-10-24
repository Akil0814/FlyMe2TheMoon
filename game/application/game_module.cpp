
#include "game_module.h"
#include "../scene/main_menu_scene.h"

#include "../../engine/builtin/builtin_scene_keys.h"
#include "../../engine/builtin/scenes/startup_loading_scene.h"
#include "../../engine/scene/scene_manager.h"
#if ELYSIA_ENABLE_IMGUI
#include "../../engine/tools/imgui/imgui_development_overlay.h"
#endif

namespace game::application
{
    elysia::application::ApplicationDescriptor GameModule::descriptor() const
    {
        using elysia::scene::SceneReloadMode;
        using elysia::scene::SceneRoute;
        using elysia::builtin::StartupLoadingScenePayload;

        elysia::application::ApplicationDescriptor descriptor;
        descriptor.logical_width = 1280;
        descriptor.logical_height = 720;
        descriptor.presentation.render.texture_filter =
            elysia::application::ApplicationTextureFilter::Linear;
        descriptor.presentation.ui.default_theme =
            elysia::ui::UiBuiltinTheme::BlueGlassMoon;
        descriptor.presentation.startup.engine_logo =
            elysia::application::ApplicationEngineLogoVariant::White;
        descriptor.presentation.fonts.ui.source =
            elysia::typography::FontSource::EngineBuiltIn;
        descriptor.presentation.fonts.floating_number.source =
            elysia::typography::FontSource::EngineBuiltIn;
        descriptor.initial_route = SceneRoute{
            .target = elysia::builtin::SceneKeys::StartupLoading,
            .payload = StartupLoadingScenePayload{
                .success_route = SceneRoute{
                    .target = game::scene_keys::MainMenu,
                    .reload_mode = SceneReloadMode::Reuse
                },
                .failure_route = std::nullopt,
                .project_logo = std::nullopt,
                .wait_for_logo_sequence = false,
                .wait_for_confirmation = true
            },
            .reload_mode = SceneReloadMode::Reuse
        };

        return descriptor;
    }

    void GameModule::register_scenes(elysia::scene::SceneManager& scene_manager) const
    {
        scene_manager.register_game_scene<game::scene::MainMenuScene>(game::scene_keys::MainMenu);
    }

    std::unique_ptr<elysia::tools::IDevelopmentOverlay>
        GameModule::create_development_overlay() const
    {
#if ELYSIA_ENABLE_IMGUI
        return std::make_unique<elysia::tools::ImGuiDevelopmentOverlay>();
#else
        return {};
#endif
    }

}
