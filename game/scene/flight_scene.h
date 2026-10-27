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
#include "game/frontend/pickup_effects.h"
#include "engine/tools/development_overlay.h"
#include "game/audio/game_audio.h"
#include "game/frontend/tutorial_ui.h"
#include "game/frontend/story_text.h"

namespace game::scene_keys {inline constexpr elysia::scene::SceneKey Flight=2;}
namespace game::scene {
struct FlightPayload {int level=1;bool restart=false;};
class FlightScene final : public elysia::scene::Scene {
    friend class AudioTestAccess;
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
    void show_result(bool saved,bool new_record);
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
    void advance_dialogue(bool skip=false);
    void update_audio(double delta);
    void update_role_audio(double delta);
    void start_entrance_sound(){_entrance_sound_pending=true;}
    void update_result_animation(double delta);
    void complete_result_animation();
    int _level=1,_stars=0,_crystals=0,_crystal_count=0;
    // Collected value = small + 5 * large; count = small + large.
    int large_crystal_count()const{return (_crystals-_crystal_count)/4;}
    int small_crystal_count()const{return _crystal_count-large_crystal_count();}
    double _elapsed=0;
    bool _finished=false,_success=false;
    bool _death_pending=false;
    bool _using_gamepad=false;
    bool _restart=false,_input_enabled=true,_demo=false,_demo_paused=false,_event_pending=false,_follow_role=true;
    play::Vec _story_camera{};
    play::CameraMove _camera_move;
    enum class OpeningPhase { Ready, Pan };
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
    frontend::RunStats* _hud_stats=nullptr;
    elysia::ui::UiLabel* _hint=nullptr;
    frontend::StarRating* _hud_stars=nullptr;
    frontend::StarPickupEffects* _star_pickups=nullptr;
    frontend::PauseIconButton* _pause_button=nullptr;
    elysia::ui::UiWindow* _pause_menu=nullptr;
    frontend::StarRating* _pause_stars=nullptr;
    elysia::ui::UiLabel* _pause_stats=nullptr;
    frontend::RunStats* _pause_metrics=nullptr;
    bool _pause_menu_open=false,_pause_was_logic_paused=false;
    std::vector<elysia::core::GameObject*> _pause_updates;
    elysia::ui::UiWindow* _result=nullptr;
    elysia::ui::UiLabel* _result_title=nullptr;
    elysia::ui::UiLabel* _result_stats=nullptr;
    frontend::RunStats* _result_metrics=nullptr;
    elysia::ui::UiLabel* _result_notice=nullptr;
    elysia::ui::UiButton* _result_continue=nullptr;
    frontend::StarRating* _result_stars=nullptr;
    testing::ReplayProbe& _probe=testing::ReplayProbe::shared();
    story::Runtime _story;
    play::TutorialReplay _tutorial_replay;
    content::Json _text;
    std::string _dialogue_key;
    frontend::StoryText* _dialogue=nullptr;
    frontend::HudPanel* _dialogue_panel=nullptr;
    frontend::HudPanel* _portrait_panel=nullptr;
    elysia::ui::UiImage* _face=nullptr;
    frontend::TutorialUi* _tutorial=nullptr;
    bool _dialogue_pointer_pressed=false;
    game::audio::World _audio;
    bool _entrance_sound_pending=false;
    bool _role_velocity_valid=false;
    play::Vec _role_previous_velocity{};
    double _idle_audio_time=0;
    struct RotationAudio {float angle=0,accumulated=0;bool initialized=false;};
    std::map<int,RotationAudio> _rotation_audio;
    double _result_animation_time=0,_result_tick_time=0;
    int _result_animation_stars=0,_result_score=0,_result_display_score=0;
    bool _result_new_record=false,_result_animation_done=true;
    double _mechanism_time=0;
    std::vector<bool> _triggered_areas;
    std::vector<std::pair<double,int>> _scheduled_actions;
};
}
