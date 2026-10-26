
#include "game_module.h"
#include "../scene/main_menu_scene.h"
#include "../scene/flight_scene.h"
#include "../scene/selection_scene.h"
#include "../scene/credits_scene.h"
#include "../scene/settings_scene.h"
#include "../testing/replay_probe.h"

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
            elysia::application::ApplicationEngineLogoVariant::LightEdge;
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

        if(const int level=game::testing::ReplayProbe::requested_level();level!=0) {
            descriptor.initial_route.payload=StartupLoadingScenePayload{
                .success_route=SceneRoute{.target=level>0?game::scene_keys::Flight:game::scene_keys::MainMenu,.payload=game::scene::FlightPayload{std::max(1,level)},.reload_mode=SceneReloadMode::Recreate},
                .wait_for_logo_sequence=true,.wait_for_confirmation=false};
        }
        return descriptor;
    }

    void GameModule::register_scenes(elysia::scene::SceneManager& scene_manager) const
    {
        scene_manager.register_game_scene<game::scene::MainMenuScene>(game::scene_keys::MainMenu);
        scene_manager.register_game_scene<game::scene::FlightScene>(game::scene_keys::Flight);
        scene_manager.register_game_scene<game::scene::SelectionScene>(game::scene_keys::Selection);
        scene_manager.register_game_scene<game::scene::CreditsScene>(game::scene_keys::Credits);
        scene_manager.register_game_scene<game::scene::AchievementsScene>(game::scene_keys::Achievements);
        scene_manager.register_game_scene<game::scene::SettingsScene>(game::scene_keys::Settings);
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
