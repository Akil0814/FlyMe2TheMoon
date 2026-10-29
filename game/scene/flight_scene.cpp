#include "flight_scene.h"
#include "main_menu_scene.h"
#include "selection_scene.h"
#include "game/progress/progress.h"
#include "game/progress/scoring.h"
#include "game/content/level_music.h"
#include "engine/camera/follow_strategy.h"
#include "engine/tools/logger.h"
#include "engine/resources/resource_service.h"
#include "engine/audio/audio_service.h"
#include <regex>
#include <format>
#include <stdexcept>
#include <numbers>
#include "game/gameplay/flight_input.h"
#include "game/gameplay/role_geometry.h"
#include "game/gameplay/seesaw_parameters.h"
#include "game/gameplay/homing_parameters.h"
#include "game/gameplay/boundary_rocks.h"
#if ELYSIA_ENABLE_IMGUI
#include "game/development/camera_controls.h"
#include "game/development/development_overlay.h"
#include <imgui.h>
#endif

namespace game::scene {
using namespace elysia;
using namespace play;
FlightScene::FlightScene():Scene(elysia::scene::SceneRuntimeFeatures{
    .fixed_step=elysia::scene::FixedStepConfig{},
    .physics=physics::PhysicsWorldConfig{.units_per_meter=32,.gravity={0,320}},
    .camera=elysia::scene::CameraSceneConfig{.focus_mode=elysia::scene::CameraFocusMode::ResolveEachFrame,.advance_when_paused=true}}) {}
FlightScene::~FlightScene() { detach_development_panel(); }
void FlightScene::attach_development_panel() {
#if ELYSIA_ENABLE_IMGUI
    detach_development_panel();
    _development_panels=runtime_context().development_panels();
    if (!_development_panels) return;
    _development_panel=_development_panels->register_panel("fly.flight.camera", [this] {
        if (ImGui::Begin(development::DevelopmentOverlay::MainPanelTitle))
            development::draw_camera_controls(camera_runtime(), kDefaultCameraZoom);
        ImGui::End();
    });
#endif
}
void FlightScene::detach_development_panel() {
#if ELYSIA_ENABLE_IMGUI
    if (_development_panels && _development_panel.is_valid())
        (void)_development_panels->unregister_panel(_development_panel);
    _development_panel={};
    _development_panels=nullptr;
#endif
}
void FlightScene::on_enter(const elysia::scene::ScenePayload& payload) {
    detach_development_panel();
    _audio.reset();_rotation_audio.clear();_entrance_sound_pending=false;_role_velocity_valid=false;_idle_audio_time=_audio.next_idle_delay();
    _probe.begin();
    _deaths=0;_failure_reason.clear();_pending_failure.clear();_laser_hit=false;_death_pending=false;
    _pause_menu_open=false;_pause_was_logic_paused=false;_pause_updates.clear();
    _transition={};_portal_emerging=false;_spawn_effects=0;_moon_effects=0;
    _camera_move={};_opening_phase=OpeningPhase::Pan;_story_started=false;_camera_moves=0;
    clear_scene_objects();_entities.clear();_projectiles.clear();_shots_fired=0;_homing_fired=0;_player=nullptr;_portal_lock=nullptr;_portal_transits=0;
    if(const auto* p=elysia::scene::try_scene_payload<FlightPayload>(payload)){_level=p->level;_restart=p->restart;}
    _data=content::level(_level);_stars=0;_crystals=0;_crystal_count=0;_elapsed=0;_finished=false;_success=false;
    (void)progress::Store::instance().load();
    _text=content::read("text/"+content::language()+"/strings.json");_input_enabled=true;_demo=false;_demo_paused=false;_event_pending=false;_follow_role=true;_dialogue_key.clear();_dialogue_pointer_pressed=false;
    _mechanism_time=0;_triggered_areas.assign(_data.at("areas").size(),false);_scheduled_actions.clear();
    create_and_add_object<Parallax>(_data,&camera());
    create_and_add_object<BoundaryRocks>(_data);
    create_and_add_object<Decor>(_data,false);
    for(const auto& line:_data.at("edges"))for(size_t i=1;i<line.size();++i) {
        Vec a{line[i-1][0],line[i-1][1]},b{line[i][0],line[i][1]};
        if(std::hypot(b.x-a.x,b.y-a.y)>.01f)create_and_add_object<Edge>(a,b);
    }
    for(const auto& obj:_data.at("entities")) {
        auto* entity=create_and_add_object<Entity>(obj);
        if(!entity||!entity->physics_state())throw std::runtime_error("Entity physics registration failed");
        _entities.push_back(entity);
    }
    for(auto* entity:_entities)if(entity->moving()&&!entity->info.contains("CarrierID")) {
        auto* pivot=create_and_add_object<Pivot>(entity->origin);
        entity->attach_platform_joint(pivot->physics_handle());
    }
    for(auto* entity:_entities)if(entity->rotating()) {
        const float rad=entity->angle*std::numbers::pi_v<float>/180;
        const Vec local{seesaw_anchor(entity->kind,entity->info),0};
        const Vec anchor=entity->origin+Vec{local.x*std::cos(rad),local.x*std::sin(rad)};
        auto* pivot=create_and_add_object<Pivot>(anchor);
        const auto joint=physics_world().create_revolute_joint({.first=pivot->physics_handle(),.second=entity->physics_handle(),.local_anchor_first={},.local_anchor_second=local,
            .settings={.enable_motor=seesaw_speed(entity->info)!=0,.motor_speed=seesaw_speed(entity->info),.max_motor_torque=static_cast<float>(parameter(entity->info,"Torque",100)*1024)}});
        if(!joint.is_valid())throw std::runtime_error("Rotating bar joint creation failed");
    }
    for(auto* entity:_entities)if(entity->info.contains("CarrierID")){
        Entity* carrier=nullptr;
        for(auto* candidate:_entities)if(!candidate->item&&candidate->id==parameter(entity->info,"CarrierID"))carrier=candidate;
        if(!carrier||carrier==entity)throw std::runtime_error("Invalid carrier reference");
        entity->attach_carrier(carrier);
    }
    _player=create_and_add_object<Player>(Vec{_data.at("born")[0],_data.at("born")[1]});
    if(!_player||!_player->physics_state())throw std::runtime_error("Player registration failed");
    _player->present_at(std::nullopt,1,true);
    create_and_add_object<Decor>(_data,true);
    build_ingame_ui();
    build_result_panel();
    build_pause_panel();
    set_ui_interaction_mode(elysia::scene::UiInteractionMode::Pointer);
    set_shortcut_devices(input::InputCapture::Keyboard|input::InputCapture::Pointer|input::InputCapture::Gamepad);
    camera_runtime().set_zoom(camera::CameraSlot::Main,kDefaultCameraZoom);
    camera_runtime().set_world_bounds(camera::CameraSlot::Main,Rect{0,0,_data.at("width"),_data.at("height")});
    camera_runtime().set_follow_strategy(camera::CameraSlot::Main,std::make_unique<camera::HardFollowStrategy>());
    const auto moon=std::find_if(_entities.begin(),_entities.end(),[](const auto* e){return e->kind==101;});
    if(moon==_entities.end())throw std::runtime_error("Opening camera requires a moon");
    _story_camera=(*moon)->center();_follow_role=false;_input_enabled=false;
    _camera_move.begin(_story_camera,_player->center(),1);
    camera_runtime().snap_to_focus(camera::CameraSlot::Main);
    pause();
    _probe.begin();
    const auto music=content::level_music(_level,_data.at("suite").get<int>());
    if(!ELYSIA_AUDIO->play_music(music))ELYSIA_LOG_WARN("fly.audio","Level music rejected: " << music);
    update_dialogue();
    ELYSIA_LOG_INFO("fly",std::format("Entered level {}: {} entities, {} contours",_level,_entities.size(),_data.at("edges").size()));
    attach_development_panel();
}
void FlightScene::on_exit(){detach_development_panel();_audio.clear();ELYSIA_AUDIO->stop_music();_pause_updates.clear();_pause_menu_open=false;_player=nullptr;_entities.clear();_hud=nullptr;_hud_stats=nullptr;_pause_metrics=nullptr;_result_metrics=nullptr;_hud_stars=nullptr;_star_pickups=nullptr;_pause_menu=nullptr;_pause_button=nullptr;_dialogue=nullptr;_face=nullptr;_tutorial=nullptr;_dialogue_panel=nullptr;_skip_button=nullptr;clear_scene_objects();}
void FlightScene::on_reset(){on_exit();}
void FlightScene::on_shortcuts(const input::RawInputFrame&,const std::vector<input::RawInputEvent>& events) {
    using C=input::RawInputControl;
    for(const auto& e:events){
        if(e.type==input::RawInputEventType::ControlReleased&&e.control==C::MouseLeft&&_dialogue_pointer_pressed){
            _dialogue_pointer_pressed=false;consume_input(e);
            if(!_pause_menu_open&&!_death_pending&&_story.dialogue())advance_dialogue();
            return;
        }
        if(e.type!=input::RawInputEventType::ControlPressed)continue;
        if(_finished && e.control==C::KeyEscape) {
            consume_input(e);
            request_scene_switch(scene_keys::Selection,SelectionPayload{(_level-1)/15});
            return;
        }
        if(_pause_menu_open){
            if(e.control==C::KeyP||e.control==C::KeyEscape||e.control==C::GamepadStart){consume_input(e);game::audio::ui_click();close_pause_menu();}
            else if(e.control==C::KeyR){consume_input(e);game::audio::ui_click();retry_level();}
            return;
        }
        if(e.control==C::KeyEscape||e.control==C::KeyP||e.control==C::GamepadStart||e.control==C::GamepadEast){consume_input(e);open_pause_menu();return;}
        if(!_death_pending&&_story_started&&_story.dialogue()&&(e.control==C::KeyEnter||e.control==C::MouseLeft||e.control==C::KeyTab||e.control==C::GamepadSouth||e.control==C::GamepadNorth)){
            if(e.control==C::MouseLeft){
                _dialogue_pointer_pressed=true;consume_input(e);return;
            }
            consume_input(e);advance_dialogue(e.control==C::KeyTab||e.control==C::GamepadNorth);return;
        }
        if(e.control==C::KeyR){consume_input(e);game::audio::ui_click();request_scene_switch(scene_keys::Flight,FlightPayload{_level,true},elysia::scene::SceneReloadMode::Recreate);return;}
        if(e.control==C::KeyEnter&&_finished){consume_input(e);
            game::audio::ui_click();continue_from_result();return;}
    }
}
void FlightScene::on_routed_input(const input::InputSnapshot& snapshot) {
    if(!_player)return;
    if(is_paused()||_finished||_event_pending||_transition.active()){_player->thrust={};return;}
    // Animation updates precede fixed steps. Keep the last replay input visible
    // until the next recorded frame replaces it, including frames without a step.
    if(_demo)return;
    _player->thrust={};
    if(!_input_enabled||snapshot.focus_lost)return;
    using C=input::RawInputControl;
    bool left=false,right=false,up=false,mouse=false;Vec point,pad;
    for(const auto& source:snapshot.sources) {
        const auto& s=source.frame.state;
        left|=s.is_pressed(C::KeyA)||s.is_pressed(C::KeyLeft);
        right|=s.is_pressed(C::KeyD)||s.is_pressed(C::KeyRight);
        up|=s.is_pressed(C::KeyW)||s.is_pressed(C::KeyUp)||s.is_pressed(C::KeySpace);
        if(source.source.is_gamepad()){
            const auto thrust=gamepad_thrust(s);
            if(thrust.length_squared()>pad.length_squared())pad=thrust;
        }
        if(s.is_pressed(C::MouseLeft)){mouse=true;point={static_cast<float>(source.frame.mouse_x),static_cast<float>(source.frame.mouse_y)};}
    }
    if(up||left||right){const float angle=(right==left?0.f:right?1.f:-1.f)*50*std::numbers::pi_v<float>/180;_player->thrust={std::sin(angle),-std::cos(angle)};}
    else _player->thrust=pad;
    if(mouse) {
        apply_pointer(camera().screen_to_world(point));
    }
}
void FlightScene::apply_pointer(Vec point) {
    _player->thrust=pointer_thrust(_player->position(),point,content::catalog().at("role"));
}
void FlightScene::on_application_suspended() {
    if (_player) _player->thrust = {};
    _dialogue_pointer_pressed = false;
    if (!_finished && !_death_pending) open_pause_menu();
}
void FlightScene::finish(bool success) {
    if(_finished||_death_pending)return;_audio.set_paused(true);_finished=true;_success=success;_player->thrust={};pause();
    _star_pickups->clear();_hud_stars->set_collected(_stars);
    for(auto* entity:_entities)if(entity->collected)entity->set_visible(false);
    auto& store=progress::Store::instance();
    // Compare before record_clear replaces the previous best; ties are not records.
    const bool new_record=success&&(!store.data.score_verified[_level-1]||
        progress::original_score(_level,_stars,_crystals,_elapsed,content::catalog().at("score").at("svrs"))>store.data.scores[_level-1]);
    const bool saved=success?store.record_clear(_level,_stars,_crystals,_crystal_count,_stars+_crystal_count,_elapsed):store.record_death(_level,_crystal_count);
    show_result(saved,new_record);
    ELYSIA_LOG_INFO("fly",std::format("Level {} {} time={:.3f} stars={} crystals={}",_level,success?"SUCCESS":"FAIL",_elapsed,_stars,_crystals));
}
void FlightScene::reached(bool success,const char* reason) {
    if(_finished||_event_pending)return;_event_pending=true;_player->thrust={};
    if(!success){
        ++_deaths;_failure_reason=reason;ELYSIA_LOG_INFO("fly.failure",_failure_reason);
        if(std::string_view(reason)=="spike")_audio.at("12-4",_player->center());
        _death_pending=true;_entrance_sound_pending=false;_audio.role_die();_player->die();
        physics_world().set_body_enabled(_player->physics_handle(),false);pause();
        return;
    }
    if(!_story.event("onRoleReachedMoon"))finish(true);
}
void FlightScene::restore_player() {
    _portal_lock=nullptr;_transition={};_portal_emerging=false;_death_pending=false;
    _audio.reset();_role_velocity_valid=false;_idle_audio_time=_audio.next_idle_delay();_player->idle_action_time=0;_result_animation_done=true;
    for(auto* projectile:_projectiles)projectile->destroy();_projectiles.clear();
    physics_world().teleport_object(_player->physics_handle(),role_spawn({_data.at("born")[0],_data.at("born")[1]}),physics::TeleportVelocityMode::Clear);
    physics_world().set_body_enabled(_player->physics_handle(),true);
    _player->present_at(std::nullopt);_player->play_effect(RoleEffect::Enter);start_entrance_sound();++_spawn_effects;
    _player->dead=false;_player->set_update_when_paused(false);_player->thrust={};_stars=0;_crystals=0;_crystal_count=0;_elapsed=0;_event_pending=false;_finished=false;
    _star_pickups->clear();_hud_stars->set_collected(0);
    for(auto* entity:_entities)if(entity->item){entity->restore_pickup();physics_world().set_body_enabled(entity->physics_handle(),true);}
    _result->set_visible(false);_result->set_active(false);
    _pause_button->set_visible(true);set_ui_interaction_mode(elysia::scene::UiInteractionMode::Pointer);
    resume();move_camera(_player->center());
}
double FlightScene::move_camera(Vec destination) {
    const double duration=_camera_move.begin(camera().center(),destination);
    _story_camera=_camera_move.position();++_camera_moves;
    return duration;
}
void FlightScene::on_before_update(double delta) {
    _audio.set_listener(camera().center());
    if(_pause_menu_open)return;
    if(_camera_move.active()){_camera_move.advance(delta);_story_camera=_camera_move.position();}
}
void FlightScene::begin_moon(Vec center) {
    if(_finished||_event_pending||_transition.active())return;
    _event_pending=true;_player->thrust={};++_moon_effects;_audio.play("70-2");
    _transition.begin_moon(_player->center(),center);
    _player->present_at(_transition.from);_player->play_effect(RoleEffect::Moon);
    if(!physics_world().set_body_enabled(_player->physics_handle(),false))throw std::runtime_error("Moon transition body disable rejected");
    ELYSIA_LOG_INFO("fly.effect","Moon approach started");
}
void FlightScene::begin_portal(Entity& source,Entity& destination) {
    _player->thrust={};_portal_lock=&destination;_portal_emerging=false;_audio.play("20-1");
    _transition_velocity=_player->velocity();
    _transition.begin_portal(_player->center(),source.center(),destination.center());
    const auto view=camera().view_rect();_transition_camera_from={view.x()+view.width()/2,view.y()+view.height()/2};
    _player->present_at(_transition.from);_player->play_effect(RoleEffect::PortalIn);
    if(!physics_world().set_body_enabled(_player->physics_handle(),false))throw std::runtime_error("Portal transition body disable rejected");
    ELYSIA_LOG_INFO("fly.portal",std::format("Pull {} -> {}",source.id,destination.id));
}
void FlightScene::update_transition(double delta) {
    if(!_transition.active())return;
    _transition.advance(delta);
    const auto phase=_transition.phase();
    if(_transition.kind==RoleTransitionKind::Moon) {
        _player->present_at(_transition.position());
        if(phase==RoleTransitionPhase::Complete) {
            _transition={};
            ELYSIA_LOG_INFO("fly.effect","Moon effect complete; dispatch reached event");
            if(!_story.event("onRoleReachedMoon"))finish(true);
        }
        return;
    }
    if((phase==RoleTransitionPhase::PortalEmerge||phase==RoleTransitionPhase::Complete)&&!_portal_emerging) {
        _portal_emerging=true;
        // Preserve the existing transit velocity policy; only presentation is new.
        if(!physics_world().teleport_object(_player->physics_handle(),_transition.destination+Vec{0,13},physics::TeleportVelocityMode::Preserve))throw std::runtime_error("Portal transit rejected");
        _player->play_effect(RoleEffect::Enter);start_entrance_sound();++_portal_transits;
        ELYSIA_LOG_INFO("fly.portal","Exit unfold started");
    }
    _player->present_at(_transition.position(),_transition.scale(),phase==RoleTransitionPhase::PortalTravel);
    if(phase==RoleTransitionPhase::Complete) {
        if(!physics_world().set_body_enabled(_player->physics_handle(),true))throw std::runtime_error("Portal transition body restore rejected");
        if(!physics_world().set_velocity(_player->physics_handle(),_transition_velocity))throw std::runtime_error("Portal transit velocity restore rejected");
        _player->present_at(std::nullopt);_player->play_effect(RoleEffect::None);_transition={};
        ELYSIA_LOG_INFO("fly.portal","Exit unfold complete; controls restored");
    }
}
content::Json FlightScene::story_action(const std::string& op,const content::Json& a) {
    auto& store=progress::Store::instance();
    if(op=="global.getBool")return store.data.story_flags.contains(a[0].get<std::string>());
    if(op=="global.isLevelPassed")return store.data.passed(a[0].get<int>());
    if(op=="global.setBool"){store.data.story_flags.insert(a[0].get<std::string>());store.save();}
    else if(op=="game.isRestart")return _restart;
    else if(op=="game.setUserInputEnabled"){_input_enabled=a[0];_player->thrust={};}
    else if(op=="game.pauseLogic")pause();
    else if(op=="game.resumeLogic")resume();
    else if(op=="game.restartTiming")_elapsed=0;
    else if(op=="game.success")finish(true);
    else if(op=="event.unhandled")finish(a[0]=="onRoleReachedMoon");
    else if(op=="game.showTutorialDiagram"){
        _tutorial->set_type(a[0]);
    }else if(op=="game.setReplayMode"){_demo=true;_demo_paused=true;_tutorial->set_demo(true);}
    else if(op=="game.setPlayMode"){_demo=false;_tutorial->set_demo(false);}
    else if(op=="game.loadTutorialRep"){
        _tutorial_replay.load(content::read("tutorial.json").at(a[0].get<std::string>()));
        ELYSIA_LOG_INFO("fly.story","Loaded original tutorial input: "+a[0].get<std::string>());
    }else if(op=="replay.skipNonControlFrames"){_tutorial_replay.skip_idle();_demo_paused=false;resume();}
    else if(op=="replay.pause"){_demo_paused=true;_player->thrust={};}
    else if(op=="role.loadState")restore_player();
    else if(op=="role.lookLeftRight"){_player->look_around();_audio.role_look();}
    else if(op=="role.setIdleActionEnabled")_player->idle_enabled=a[0];
    else if(op=="camera.setFollowRoleMode"){_follow_role=a[0];if(!_follow_role&&!_camera_move.active())_story_camera=camera().center();}
    else if(op=="camera.gotoPosition")return move_camera({a[0],_data.at("height").get<float>()-a[1].get<float>()});
    else if(op=="camera.gotoRole")return move_camera(_player->center());
    else if(op=="sound.play"){
        std::string key=a[0].is_string()?a[0].get<std::string>():std::to_string(a[0].get<int>());
        // The original level 31 omitted quotes: 55-2 evaluates to 53 in Lua.
        if(_level==31&&a[0].is_number()&&a[0].get<int>()==53)key="55-2";
        _audio.play(key);
    }else if(op!="global.setBool")throw std::runtime_error("Unimplemented story operation "+op);
    return nullptr;
}
void FlightScene::update_dialogue() {
    const auto& line=_story.dialogue();const bool has_dialogue=_story_started&&line.has_value();const bool visible=has_dialogue&&!_pause_menu_open&&!_finished;
    _dialogue_panel->set_visible(visible);_dialogue->set_visible(visible);_face->set_visible(visible);
    _skip_button->set_visible(visible&&_story.skip_enabled());
    if(has_dialogue&&_dialogue_key!=line->key){_dialogue_key=line->key;
        _dialogue->set_story_text(_text.value(line->key,line->key));_face->set_texture(ELYSIA_RESOURCES->find_texture("fly.ui."+line->face));
        ELYSIA_LOG_INFO("fly.story","Dialogue: "+line->key);
    }
    if(!has_dialogue)_dialogue_key.clear();
    _tutorial->set_visible(!_pause_menu_open&&!_finished);
    update_hud_visibility();
}
void FlightScene::advance_dialogue(bool skip){
    if(!_story.dialogue())return;
    if(skip)_story.advance(true);
    else if(!_dialogue->complete())_dialogue->reveal();
    else _story.advance();
    update_dialogue();
}
void FlightScene::on_fixed_update(std::uint64_t,double delta) {
    if(_demo&&!_demo_paused&&!_finished&&!_event_pending&&!_transition.active()) {
        _player->thrust={};
        if(auto point=_tutorial_replay.next(_data.at("height")))apply_pointer(*point);
    }
    _wind_force={};
    if(!_finished&&!_event_pending) {
        for(auto* projectile:_projectiles){
            const bool was_expired=projectile->expired;
            if(projectile->advance(delta,_player->position(),*_player)){_laser_hit=true;_pending_failure="projectile";}
            if(!was_expired&&projectile->expired)_audio.at("34-1",projectile->position());
        }
        for(auto* entity:_entities){
            _wind_force+=entity->wind_force_at(_player->position());
            if(entity->update_laser(delta,_player->center(),*_player)){_laser_hit=true;_pending_failure="laser";}
            const int old_bomb_state=entity->bomb_state();
            if(entity->update_bomb(delta,_player->center())){_laser_hit=true;_pending_failure="bomb";}
            if(old_bomb_state==1&&entity->bomb_state()==2)_audio.at("19-2",entity->center());
            const int shots=entity->shots_due(delta,_player->center());
            for(int i=0;i<shots;++i){
                const Vec direction=entity->fire_direction(_player->center());const bool homing=entity->kind==113;
                const float power=homing?homing_power(entity->info):static_cast<float>(parameter(entity->info,"Power",1.5));
                auto* projectile=create_and_add_object<Projectile>(entity->fire_position(direction),homing?Vec{}:direction*(power*160),homing,power,homing?homing_max_speed(entity->info):static_cast<float>(parameter(entity->info,"MaxSpeed",6)),homing?entity->physics_state()->angle:0);
                if(!projectile||!projectile->physics_state())throw std::runtime_error("Projectile registration failed");
                if(homing&&_probe.enabled())ELYSIA_LOG_INFO("fly.homing",std::format("launcher={} spawn=({:.3f},{:.3f}) angle={:.6f} velocity=({:.3f},{:.3f}) Power={} MaxSpeed={}",entity->id,projectile->position().x,projectile->position().y,projectile->physics_state()->angle,projectile->velocity().x,projectile->velocity().y,power,homing_max_speed(entity->info)));
                _audio.at(homing?"17-4":"16-1",entity->fire_position(direction));
                _projectiles.push_back(projectile);++_shots_fired;if(homing)++_homing_fired;
            }
        }
        if(!_transition.active()&&_wind_force.length_squared()>0&&!physics_world().apply_force(_player->physics_handle(),_wind_force))throw std::runtime_error("Wind force rejected");
    }
    update_role_audio(delta);
    _mechanism_time+=delta;
    for(auto it=_scheduled_actions.begin();it!=_scheduled_actions.end();) {
        if(it->first>_mechanism_time){++it;continue;}
        for(auto* entity:_entities)if(!entity->item&&entity->id==it->second){entity->trigger_start();ELYSIA_LOG_INFO("fly.trigger",std::format("Start entity {} at {:.3f}",entity->id,_mechanism_time));}
        it=_scheduled_actions.erase(it);
    }
}
void FlightScene::on_after_update(double delta) {
    if(!_player)return;
    update_result_animation(delta);
    if(!_pause_menu_open){
    _tutorial->advance(delta);
    if(_story.dialogue())_dialogue->advance(delta);
    if(_demo){
        _tutorial->set_pointer_pressed(_tutorial_replay.pressed());
        if(const auto& point=_tutorial_replay.last_point())_tutorial->set_pointer_position(camera().world_to_screen(*point));
    }
    if(!_finished)_star_pickups->advance(delta);
    const bool opening_frame=opening();
    std::erase_if(_projectiles,[](auto* p){if(p->expired){p->destroy();return true;}return false;});
    if(opening()) {
        if(_opening_phase==OpeningPhase::Pan&&!_camera_move.active()) {
            _player->present_at(std::nullopt);_player->play_effect(RoleEffect::Enter);start_entrance_sound();++_spawn_effects;
            _opening_phase=OpeningPhase::Ready;_follow_role=true;_input_enabled=true;resume();_story_started=true;
            _story.start(_data.at("script"),[this](const std::string& op,const content::Json& args){return story_action(op,args);});
            ELYSIA_LOG_INFO("fly.camera","Opening pan complete; original story and role entrance ready");
        }
    }else if(!_death_pending)_story.update(delta);
    if(_death_pending&&_player->death_effect_finished()) {
        _death_pending=false;
        // Notify Lua only after the last death frame, including scripted restores.
        if(!_story.event("onRoleDied"))finish(false);
        else progress::Store::instance().record_death(_level,_crystal_count);
    }
    update_dialogue();
    update_transition(delta);
    if(_laser_hit){_laser_hit=false;reached(false,_pending_failure.c_str());}
    if(!opening_frame&&!is_paused()&&!_finished&&!_event_pending&&_input_enabled&&!_demo)_elapsed+=delta;
    if(!is_paused()&&!_finished&&!_event_pending&&!_transition.active()) {
        if(_portal_lock&&(_player->center()-_portal_lock->center()).length()>60)_portal_lock=nullptr;
        for(size_t i=0;i<_triggered_areas.size();++i)if(!_triggered_areas[i]) {
            const auto& area=_data.at("areas")[i];const Vec p=_player->position();
            if(p.x+14<area.at("x").get<float>()||p.x-14>area.at("x").get<float>()+area.at("w").get<float>()||p.y+14<area.at("y").get<float>()||p.y-14>area.at("y").get<float>()+area.at("h").get<float>())continue;
            _triggered_areas[i]=true;
            for(const auto& action:area.at("actions")) {
                if(action.at("Action")!="Start")throw std::runtime_error("Unknown area action");
                const std::string target=action.at("Target");if(!target.starts_with("4:"))throw std::runtime_error("Unknown area target layer");
                _scheduled_actions.emplace_back(_mechanism_time+parameter(action,"Delay"),std::stoi(target.substr(2)));
            }
        }
        std::vector<physics::CollisionOverlapQueryHit> hits;
        physics_world().overlap_shape({.shape=role_sensor_shape(),.pose={_player->position(),0},.filter={.collision={.category=1,.mask=4|8}}},hits);
        for(const auto& hit:hits)for(auto* entity:_entities) {
            if(_finished||_event_pending||_transition.active())break;
            if(entity->collected)continue;
            if(entity->lethal(hit.target.collider)){reached(false,"spike");continue;}
            if(hit.target.collider!=entity->physics_collider(0))continue;
            if(entity->item){
                entity->begin_pickup();physics_world().set_body_enabled(entity->physics_handle(),false);
                if(entity->kind==3){
                    ++_stars;_audio.play("33-6");
                    _star_pickups->launch(camera().world_to_screen(entity->center()),_hud_stars->slot_rect(_stars-1).center());
                }else {_crystals+=entity->kind==5?5:1;++_crystal_count;_audio.crystal();}
            }
            else if(entity->kind==101){entity->light_moon();begin_moon(entity->center());}
            else if(entity->kind==115&&entity!=_portal_lock&&entity->info.contains("TargetPortalID")) {
                const int target=static_cast<int>(parameter(entity->info,"TargetPortalID"));Entity* destination=nullptr;
                // Original levels normally reference object IDs. The single explicit
                // PortalID override in level 48 takes precedence over that fallback.
                for(auto* candidate:_entities)if(candidate->kind==115&&candidate->info.contains("PortalID")&&parameter(candidate->info,"PortalID")==target)destination=candidate;
                if(!destination)for(auto* candidate:_entities)if(candidate->kind==115&&candidate->id==target)destination=candidate;
                if(!destination)throw std::runtime_error("Portal destination absent");
                begin_portal(*entity,*destination);
            }
        }
        if(_player->center().y>_data.at("height").get<float>()+100||_player->center().x< -100||_player->center().x>_data.at("width").get<float>()+100)reached(false,"out_of_bounds");
    }
    }
    update_audio(delta);
    _hud_stars->set_collected(_stars-_star_pickups->pending_count());
    _hud->set_text_content(ui::ui_raw_text(content::format("hud_stage",_level)));
    _hud_stats->set_values(_elapsed,large_crystal_count(),small_crystal_count());
    update_hud_visibility();
    if(_probe.enabled()) {
    content::Json homing_projectiles=content::Json::array();
    if(_probe.enabled())for(const auto* projectile:_projectiles)if(projectile->homing()&&!projectile->expired){
        const auto state=*projectile->physics_state();const auto pose=*physics_world().render_pose(projectile->physics_handle());
        homing_projectiles.push_back({{"x",state.position.x},{"y",state.position.y},{"vx",state.velocity.x},{"vy",state.velocity.y},{"angle",state.angle},{"angular_velocity",state.angular_velocity},{"render_angle",pose.angle}});
    }
    content::Json mechanisms=content::Json::array();
    for(const auto* entity:_entities)if(!entity->item&&((entity->kind>=105&&entity->kind<=107)||entity->rotating()))mechanisms.push_back({{"id",entity->id},{"kind",entity->kind},{"x",entity->position().x},{"y",entity->position().y},{"angle",entity->physics_state()->angle},{"mass",entity->physics_state()->mass},{"rotational_inertia",entity->physics_state()->rotational_inertia},{"angular_velocity",entity->physics_state()->angular_velocity},{"awake",entity->physics_state()->awake},{"body_enabled",entity->physics_state()->enabled}});
    if(_probe.update(runtime_context().renderer(),{{"tutorial_type",_tutorial->type()},{"tutorial_frame",_tutorial->frame()},{"dialogue_characters",_dialogue->visible_count()},{"dialogue_complete",_dialogue->complete()},{"role_effect_time",_player->effect_time()},{"audio_requests",_audio.requests()},{"audio_loops",_audio.active_loops()},{"audio_events",_audio.events()},{"result_animation_done",_result_animation_done},{"restart",_restart},{"mechanism_time",_mechanism_time},{"pause_menu_open",_pause_menu_open},{"hud_gold_stars",_hud_stars->collected()},{"hud_empty_stars",3-_hud_stars->collected()},{"result_gold_stars",_result_stars->collected()},{"role_effect_frame",_player->effect_pose().frame},{"opening_camera",opening()},{"opening_phase",static_cast<int>(_opening_phase)},{"camera_moving",static_cast<int>(_camera_move.active())},{"camera_holding",_camera_move.holding()},{"camera_moves",_camera_moves},{"camera_x",camera().center().x},{"camera_y",camera().center().y},{"role_effect_image",static_cast<int>(_player->effect_pose().image)},{"role_body_visible",_player->body_visible()},{"transition_phase",static_cast<int>(_transition.phase())},{"spawn_effects",_spawn_effects},{"moon_effects",_moon_effects},{"role_body_enabled",_player->physics_state()->enabled},{"paused",is_paused()},{"thrust_x",_player->thrust.x},{"thrust_y",_player->thrust.y},{"result_visible",_finished},{"death_playing",_death_pending},{"death_time",_player->death_effect_time()},{"deaths",_deaths},{"failure_reason",_failure_reason},{"detonated_bombs",std::count_if(_entities.begin(),_entities.end(),[](const auto* e){return e->bomb_state()>=2;})},{"pilot_view",{camera().view_rect().x(),camera().view_rect().y(),camera().zoom()}},{"scene",2},{"level",_level},{"success",_success},{"finished",_finished},{"stars",_stars},{"crystals",_crystals},{"crystal_count",_crystal_count},{"elapsed",_elapsed},{"dialogue",_dialogue_key},{"input_enabled",_input_enabled},{"demo",_demo},{"demo_frame",_tutorial_replay.cursor()},{"portal_transits",_portal_transits},{"active_lasers",std::count_if(_entities.begin(),_entities.end(),[](const auto* e){return e->laser_active();})},{"wind_x",_wind_force.x},{"wind_y",_wind_force.y},{"shots_fired",_shots_fired},{"homing_fired",_homing_fired},{"projectiles",_projectiles.size()},{"armed_bombs",std::count_if(_entities.begin(),_entities.end(),[](const auto* e){return e->bomb_state()>0;})},{"homing_projectiles",homing_projectiles},{"mechanisms",mechanisms},{"triggered_count",std::count(_triggered_areas.begin(),_triggered_areas.end(),true)},{"x",_player->center().x},{"y",_player->center().y},{"vx",_player->velocity().x},{"vy",_player->velocity().y}}))request_quit();
    }
}
std::optional<camera::CameraFocus> FlightScene::resolve_camera_focus(camera::CameraSlot)const {
    if(!_player)return std::nullopt;
    if(opening()||_camera_move.active()){const Rect focus{_story_camera.x-1,_story_camera.y-1,2,2};return camera::CameraFocus{focus,focus};}
    if(_follow_role&&_transition.kind==RoleTransitionKind::Portal) {
        const auto p=_transition_camera_from+(_transition.destination-_transition_camera_from)*_transition.camera_fraction();
        const Rect focus{p.x-1,p.y-1,2,2};return camera::CameraFocus{focus,focus};
    }
    if(!_follow_role){const Rect focus{_story_camera.x-1,_story_camera.y-1,2,2};return camera::CameraFocus{focus,focus};}return camera::CameraFocus{_player->render_rect(),_player->render_rect()};
}
}
