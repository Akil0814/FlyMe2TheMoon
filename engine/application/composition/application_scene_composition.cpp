#include "application_scene_composition.h"

#include "../../builtin/builtin_scene_keys.h"
#include "../../builtin/scenes/settings_scene.h"
#include "../../builtin/scenes/application_failure_scene.h"
#include "../../builtin/scenes/startup_loading_scene.h"
#include "../../elysia/detail/realm_scene_composition.h"
#include "../../scene/scene_manager.h"

namespace elysia::application
{
ApplicationDescriptor describe_game_module(const IGameModule& game_module)
{
    return game_module.descriptor();
}

void compose_application_scenes(
    elysia::scene::SceneManager& scene_manager,
    const IGameModule& game_module,
    const ApplicationDescriptor& descriptor)
{
    scene_manager.register_engine_scene<
        elysia::builtin::StartupLoadingScene>(
            elysia::builtin::SceneKeys::StartupLoading);
    scene_manager.register_engine_scene<
        elysia::builtin::SettingsScene>(
            elysia::builtin::SceneKeys::Settings);
    scene_manager.register_engine_scene<
        elysia::builtin::ApplicationFailureScene>(
            elysia::builtin::SceneKeys::ApplicationFailure);
    elysia::realm::detail::register_realm_scenes(
        scene_manager);
    game_module.register_scenes(
        scene_manager);
    scene_manager.start(descriptor.initial_route);
}
}
