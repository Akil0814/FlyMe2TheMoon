#pragma once
#include "game/scene/flight_scene.h"
#include "game/scene/selection_scene.h"
#include "game/scene/credits_scene.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/widgets/image/ui_image.h"

namespace game::scene {
class FlightUiTestAccess {
    using Rect=elysia::core::Rect;
    using Command=elysia::core::UiRenderCommand;
    static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
    static auto commands(const elysia::ui::UiElement& element){std::vector<Command> out;element.submit_ui_render_commands(out);return out;}
    static void contained(const elysia::ui::UiElement& element,Rect area){
        for(const auto& command:commands(element))check(command.screen_rect.x()>=area.x()-.01f&&command.screen_rect.y()>=area.y()-.01f&&command.screen_rect.right()<=area.right()+.01f&&command.screen_rect.bottom()<=area.bottom()+.01f,"Flight UI command escaped its allotted area");
    }
    static const Command& texture(const std::vector<Command>& drawn,const char* key){
        auto* expected=ELYSIA_RESOURCES->find_texture(key);
        const auto found=std::find_if(drawn.begin(),drawn.end(),[&](const auto& c){return c.texture==expected;});
        check(expected&&found!=drawn.end(),"Expected tutorial sprite is missing");return *found;
    }
    static void click(elysia::ui::UiWindow& window,const elysia::ui::UiElement& element){
        using namespace elysia;
        const auto point=element.screen_rect().center();
        ui::UiInputEvent event{.type=ui::UiInputEventType::PointerPressed,.device=input::InputDevice::Mouse,.control=input::RawInputControl::MouseLeft,.mouse_x=static_cast<int>(point.x),.mouse_y=static_cast<int>(point.y)};
        window.on_ui_input_event(event);window.on_ui_input_frame({});window.update(1./60.);
        event.type=ui::UiInputEventType::PointerReleased;window.on_ui_input_event(event);window.on_ui_input_frame({});window.update(1./60.);
    }
    static void modal(elysia::ui::UiWindow& window){
        window.update(0);
        check(window.screen_rect()==Rect{0,0,640,960}&&window.style().corner_radius==0,"Flight modal shade must cover all four portrait corners");
        check(window.child_at(0)->screen_rect()==Rect{64,200,512,560},"Flight modal must be centered at its portrait size");
        auto& list=dynamic_cast<elysia::ui::UiListContainer&>(*window.child_at(1));
        check(list.screen_rect().nearly_equals({104,230,432,500}),"Flight modal content must fit inside the frame");
        for(std::size_t i=0;i<list.child_count();++i)contained(*list.child_at(i),list.screen_rect());
    }
public:
    static void run(FlightScene& scene,const std::function<void(const char*)>& capture,bool hud_only=false){
        using namespace elysia;
        struct Requests final:elysia::scene::SceneRequestObserver {
            std::vector<elysia::scene::SceneRequest> items;
            void on_scene_request(const elysia::scene::SceneRequest& request)override{items.push_back(request);}
        } requests;
        scene.attach(&requests);
        scene._opening_phase=FlightScene::OpeningPhase::Ready;scene._camera_move={};scene._event_pending=false;scene._follow_role=true;
        scene._player->present_at(std::nullopt);scene._player->play_effect(play::RoleEffect::None);
        scene.camera_runtime().snap_to_focus(camera::CameraSlot::Main);scene.resume();
        scene._stars=2;scene._elapsed=123.4;scene._crystals=27;scene._crystal_count=15;scene.on_after_update(0);
        check(scene._hud->screen_rect()==Rect{12,12,112,40}&&scene._hud_stats->screen_rect()==Rect{132,12,316,40}&&scene._hud_stars->screen_rect()==Rect{460,16,104,32}&&scene._pause_button->screen_rect()==Rect{580,10,44,44},"Phone HUD must use one compact row ordered stage, time, crystals, stars, pause");
        const auto& roots=elysia::scene::SceneTestAccess::roots(scene);
        check(std::count_if(roots.begin(),roots.end(),[](const auto& root){return dynamic_cast<ui::UiLabel*>(root.get())!=nullptr;})==1,"The bottom instruction bar must be removed, not merely hidden");
        check(scene._hud->is_visible()&&scene._hud_stats->is_visible()&&scene._hud_stars->is_visible(),"Normal flight must show all HUD counters");
        check(scene._pause_button->order()>scene._tutorial->order()&&scene._tutorial->order()>scene._dialogue->order(),"Pause and Demo pointer must remain above tutorial/dialogue artwork");
        contained(*scene._hud,scene._hud->screen_rect());contained(*scene._hud_stats,scene._hud_stats->screen_rect());
        scene._hud_stats->set_values(99999.9,9999,9999);contained(*scene._hud_stats,scene._hud_stats->screen_rect());
        scene._hud_stats->set_values(scene._elapsed,3,12);capture("hud");
        if(hud_only){scene.detach(&requests);return;}

        scene._star_pickups->launch({120,600},scene._hud_stars->slot_rect(1).center());scene._star_pickups->advance(.35);
        scene._tutorial->set_type(1);scene.update_hud_visibility();
        check(!scene._hud->is_visible()&&!scene._hud_stats->is_visible()&&!scene._hud_stars->is_visible()&&!scene._star_pickups->is_visible()&&scene._pause_button->is_visible(),"Teaching must hide counters and pickup drawing but keep Pause visible");
        check(commands(*scene._star_pickups).empty()&&scene._star_pickups->pending_count()==1,"Hidden pickups must retain their independent flight state");
        auto diagram=commands(*scene._tutorial);
        check(texture(diagram,"fly.TutorialUI.tutBubble").screen_rect==Rect{34,10,572,633},"Teaching bubble must use the original doubled phone coordinates");
        const auto doll=texture(diagram,"fly.TutorialUI.tutCharStand").screen_rect;
        check(std::abs(doll.x()-281.6f)<.001f&&std::abs(doll.y()-187.8f)<.001f,"Teaching doll must use its original top-left anchor");
        scene._tutorial->advance(1);diagram=commands(*scene._tutorial);
        check(texture(diagram,"fly.TutorialUI.tutDown").screen_rect.position().nearly_equals({262,318})&&texture(diagram,"fly.TutorialUI.tutArrow").screen_rect.center().nearly_equals({320,254.4f}),"First teaching press and centered arrow coordinates changed");
        std::string longest;
        for(const auto& [key,value]:scene._text.items())if(value.is_string()&&value.get_ref<const std::string&>().size()>longest.size())longest=value.get<std::string>();
        scene._dialogue->set_story_text(longest);scene._dialogue->reveal();
        scene._dialogue_panel->set_visible(true);scene._portrait_panel->set_visible(true);scene._face->set_visible(true);scene._dialogue->set_visible(true);
        scene._face->set_texture(ELYSIA_RESOURCES->find_texture("fly.ui.facePutong"));
        check(scene._dialogue_panel->screen_rect()==Rect{16,688,608,248}&&scene._dialogue->screen_rect()==Rect{156,704,452,216},"Phone story layout must sit above the bottom margin");
        contained(*scene._dialogue,scene._ui_layout.text);capture("tutorial");
        scene._tutorial->set_type(2);
        const auto frames=content::read("tutorial_visual.json").at("direction_frames");
        constexpr const char* dolls[]{"tutCharStand","tutCharLeft","tutCharRight","tutCharFall","tutCharUp"};
        for(int frame=0;frame<80;++frame){
            diagram=commands(*scene._tutorial);contained(*scene._tutorial,scene._ui_layout.viewport);
            check(scene._tutorial->frame()==frame,"Second teaching animation lost its original 10 Hz cadence");
            texture(diagram,(std::string("fly.TutorialUI.")+dolls[frames[frame].at("doll").get<int>()]).c_str());
            for(const auto& [field,key]:std::array<std::pair<const char*,const char*>,3>{{{"up","tutUp"},{"down","tutDown"},{"blank","tutBlank"}}}){
                const auto& point=frames[frame].at(field);if(point[0]==0&&point[1]==0)continue;
                const auto position=texture(diagram,(std::string("fly.TutorialUI.")+key).c_str()).screen_rect.position();
                check(position.nearly_equals({point[0].get<float>()*2,960-point[1].get<float>()*2}),"Second teaching lookup position must map directly from the original coordinates");
            }
            scene._tutorial->advance(.1);
        }
        scene._tutorial->set_type(0);scene.update_hud_visibility();capture("dialogue");
        scene._demo=true;scene._tutorial->set_demo(true);scene.update_hud_visibility();
        auto demo=commands(*scene._tutorial);
        check(texture(demo,"fly.InGameUI.fingerUp").screen_rect.center().nearly_equals({320,840}),"Demo pointer must start at the original phone position");
        const auto demo_label=std::string("fly.frontend.tutorial_demo_")+(content::language()=="zh-Hans"?"zh":content::language());
        check(std::abs(texture(demo,demo_label.c_str()).screen_rect.y()-64)<.001f,"Demo label must begin at original top offset 64");
        const auto input_world=scene.camera().screen_to_world({400,550});
        scene._tutorial_replay.load(content::Json{{"frames",content::Json::array({content::Json::array({input_world.x,scene._data.at("height").get<float>()-input_world.y}),nullptr})}});
        scene._tutorial_replay.next(scene._data.at("height"));scene.on_after_update(0);demo=commands(*scene._tutorial);
        check(texture(demo,"fly.InGameUI.fingerDown").screen_rect.center().nearly_equals(scene.camera().world_to_screen(*scene._tutorial_replay.last_point())),"Demo hand must follow the actual replay input projected through the current camera");
        scene._tutorial_replay.next(scene._data.at("height"));scene.on_after_update(0);demo=commands(*scene._tutorial);
        check(texture(demo,"fly.InGameUI.fingerUp").screen_rect.center().nearly_equals({400,550}),"Released Demo hand must retain its last input location");
        check(!scene._hud->is_visible()&&!scene._star_pickups->is_visible(),"Automatic demonstration must keep ordinary HUD hidden");
        capture("demo");

        scene._tutorial->set_type(1);scene._dialogue->set_story_text("Pause freezes text");scene._dialogue->advance(.05);
        const auto frame=scene._tutorial->frame();const auto characters=scene._dialogue->visible_count();
        scene._star_pickups->set_visible(true);const auto flying=commands(*scene._star_pickups);scene._star_pickups->set_visible(false);
        scene.open_pause_menu();modal(*scene._pause_menu);scene.on_after_update(.2);
        check(scene._tutorial->frame()==frame&&scene._dialogue->visible_count()==characters&&!scene._tutorial->is_visible(),"Pause menu must freeze and hide teaching and story presentation");
        scene._star_pickups->set_visible(true);const auto frozen=commands(*scene._star_pickups);scene._star_pickups->set_visible(false);
        check(frozen.size()==flying.size()&&frozen.front().screen_rect==flying.front().screen_rect,"Pause must freeze an in-flight star even while its drawing is hidden");
        input::InputSystem inputs;inputs.initialize();
        for(bool down:{true,false}){
            inputs.begin_frame();SDL_Event event{};event.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;event.button.button=SDL_BUTTON_LEFT;event.button.down=down;event.button.x=8;event.button.y=900;inputs.process_event(event);
            elysia::scene::SceneTestAccess::input(scene,inputs.snapshot());
        }
        check(scene._pause_menu_open&&scene._player->thrust.length_squared()==0,"Tapping outside the pause frame must not resume or thrust");inputs.shutdown();capture("pause");
        auto& pause_list=dynamic_cast<ui::UiListContainer&>(*scene._pause_menu->child_at(1));click(*scene._pause_menu,*pause_list.child_at(5));
        check(!scene._pause_menu_open&&scene._tutorial->is_visible(),"Resume must close Pause and restore the tutorial");
        scene._demo=false;scene._tutorial->set_demo(false);scene._tutorial->set_type(0);scene.update_dialogue();scene.on_after_update(.4);
        check(scene._hud->is_visible()&&scene._star_pickups->pending_count()==0&&scene._hud_stars->collected()==scene._stars,"Leaving teaching must restore counters and finish the hidden pickup flight");

        scene._finished=true;scene._success=true;scene._stars=3;scene._elapsed=123.4;scene.pause();scene.show_result(true,true);
        modal(*scene._result);scene.update_result_animation(.1);capture("result-stars");
        scene.update_result_animation(8);check(scene._result_animation_done&&scene._result_stars->collected()==3&&scene._result_display_score==scene._result_score,"Portrait settlement must finish stars, score and record presentation");capture("success");
        auto& result_list=dynamic_cast<ui::UiListContainer&>(*scene._result->child_at(1));
        scene._success=false;scene.show_result(true,false);modal(*scene._result);check(!scene._result_continue->is_visible(),"Failed settlement must not offer Next Level");capture("failure");
        click(*scene._result,*result_list.child_at(6));
        check(requests.items.size()==1&&requests.items.back().route.target==scene_keys::Flight&&elysia::scene::try_scene_payload<FlightPayload>(requests.items.back().route.payload)->restart,"Retry must preserve the level restart route");requests.items.clear();
        click(*scene._result,*result_list.child_at(7));check(requests.items.size()==1&&requests.items.back().route.target==scene_keys::Selection,"Settlement selection button must retain its route");requests.items.clear();
        scene._success=true;scene.show_result(false,false);modal(*scene._result);
        check(scene._result_notice->is_visible()&&scene._result_notice->text_content().value==content::ui("save_failure"),"Save failure must remain visible after removing the bottom hint bar");capture("save-failure");
        scene.complete_result_animation();progress::Store::instance().data.stars[0]=3;click(*scene._result,*scene._result_continue);
        check(requests.items.size()==1&&requests.items.back().route.target==scene_keys::Flight,"Next Level must preserve the unlocked next-level route");requests.items.clear();
        scene._level=60;scene.show_result(true,false);scene.complete_result_animation();click(*scene._result,*scene._result_continue);
        check(requests.items.size()==1&&requests.items.back().route.target==scene_keys::Credits,"Final-level settlement must still enter credits");
        scene.detach(&requests);
    }
};
}

