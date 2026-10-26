#pragma once
#include "engine/scene/scene.h"
#include "game/gameplay/flight_objects.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "game/testing/replay_probe.h"
#include "game/story/story_runtime.h"
#include "engine/ui/widgets/text/ui_text_block.h"
#include "engine/ui/widgets/image/ui_image.h"
#include "engine/audio/sound_playback_types.h"
#include "game/gameplay/tutorial_replay.h"
#include "game/gameplay/projectile.h"
#include "game/gameplay/camera_move.h"
#include "engine/ui/window/ui_window.h"
#include "engine/ui/widgets/ui_button.h"
#include "game/frontend/moon_ui.h"
#include "engine/tools/development_overlay.h"

namespace game::scene_keys {inline constexpr elysia::scene::SceneKey Flight=2;}
namespace game::scene {
struct FlightPayload {int level=1;bool restart=false;};
class FlightScene final : public elysia::scene::Scene {
public:
    FlightScene();
    ~FlightScene() override;
protected:
    void on_enter(const elysia::scene::ScenePayload&) override;
    void on_exit() override;
    void on_reset() override;
    void on_shortcuts(const elysia::input::RawInputFrame&,const std::vector<elysia::input::RawInputEvent>&) override;
    void on_routed_input(const elysia::input::InputSnapshot&) override;
    void on_before_update(double delta) override;
    void on_after_update(double delta) override;
    void on_fixed_update(std::uint64_t tick,double delta)override;
    std::optional<elysia::camera::CameraFocus> resolve_camera_focus(elysia::camera::CameraSlot) const override;
private:
    void attach_development_panel();
    void detach_development_panel();
    static constexpr float kDefaultCameraZoom = 1.5f;
#if ELYSIA_ENABLE_IMGUI
    elysia::tools::IDevelopmentPanelRegistry* _development_panels = nullptr;
    elysia::tools::DevelopmentPanelHandle _development_panel{};
#endif
    void finish(bool success);
    void build_result_panel();
    elysia::ui::UiWindow* build_modal_frame();
    void build_pause_panel();
    void open_pause_menu();
    void close_pause_menu();
    void retry_level();
    void show_result(bool saved);
    void continue_from_result();
    void reached(bool success,const char* reason="");
    void restore_player();
    void begin_moon(play::Vec center);
    void begin_portal(play::Entity& source,play::Entity& destination);
    void update_transition(double delta);
    double move_camera(play::Vec destination);
    void apply_pointer(play::Vec point);
    content::Json story_action(const std::string&,const content::Json&);
    void update_dialogue();
    int _level=1,_stars=0,_crystals=0,_crystal_count=0;
    double _elapsed=0;
    bool _finished=false,_success=false;
    bool _restart=false,_input_enabled=true,_demo=false,_demo_paused=false,_event_pending=false,_follow_role=true;
    play::Vec _story_camera{};
    play::CameraMove _camera_move;
    enum class OpeningPhase { Ready, Pan, Spawn };
    OpeningPhase _opening_phase=OpeningPhase::Ready;
    bool opening()const{return _opening_phase!=OpeningPhase::Ready;}
    bool _story_started=false;
    int _camera_moves=0;
    content::Json _data;
    play::Player* _player=nullptr;
    std::vector<play::Entity*> _entities;
    play::Entity* _portal_lock=nullptr;
    int _portal_transits=0;
    play::RoleTransition _transition;
    play::Vec _transition_camera_from{};
    play::Vec _transition_velocity{};
    bool _portal_emerging=false;
    int _spawn_effects=0,_moon_effects=0;
    play::Vec _wind_force{};
    bool _laser_hit=false;
    std::vector<play::Projectile*> _projectiles;
    int _shots_fired=0,_homing_fired=0;
    int _deaths=0;
    std::string _failure_reason,_pending_failure;
    elysia::ui::UiLabel* _hud=nullptr;
    elysia::ui::UiLabel* _hint=nullptr;
    frontend::StarRating* _hud_stars=nullptr;
    frontend::PauseIconButton* _pause_button=nullptr;
    elysia::ui::UiWindow* _pause_menu=nullptr;
    frontend::StarRating* _pause_stars=nullptr;
    elysia::ui::UiLabel* _pause_stats=nullptr;
    bool _pause_menu_open=false,_pause_was_logic_paused=false;
    std::vector<elysia::core::GameObject*> _pause_updates;
    elysia::ui::UiWindow* _result=nullptr;
    elysia::ui::UiLabel* _result_title=nullptr;
    elysia::ui::UiLabel* _result_stats=nullptr;
    elysia::ui::UiLabel* _result_save=nullptr;
    elysia::ui::UiButton* _result_continue=nullptr;
    frontend::StarRating* _result_stars=nullptr;
    testing::ReplayProbe& _probe=testing::ReplayProbe::shared();
    story::Runtime _story;
    play::TutorialReplay _tutorial_replay;
    content::Json _text;
    std::string _dialogue_key;
    elysia::ui::UiTextBlock* _dialogue=nullptr;
    frontend::MoonPanel* _dialogue_panel=nullptr;
    frontend::MoonPanel* _portrait_panel=nullptr;
    elysia::ui::UiImage* _face=nullptr;
    elysia::ui::UiImage* _tutorial=nullptr;
    std::vector<elysia::audio::SoundHandle> _sounds;
    double _mechanism_time=0;
    std::vector<bool> _triggered_areas;
    std::vector<std::pair<double,int>> _scheduled_actions;
};
}
