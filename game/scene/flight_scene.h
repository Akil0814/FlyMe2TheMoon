#pragma once
#include "engine/scene/scene.h"
#include "game/gameplay/flight_objects.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "game/testing/replay_probe.h"
#include "game/story/story_runtime.h"
#include "engine/ui/widgets/text/ui_text_block.h"
#include "engine/ui/widgets/image/ui_image.h"
#include "engine/audio/sound_playback_types.h"

namespace game::scene_keys {inline constexpr elysia::scene::SceneKey Flight=2;}
namespace game::scene {
struct FlightPayload {int level=1;bool restart=false;};
class FlightScene final : public elysia::scene::Scene {
public:
    FlightScene();
protected:
    void on_enter(const elysia::scene::ScenePayload&) override;
    void on_exit() override;
    void on_reset() override;
    void on_shortcuts(const elysia::input::RawInputFrame&,const std::vector<elysia::input::RawInputEvent>&) override;
    void on_routed_input(const elysia::input::InputSnapshot&) override;
    void on_after_update(double delta) override;
    void on_fixed_update(std::uint64_t tick,double delta)override;
    std::optional<elysia::camera::CameraFocus> resolve_camera_focus(elysia::camera::CameraSlot) const override;
private:
    void finish(bool success);
    void reached(bool success);
    void restore_player();
    content::Json story_action(const std::string&,const content::Json&);
    void update_dialogue();
    int _level=1,_stars=0,_crystals=0,_crystal_count=0;
    double _elapsed=0;
    bool _finished=false,_success=false;
    bool _restart=false,_input_enabled=true,_demo=false,_demo_paused=false,_event_pending=false,_follow_role=true;
    play::Vec _story_camera{};
    content::Json _data;
    play::Player* _player=nullptr;
    std::vector<play::Entity*> _entities;
    elysia::ui::UiLabel* _hud=nullptr;
    elysia::ui::UiLabel* _hint=nullptr;
    testing::ReplayProbe& _probe=testing::ReplayProbe::shared();
    story::Runtime _story;
    content::Json _text;
    std::string _dialogue_key;
    elysia::ui::UiTextBlock* _dialogue=nullptr;
    elysia::ui::UiLabel* _dialogue_panel=nullptr;
    elysia::ui::UiImage* _face=nullptr;
    elysia::ui::UiImage* _tutorial=nullptr;
    std::vector<elysia::audio::SoundHandle> _sounds;
    double _mechanism_time=0;
    std::vector<bool> _triggered_areas;
    std::vector<std::pair<double,int>> _scheduled_actions;
};
}
