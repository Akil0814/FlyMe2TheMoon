#include "flight_scene.h"
#include "main_menu_scene.h"
#include "selection_scene.h"
#include "game/progress/progress.h"
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

namespace game::scene {
using namespace elysia;
using namespace play;
FlightScene::FlightScene():Scene(elysia::scene::SceneRuntimeFeatures{
    .fixed_step=elysia::scene::FixedStepConfig{},
    .physics=physics::PhysicsWorldConfig{.units_per_meter=32,.gravity={0,320}},
    .camera=elysia::scene::CameraSceneConfig{.focus_mode=elysia::scene::CameraFocusMode::ResolveEachFrame,.advance_when_paused=true}}) {}
void FlightScene::on_enter(const elysia::scene::ScenePayload& payload) {
    _probe.begin();
    _deaths=0;_failure_reason.clear();_pending_failure.clear();_laser_hit=false;
    clear_scene_objects();_entities.clear();_projectiles.clear();_shots_fired=0;_homing_fired=0;_player=nullptr;_portal_lock=nullptr;_portal_transits=0;
    if(const auto* p=elysia::scene::try_scene_payload<FlightPayload>(payload)){_level=p->level;_restart=p->restart;}
    _data=content::level(_level);_stars=0;_crystals=0;_crystal_count=0;_elapsed=0;_finished=false;_success=false;
    (void)progress::Store::instance().load();
    _text=content::read("text/"+content::language()+"/strings.json");_input_enabled=true;_demo=false;_demo_paused=false;_event_pending=false;_follow_role=true;_dialogue_key.clear();
    _mechanism_time=0;_triggered_areas.assign(_data.at("areas").size(),false);_scheduled_actions.clear();
    create_and_add_object<Parallax>(_data,&camera());
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
    create_and_add_object<Decor>(_data,true);
    _hud=create_and_add_object<ui::UiLabel>(Rect{24,16,1180,38},10,ui::ui_raw_text(""));
    _hint=create_and_add_object<ui::UiLabel>(Rect{24,668,1230,36},10,ui::ui_raw_text("Hold Space / W to fly; A / D to steer. Mouse: hold below Kiana to thrust away.  P pause  R retry  Esc menu"));
    _hint->set_text_fit_mode(ui::UiLabelTextFitMode::ShrinkToFit);
    _hud->set_style_overrides({.background=core::Color{14,22,49,220},.draw_background=true});_hud->set_padding(6);
    _hint->set_style_overrides({.background=core::Color{14,22,49,230},.draw_background=true});_hint->set_padding(6);
    _dialogue_panel=create_and_add_object<ui::UiLabel>(Rect{20,476,1240,184},20,ui::ui_raw_text(""));
    _dialogue_panel->set_style_overrides({.background=core::Color{15,24,53,245},.draw_background=true});
    _face=create_and_add_object<ui::UiImage>(nullptr,Rect{34,486,160,160},21);
    _dialogue=create_and_add_object<ui::UiTextBlock>(Rect{212,492,1024,152},22);_dialogue->set_padding(8);
    _tutorial=create_and_add_object<ui::UiImage>(nullptr,Rect{1040,280,140,160},19);_tutorial->set_visible(false);
    build_result_panel();
    set_ui_interaction_mode(elysia::scene::UiInteractionMode::Pointer);
    camera_runtime().set_zoom(camera::CameraSlot::Main,1.0f);
    camera_runtime().set_world_bounds(camera::CameraSlot::Main,Rect{0,0,_data.at("width"),_data.at("height")});
    camera_runtime().set_follow_strategy(camera::CameraSlot::Main,std::make_unique<camera::HardFollowStrategy>());
    camera_runtime().snap_to_focus(camera::CameraSlot::Main);
    resume();
    _probe.begin();
    const char* tracks[]{"SkyTunnel","SpiralofAdventure","SymphonyofRush","LifeforSpeed"};
    if(!ELYSIA_AUDIO->play_music(std::string("fly.")+tracks[(_level-1)/15]))ELYSIA_LOG_WARN("fly.audio","Level music rejected");
    _story.start(_data.at("script"),[this](const std::string& op,const content::Json& args){return story_action(op,args);});update_dialogue();
    ELYSIA_LOG_INFO("fly",std::format("Entered level {}: {} entities, {} contours",_level,_entities.size(),_data.at("edges").size()));
}
void FlightScene::on_exit(){for(auto h:_sounds)ELYSIA_AUDIO->stop_sound(h);_sounds.clear();ELYSIA_AUDIO->stop_music();_player=nullptr;_entities.clear();_hud=nullptr;_hint=nullptr;_dialogue=nullptr;_face=nullptr;_tutorial=nullptr;_dialogue_panel=nullptr;clear_scene_objects();}
void FlightScene::on_reset(){on_exit();}
void FlightScene::on_shortcuts(const input::RawInputFrame&,const std::vector<input::RawInputEvent>& events) {
    using C=input::RawInputControl;
    for(const auto& e:events)if(e.type==input::RawInputEventType::ControlPressed) {
        if(_story.dialogue()&&(e.control==C::KeyEnter||e.control==C::MouseLeft||e.control==C::KeyTab)){consume_input(e);_story.advance(e.control==C::KeyTab);update_dialogue();return;}
        if(e.control==C::KeyEscape){consume_input(e);request_scene_switch(scene_keys::Selection,SelectionPayload{(_level-1)/15});return;}
        if(e.control==C::KeyR){consume_input(e);request_scene_switch(scene_keys::Flight,FlightPayload{_level,true},elysia::scene::SceneReloadMode::Recreate);return;}
        if(e.control==C::KeyEnter&&_finished){consume_input(e);
            continue_from_result();return;}
        if(e.control==C::KeyP&&!_finished&&_input_enabled&&!_story.dialogue()){consume_input(e);if(is_paused())resume();else pause();if(_player)_player->thrust={};return;}
    }
}
void FlightScene::on_routed_input(const input::InputSnapshot& snapshot) {
    if(!_player)return;_player->thrust={};if(is_paused()||_finished||_event_pending)return;
    if(_demo)return;
    if(!_input_enabled||snapshot.focus_lost)return;
    using C=input::RawInputControl;
    bool left=false,right=false,up=false,mouse=false;Vec point;
    for(const auto& source:snapshot.sources) {
        const auto& s=source.frame.state;
        left|=s.is_pressed(C::KeyA)||s.is_pressed(C::KeyLeft);
        right|=s.is_pressed(C::KeyD)||s.is_pressed(C::KeyRight);
        up|=s.is_pressed(C::KeyW)||s.is_pressed(C::KeyUp)||s.is_pressed(C::KeySpace);
        if(s.is_pressed(C::MouseLeft)){mouse=true;point={static_cast<float>(source.frame.mouse_x),static_cast<float>(source.frame.mouse_y)};}
    }
    if(up||left||right){const float angle=(right==left?0.f:right?1.f:-1.f)*50*std::numbers::pi_v<float>/180;_player->thrust={std::sin(angle),-std::cos(angle)};}
    if(mouse) {
        apply_pointer(camera().screen_to_world(point));
    }
}
void FlightScene::apply_pointer(Vec point) {
    _player->thrust=pointer_thrust(_player->position(),point,content::catalog().at("role"));
}
void FlightScene::finish(bool success) {
    if(_finished)return;_finished=true;_success=success;_player->thrust={};pause();
    _hint->set_text_content(ui::ui_raw_text(content::ui(success?"success_hint":"failure_hint")));
    auto& store=progress::Store::instance();
    const bool saved=success?store.record_clear(_level,_stars,_crystals,_crystal_count,_stars+_crystal_count,_elapsed):store.record_death(_level,_crystal_count);
    if(!saved)_hint->set_text_content(ui::ui_raw_text(content::ui("save_failure")));
    show_result(saved);
    ELYSIA_LOG_INFO("fly",std::format("Level {} {} time={:.3f} stars={} crystals={}",_level,success?"SUCCESS":"FAIL",_elapsed,_stars,_crystals));
}
void FlightScene::reached(bool success,const char* reason) {
    if(_finished||_event_pending)return;_event_pending=true;_player->thrust={};
    if(!success){++_deaths;_failure_reason=reason;ELYSIA_LOG_INFO("fly.failure",_failure_reason);_player->die();pause();}
    if(!_story.event(success?"onRoleReachedMoon":"onRoleDied"))finish(success);
    else if(!success)progress::Store::instance().record_death(_level,_crystal_count);
}
void FlightScene::restore_player() {
    _portal_lock=nullptr;
    for(auto* projectile:_projectiles)projectile->destroy();_projectiles.clear();
    physics_world().teleport_object(_player->physics_handle(),role_spawn({_data.at("born")[0],_data.at("born")[1]}),physics::TeleportVelocityMode::Clear);
    _player->dead=false;_player->set_update_when_paused(false);_player->thrust={};_stars=0;_crystals=0;_crystal_count=0;_elapsed=0;_event_pending=false;_finished=false;
    for(auto* entity:_entities)if(entity->item){entity->collected=false;entity->set_visible(true);physics_world().set_body_enabled(entity->physics_handle(),true);}
    _result->set_visible(false);_result->set_active(false);
    resume();camera_runtime().snap_to_focus(camera::CameraSlot::Main);
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
        const int index=a[0];_tutorial->set_visible(index!=0);if(index)_tutorial->set_texture(ELYSIA_RESOURCES->find_texture(index==1?"fly.TutorialUI.tutCharUp":"fly.TutorialUI.tutCharLeft"));
    }else if(op=="game.setReplayMode"){_demo=true;_demo_paused=true;}
    else if(op=="game.setPlayMode")_demo=false;
    else if(op=="game.loadTutorialRep"){
        _tutorial_replay.load(content::read("tutorial.json").at(a[0].get<std::string>()));
        ELYSIA_LOG_INFO("fly.story","Loaded original tutorial input: "+a[0].get<std::string>());
    }else if(op=="replay.skipNonControlFrames"){_tutorial_replay.skip_idle();_demo_paused=false;resume();}
    else if(op=="replay.pause"){_demo_paused=true;_player->thrust={};}
    else if(op=="role.loadState")restore_player();
    else if(op=="role.lookLeftRight")_player->look_time=3;
    else if(op=="role.setIdleActionEnabled")_player->idle_enabled=a[0];
    else if(op=="camera.setFollowRoleMode")_follow_role=a[0];
    else if(op=="camera.gotoPosition"){_story_camera={a[0],_data.at("height").get<float>()-a[1].get<float>()};camera_runtime().snap_to_focus(camera::CameraSlot::Main);}
    else if(op=="camera.gotoRole"){_story_camera=_player->center();camera_runtime().snap_to_focus(camera::CameraSlot::Main);}
    else if(op=="sound.play"){
        std::string key=a[0].is_string()?a[0].get<std::string>():std::to_string(a[0].get<int>());std::replace(key.begin(),key.end(),'-','_');
        const auto result=ELYSIA_AUDIO->request_sound("fly.voice_"+key,{.group=audio::SoundGroup::Extra});
        if(result.handle)_sounds.push_back(*result.handle);else ELYSIA_LOG_WARN("fly.audio","Original voice request rejected: "+key);
    }else if(op!="global.setBool")throw std::runtime_error("Unimplemented story operation "+op);
    return nullptr;
}
void FlightScene::update_dialogue() {
    const auto& line=_story.dialogue();const bool visible=line.has_value();_dialogue_panel->set_visible(visible);_dialogue->set_visible(visible);_face->set_visible(visible);
    if(visible&&_dialogue_key!=line->key){_dialogue_key=line->key;
        std::string text=_text.value(line->key,line->key);text=std::regex_replace(text,std::regex("#c[0-9a-fA-F]{6}|#n"),"");
        _dialogue->set_text_content(ui::ui_raw_text(text));_face->set_texture(ELYSIA_RESOURCES->find_texture("fly.ui."+line->face));
        ELYSIA_LOG_INFO("fly.story","Dialogue: "+line->key);
    }
    if(!visible)_dialogue_key.clear();
}
void FlightScene::on_fixed_update(std::uint64_t,double delta) {
    if(_demo&&!_demo_paused&&!_finished&&!_event_pending) {
        _player->thrust={};
        if(auto point=_tutorial_replay.next(_data.at("height")))apply_pointer(*point);
    }
    _wind_force={};
    if(!_finished&&!_event_pending) {
        for(auto* projectile:_projectiles)if(projectile->advance(delta,_player->position(),*_player)){_laser_hit=true;_pending_failure="projectile";}
        for(auto* entity:_entities){
            _wind_force+=entity->wind_force_at(_player->position());
            if(entity->update_laser(delta,_player->center(),*_player)){_laser_hit=true;_pending_failure="laser";}
            if(entity->update_bomb(delta,_player->center())){_laser_hit=true;_pending_failure="bomb";}
            const int shots=entity->shots_due(delta,_player->center());
            for(int i=0;i<shots;++i){
                const Vec direction=entity->fire_direction(_player->center());const bool homing=entity->kind==113;
                const float power=static_cast<float>(parameter(entity->info,"Power",homing?2.5:1.5));
                auto* projectile=create_and_add_object<Projectile>(entity->center()+direction*30,direction*(homing?64:power*160),homing,power,static_cast<float>(parameter(entity->info,"MaxSpeed",6)));
                if(!projectile||!projectile->physics_state())throw std::runtime_error("Projectile registration failed");
                _projectiles.push_back(projectile);++_shots_fired;if(homing)++_homing_fired;
            }
        }
        if(_wind_force.length_squared()>0&&!physics_world().apply_force(_player->physics_handle(),_wind_force))throw std::runtime_error("Wind force rejected");
    }
    _mechanism_time+=delta;
    for(auto it=_scheduled_actions.begin();it!=_scheduled_actions.end();) {
        if(it->first>_mechanism_time){++it;continue;}
        for(auto* entity:_entities)if(!entity->item&&entity->id==it->second){entity->trigger_start();ELYSIA_LOG_INFO("fly.trigger",std::format("Start entity {} at {:.3f}",entity->id,_mechanism_time));}
        it=_scheduled_actions.erase(it);
    }
}
void FlightScene::on_after_update(double delta) {
    if(!_player)return;
    std::erase_if(_projectiles,[](auto* p){if(p->expired){p->destroy();return true;}return false;});
    _story.update(delta);update_dialogue();
    if(_laser_hit){_laser_hit=false;reached(false,_pending_failure.c_str());}
    if(!is_paused()&&!_finished&&!_event_pending) {
        if(_portal_lock&&(_player->center()-_portal_lock->center()).length()>60)_portal_lock=nullptr;
        if(_input_enabled&&!_demo)_elapsed+=delta;
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
            if(entity->collected)continue;
            if(entity->lethal(hit.target.collider)){reached(false,"spike");continue;}
            if(hit.target.collider!=entity->physics_collider(0))continue;
            if(entity->item){entity->collected=true;entity->set_visible(false);physics_world().set_body_enabled(entity->physics_handle(),false);if(entity->kind==3)++_stars;else {_crystals+=entity->kind==5?5:1;++_crystal_count;}}
            else if(entity->kind==101)reached(true);
            else if(entity->kind==115&&entity!=_portal_lock&&entity->info.contains("TargetPortalID")) {
                const int target=static_cast<int>(parameter(entity->info,"TargetPortalID"));Entity* destination=nullptr;
                // Original levels normally reference object IDs. The single explicit
                // PortalID override in level 48 takes precedence over that fallback.
                for(auto* candidate:_entities)if(candidate->kind==115&&candidate->info.contains("PortalID")&&parameter(candidate->info,"PortalID")==target)destination=candidate;
                if(!destination)for(auto* candidate:_entities)if(candidate->kind==115&&candidate->id==target)destination=candidate;
                if(!destination)throw std::runtime_error("Portal destination absent");
                if(!physics_world().teleport_object(_player->physics_handle(),destination->center(),physics::TeleportVelocityMode::Preserve))throw std::runtime_error("Portal transit rejected");
                _portal_lock=destination;++_portal_transits;camera_runtime().snap_to_focus(camera::CameraSlot::Main);
                ELYSIA_LOG_INFO("fly.portal",std::format("Transit {} -> {}",entity->id,destination->id));
                return;
            }
        }
        if(_player->center().y>_data.at("height").get<float>()+100||_player->center().x< -100||_player->center().x>_data.at("width").get<float>()+100)reached(false,"out_of_bounds");
    }
    _hud->set_text_content(ui::ui_raw_text(content::format("hud",_level,_stars,_crystals,_elapsed,_finished?content::ui(_success?"done":"failed"):_demo?content::ui("demo"):is_paused()?content::ui("paused"):"")));
    if(_story.dialogue())_hint->set_text_content(ui::ui_raw_text(content::ui(_story.dialogue()->remaining>0?"wait":_story.skip_enabled()?"skip":"continue")));
    else if(!_finished)_hint->set_text_content(ui::ui_raw_text(content::ui("controls")));
    if(_probe.enabled()) {
    content::Json mechanisms=content::Json::array();
    for(const auto* entity:_entities)if(!entity->item&&((entity->kind>=105&&entity->kind<=107)||entity->rotating()))mechanisms.push_back({{"id",entity->id},{"kind",entity->kind},{"x",entity->position().x},{"y",entity->position().y},{"angle",entity->physics_state()->angle},{"mass",entity->physics_state()->mass},{"rotational_inertia",entity->physics_state()->rotational_inertia},{"angular_velocity",entity->physics_state()->angular_velocity}});
    if(_probe.update(runtime_context().renderer(),{{"paused",is_paused()},{"thrust_x",_player->thrust.x},{"thrust_y",_player->thrust.y},{"result_visible",_finished},{"deaths",_deaths},{"failure_reason",_failure_reason},{"detonated_bombs",std::count_if(_entities.begin(),_entities.end(),[](const auto* e){return e->bomb_state()>=2;})},{"pilot_view",{camera().view_rect().x(),camera().view_rect().y(),camera().zoom()}},{"scene",2},{"level",_level},{"success",_success},{"finished",_finished},{"stars",_stars},{"crystals",_crystals},{"crystal_count",_crystal_count},{"elapsed",_elapsed},{"dialogue",_dialogue_key},{"input_enabled",_input_enabled},{"demo",_demo},{"demo_frame",_tutorial_replay.cursor()},{"portal_transits",_portal_transits},{"active_lasers",std::count_if(_entities.begin(),_entities.end(),[](const auto* e){return e->laser_active();})},{"wind_x",_wind_force.x},{"wind_y",_wind_force.y},{"shots_fired",_shots_fired},{"homing_fired",_homing_fired},{"projectiles",_projectiles.size()},{"armed_bombs",std::count_if(_entities.begin(),_entities.end(),[](const auto* e){return e->bomb_state()>0;})},{"mechanisms",mechanisms},{"triggered_count",std::count(_triggered_areas.begin(),_triggered_areas.end(),true)},{"x",_player->center().x},{"y",_player->center().y},{"vx",_player->velocity().x},{"vy",_player->velocity().y}}))request_quit();
    }
}
std::optional<camera::CameraFocus> FlightScene::resolve_camera_focus(camera::CameraSlot)const {
    if(!_player)return std::nullopt;if(!_follow_role){const Rect focus{_story_camera.x-1,_story_camera.y-1,2,2};return camera::CameraFocus{focus,focus};}return camera::CameraFocus{_player->render_rect(),_player->render_rect()};
}
}
