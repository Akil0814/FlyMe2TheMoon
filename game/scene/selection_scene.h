#pragma once
#include "engine/scene/scene.h"
#include "game/audio/game_audio.h"
#include "game/frontend/moon_ui.h"
namespace game::scene_keys {inline constexpr elysia::scene::SceneKey Selection=3,Achievements=4;}
namespace game::scene {
struct SelectionPayload {int chapter=0;};
struct AchievementsPayload {int page=0;};
class SelectionScene final:public elysia::scene::Scene {
    game::audio::World _audio;
    std::vector<game::frontend::LevelNode*> _unlock_nodes;
    double _unlock_time=0;
protected:
    void on_enter(const elysia::scene::ScenePayload&)override;
    void on_exit()override{_audio.clear();_unlock_nodes.clear();clear_scene_objects();}
    void on_reset()override{on_exit();}
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
