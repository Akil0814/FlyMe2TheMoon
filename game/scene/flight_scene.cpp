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

namespace game::scene {
using namespace elysia;
using namespace play;
FlightScene::FlightScene():Scene(elysia::scene::SceneRuntimeFeatures{
    .fixed_step=elysia::scene::FixedStepConfig{},
    .physics=physics::PhysicsWorldConfig{.units_per_meter=32,.gravity={0,320}},
    .camera=elysia::scene::CameraSceneConfig{.focus_mode=elysia::scene::CameraFocusMode::ResolveEachFrame,.advance_when_paused=true}}) {}
void FlightScene::on_enter(const elysia::scene::ScenePayload& payload) {
    clear_scene_objects();_entities.clear();_player=nullptr;
    if(const auto* p=elysia::scene::try_scene_payload<FlightPayload>(payload)){_level=p->level;_restart=p->restart;}
    _data=content::level(_level);_stars=0;_crystals=0;_crystal_count=0;_elapsed=0;_finished=false;_success=false;
    (void)progress::Store::instance().load();
    _text=content::read("text/zh-Hans/strings.json");_input_enabled=true;_demo=false;_demo_paused=false;_event_pending=false;_follow_role=true;_dialogue_key.clear();
    _mechanism_time=0;_triggered_areas.assign(_data.at("areas").size(),false);_scheduled_actions.clear();
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
    for(auto* entity:_entities)if(entity->rotating()) {
        const float rad=entity->angle*std::numbers::pi_v<float>/180;
        const Vec local{static_cast<float>((parameter(entity->info,"Anchor",.5)-.5)*entity->size().x),0};
        const Vec anchor=entity->origin+Vec{local.x*std::cos(rad),local.x*std::sin(rad)};
        auto* pivot=create_and_add_object<Pivot>(anchor);
        const auto joint=physics_world().create_revolute_joint({.first=pivot->physics_handle(),.second=entity->physics_handle(),.local_anchor_first={},.local_anchor_second=local,
            .enable_motor=entity->info.contains("Speed"),.motor_speed=static_cast<float>(parameter(entity->info,"Speed")*std::numbers::pi/180),.max_motor_torque=static_cast<float>(parameter(entity->info,"Torque",30)*1024)});
        if(!joint.is_valid())throw std::runtime_error("Rotating bar joint creation failed");
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
            const int next=_level+1;
            if(!_success)request_scene_switch(scene_keys::Flight,FlightPayload{_level,true},elysia::scene::SceneReloadMode::Recreate);
            else if(next<=60&&progress::Store::instance().data.unlocked(next,content::catalog().at("dependencies")))request_scene_switch(scene_keys::Flight,FlightPayload{next},elysia::scene::SceneReloadMode::Recreate);
            else request_scene_switch(scene_keys::Selection,SelectionPayload{(_level-1)/15});return;}
        if(e.control==C::KeyP&&!_finished&&_input_enabled&&!_story.dialogue()){consume_input(e);if(is_paused())resume();else pause();if(_player)_player->thrust={};return;}
    }
}
void FlightScene::on_routed_input(const input::InputSnapshot& snapshot) {
    if(!_player)return;_player->thrust={};if(is_paused()||_finished||_event_pending)return;
    if(_demo){if(!_demo_paused)_player->thrust={0,-1};return;}
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
    if(up||left||right)_player->thrust={right==left?0.f:right?.65f:-.65f,up?-1.f:-.5f};
    if(mouse) {
        point=camera().screen_to_world(point);Vec dir=_player->center()-point;
        const float length=std::hypot(dir.x,dir.y);
        if(length>4)_player->thrust={std::clamp(dir.x/length,-.8f,.8f),std::min(-.2f,dir.y/length)};
    }
}
void FlightScene::finish(bool success) {
    if(_finished)return;_finished=true;_success=success;_player->thrust={};pause();
    _hint->set_text_content(ui::ui_raw_text(success?"Moon reached! Enter: next level   R: retry for all stars   Esc: menu":"Kiana fell! Enter / R: retry   Esc: menu"));
    auto& store=progress::Store::instance();
    const bool saved=success?store.record_clear(_level,_stars,_crystals,_crystal_count,_stars+_crystal_count,_elapsed):store.record_death(_level,_crystal_count);
    if(!saved)_hint->set_text_content(ui::ui_raw_text("Progress could not be saved. R: retry    Esc: menu. Existing save retained."));
    ELYSIA_LOG_INFO("fly",std::format("Level {} {} time={:.3f} stars={} crystals={}",_level,success?"SUCCESS":"FAIL",_elapsed,_stars,_crystals));
}
void FlightScene::reached(bool success) {
    if(_finished||_event_pending)return;_event_pending=true;_player->thrust={};
    if(!success){_player->dead=true;pause();}
    if(!_story.event(success?"onRoleReachedMoon":"onRoleDied"))finish(success);
    else if(!success)progress::Store::instance().record_death(_level,_crystal_count);
}
void FlightScene::restore_player() {
    physics_world().teleport_object(_player->physics_handle(),{_data.at("born")[0].get<float>()-24,_data.at("born")[1].get<float>()-48},physics::TeleportVelocityMode::Clear);
    _player->dead=false;_player->thrust={};_stars=0;_crystals=0;_crystal_count=0;_elapsed=0;_event_pending=false;_finished=false;
    for(auto* entity:_entities)if(entity->item){entity->collected=false;entity->set_visible(true);physics_world().set_body_enabled(entity->physics_handle(),true);}
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
        if(a[0]!="Rep_rd_1")throw std::runtime_error("Unknown tutorial replay");
        ELYSIA_LOG_INFO("fly.story","Original Rep_rd_1 file missing: demonstrating vertical thrust through the normal physics path");
    }else if(op=="replay.skipNonControlFrames"){_demo_paused=false;resume();}
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
    _mechanism_time+=delta;
    for(auto it=_scheduled_actions.begin();it!=_scheduled_actions.end();) {
        if(it->first>_mechanism_time){++it;continue;}
        for(auto* entity:_entities)if(!entity->item&&entity->id==it->second){entity->trigger_start();ELYSIA_LOG_INFO("fly.trigger",std::format("Start entity {} at {:.3f}",entity->id,_mechanism_time));}
        it=_scheduled_actions.erase(it);
    }
}
void FlightScene::on_after_update(double delta) {
    if(!_player)return;
    _story.update(delta);update_dialogue();
    if(!is_paused()&&!_finished&&!_event_pending) {
        if(_input_enabled&&!_demo)_elapsed+=delta;
        for(size_t i=0;i<_triggered_areas.size();++i)if(!_triggered_areas[i]) {
            const auto& area=_data.at("areas")[i];const Vec p=_player->position()+Vec{24,34};
            if(p.x+14<area.at("x").get<float>()||p.x-14>area.at("x").get<float>()+area.at("w").get<float>()||p.y+14<area.at("y").get<float>()||p.y-14>area.at("y").get<float>()+area.at("h").get<float>())continue;
            _triggered_areas[i]=true;
            for(const auto& action:area.at("actions")) {
                if(action.at("Action")!="Start")throw std::runtime_error("Unknown area action");
                const std::string target=action.at("Target");if(!target.starts_with("4:"))throw std::runtime_error("Unknown area target layer");
                _scheduled_actions.emplace_back(_mechanism_time+parameter(action,"Delay"),std::stoi(target.substr(2)));
            }
        }
        std::vector<physics::CollisionOverlapQueryHit> hits;
        physics_world().overlap_circle({.center=_player->position()+Vec{24,34},.radius=14,.filter={.category=1,.mask=4|8}},hits);
        for(const auto& hit:hits)for(auto* entity:_entities) {
            if(entity->collected)continue;
            if(entity->lethal(hit.target.collider)){reached(false);continue;}
            if(hit.target.collider!=entity->physics_collider(0))continue;
            if(entity->item){entity->collected=true;entity->set_visible(false);physics_world().set_body_enabled(entity->physics_handle(),false);if(entity->kind==3)++_stars;else {_crystals+=entity->kind==5?5:1;++_crystal_count;}}
            else if(entity->kind==101)reached(true);
        }
        if(_player->center().y>_data.at("height").get<float>()+100||_player->center().x< -100||_player->center().x>_data.at("width").get<float>()+100)reached(false);
    }
    _hud->set_text_content(ui::ui_raw_text(std::format("月之残境    {:02} / 60       星星  {} / 3       水晶  {}       {:.1f} 秒{}",_level,_stars,_crystals,_elapsed,_finished?(_success?"    完成":"    失败"):_demo?"    飞行示范":is_paused()?"    暂停":"")));
    if(_story.dialogue())_hint->set_text_content(ui::ui_raw_text(_story.dialogue()->remaining>0?"请稍候……":_story.skip_enabled()?"Enter / 单击：继续      Tab：跳过已读剧情":"Enter / 单击：继续"));
    else if(!_finished)_hint->set_text_content(ui::ui_raw_text("按住 Space / W 上升 · A / D 转向 · 鼠标按住角色下方施放魔法    P 暂停    R 重试    Esc 选关"));
    if(_probe.enabled()) {
    content::Json mechanisms=content::Json::array();
    for(const auto* entity:_entities)if(!entity->item&&((entity->kind>=105&&entity->kind<=107)||entity->rotating()))mechanisms.push_back({{"id",entity->id},{"kind",entity->kind},{"x",entity->position().x},{"y",entity->position().y},{"angle",entity->physics_state()->angle}});
    if(_probe.update(runtime_context().renderer(),{{"scene",2},{"level",_level},{"success",_success},{"finished",_finished},{"stars",_stars},{"crystals",_crystals},{"crystal_count",_crystal_count},{"elapsed",_elapsed},{"dialogue",_dialogue_key},{"input_enabled",_input_enabled},{"demo",_demo},{"mechanisms",mechanisms},{"triggered_count",std::count(_triggered_areas.begin(),_triggered_areas.end(),true)},{"x",_player->center().x},{"y",_player->center().y},{"vx",_player->velocity().x},{"vy",_player->velocity().y}}))request_quit();
    }
}
std::optional<camera::CameraFocus> FlightScene::resolve_camera_focus(camera::CameraSlot)const {
    if(!_player)return std::nullopt;if(!_follow_role){const Rect focus{_story_camera.x-1,_story_camera.y-1,2,2};return camera::CameraFocus{focus,focus};}return camera::CameraFocus{_player->render_rect(),_player->render_rect()};
}
}
