#pragma once
#include "engine/scene/scene.h"
namespace game::scene_keys {inline constexpr elysia::scene::SceneKey Selection=3,Achievements=4;}
namespace game::scene {
struct SelectionPayload {int chapter=0;};
struct AchievementsPayload {int page=0;};
class SelectionScene final:public elysia::scene::Scene {
protected:
    void on_enter(const elysia::scene::ScenePayload&)override;
    void on_exit()override{clear_scene_objects();}
    void on_reset()override{clear_scene_objects();}
    void on_after_update(double)override;
};
class AchievementsScene final:public elysia::scene::Scene {
    int _page=0;
protected:
    void on_enter(const elysia::scene::ScenePayload&)override;
    void on_exit()override{clear_scene_objects();}
    void on_reset()override{clear_scene_objects();}
    void on_after_update(double)override;
};
}
