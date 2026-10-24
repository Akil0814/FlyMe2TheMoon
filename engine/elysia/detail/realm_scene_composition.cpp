#include "realm_scene_composition.h"

#include "elysia_intro_scene.h"
#include "elysia_realm_scene.h"
#include "realm_scene_keys.h"
#include "../../scene/routing/scene_key.h"
#include "../../scene/scene_manager.h"

namespace elysia::realm::detail
{
void register_realm_scenes(
    elysia::scene::SceneManager& scene_manager)
{
    scene_manager.register_engine_scene<ElysiaIntroScene>(
        elysia::scene::SceneKeys::ElysiaRealm);
    scene_manager.register_engine_scene<ElysiaRealmScene>(
        SceneKeys::RealmContent);
}
}