namespace game::testing {
inline void check_flight_ui(SDL_Renderer* renderer,const elysia::io::ContentRegistry& registry,bool hud_only=false){
    using namespace elysia;
    const auto language=ELYSIA_LOCALIZATION->current_language();auto& store=progress::Store::instance();const auto progress=store.data;
    const auto directory=std::filesystem::path("out/qa/flight-ui")/std::to_string(SDL_GetTicksNS());std::filesystem::create_directories(directory);
    if(!ELYSIA_SAVE->initialize(directory)||!ELYSIA_AUDIO->initialize({}))throw std::runtime_error("Flight UI fixture initialization failed");
    auto* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,640,960);
    if(!target)throw std::runtime_error(SDL_GetError());
    for(const auto& locale:ELYSIA_LOCALIZATION->supported_languages()){
        if(!ELYSIA_LOCALIZATION->set_language(locale))throw std::runtime_error("Flight UI language switch failed");
        elysia::scene::SceneRuntimeContext context(renderer,registry,640,960);game::scene::FlightScene flight;
        elysia::scene::SceneTestAccess::bind(flight,context);elysia::scene::SceneTestAccess::enter(flight,game::scene::FlightPayload{1});
        game::scene::FlightUiTestAccess::run(flight,[&](const char* state){
            if(locale!="zh-Hans"&&!hud_only)return;
            if(!SDL_SetRenderTarget(renderer,target)||!SDL_SetRenderDrawColor(renderer,4,13,36,255)||!SDL_RenderClear(renderer))throw std::runtime_error(SDL_GetError());
            elysia::scene::SceneTestAccess::render(flight,renderer);
            auto* surface=SDL_RenderReadPixels(renderer,nullptr);if(!surface)throw std::runtime_error(SDL_GetError());
            const auto path=hud_only?std::format("out/qa/flight-portrait-{}-{}-preview.bmp",state,locale):std::format("out/qa/flight-portrait-{}-preview.bmp",state);
            const bool saved=SDL_SaveBMP(surface,path.c_str());SDL_DestroySurface(surface);
            if(!saved||!SDL_SetRenderTarget(renderer,nullptr))throw std::runtime_error(SDL_GetError());
        },hud_only);
        elysia::scene::SceneTestAccess::exit(flight);
    }
    SDL_DestroyTexture(target);store.data=progress;
    if(!ELYSIA_LOCALIZATION->set_language(language))throw std::runtime_error("Flight UI language restore failed");
    ELYSIA_AUDIO->shutdown();ELYSIA_SAVE->shutdown();
}
}
