#pragma once
// Included after the engine SceneTestAccess fixture in visual_tests.cpp.
namespace game::scene {
class AudioTestAccess {
    static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
    static int count(const FlightScene& scene,const char* file){const auto& events=scene._audio.events();const auto at=events.find(file);return at==events.end()?0:at->second;}
public:
    static void run(FlightScene& scene){
        scene._audio.reset();scene._player->play_effect(play::RoleEffect::Enter);scene.start_entrance_sound();
        scene._player->update(.02);scene.update_audio(.02);check(count(scene,"70-1")==0,"Entrance sound played before original frame 1");
        scene._player->update(.02);scene.update_audio(.02);check(count(scene,"70-1")==1,"Entrance frame 1 did not request 70-1");
        scene.update_audio(.02);check(count(scene,"70-1")==1,"Entrance sound repeated without another entrance");
        scene._level=31;scene.story_action("sound.play",content::Json::array({53}));
        check(count(scene,"55-2")==1,"Level 31 numeric subtraction was not repaired by the runtime adapter");
        scene._level=20;scene.story_action("sound.play",content::Json::array({"44-1"}));
        check(count(scene,"44-1")==1,"Level 20 alias failed in the actual story bridge");scene._level=1;
        scene._opening_phase=FlightScene::OpeningPhase::Ready;scene._camera_move={};scene._follow_role=true;
        scene._player->play_effect(play::RoleEffect::None);scene._player->present_at(std::nullopt);scene.resume();
        for(int kind:{3,4,5}){
            auto found=std::ranges::find_if(scene._entities,[kind](const auto* entity){return entity->item&&entity->kind==kind;});
            if(found==scene._entities.end())continue;
            check(scene.physics_world().teleport_object(scene._player->physics_handle(),(*found)->center(),elysia::physics::TeleportVelocityMode::Clear),"Pickup test teleport failed");
            elysia::scene::SceneTestAccess::update(scene,1./60.);
            check((*found)->collected,"Actual pickup overlap did not collect the test item");
        }
        check(count(scene,"33-6")>0&&count(scene,"90-1")>0,"Actual pickup events did not request original sounds");
        scene._audio.set_listener(scene._player->center());
        scene.reached(false,"spike");check(count(scene,"12-4")==1,"Spike hit must request the native hit effect");
        check(count(scene,"47-1")+count(scene,"48-1")==1,"Death must choose one original variant");
        scene.reached(false,"spike");check(count(scene,"12-4")==1&&count(scene,"47-1")+count(scene,"48-1")==1,"Spike/death effects repeated");
        scene.restore_player();scene._player->update(.04);scene.update_audio(.04);check(count(scene,"70-1")==1,"Scripted restore did not replay entrance sound");
        scene._opening_phase=FlightScene::OpeningPhase::Ready;scene._camera_move={};scene._event_pending=false;
        scene.begin_moon(scene._player->center());check(count(scene,"70-2")==1,"Moon transition did not request its sound");
        scene.begin_moon(scene._player->center());check(count(scene,"70-2")==1,"Moon transition repeated its sound");
        scene._event_pending=false;scene._transition={};scene._elapsed=10;scene._stars=3;scene.finish(true);
        scene.update_result_animation(0);check(scene._result_stars->collected()==0&&count(scene,"106-1")==1,"First result sound must start with particles, before the star lights");
        scene.update_result_animation(.19);check(scene._result_stars->collected()==0,"Result star lit before its 0.2 second particle lead-in");
        scene.update_result_animation(.01);check(scene._result_stars->collected()==1,"First result star did not light after its particle lead-in");
        scene.update_result_animation(.3);check(scene._result_stars->collected()==1&&count(scene,"106-2")==1,"Second burst must sound before its star lights");
        scene.update_result_animation(.2);check(scene._result_stars->collected()==2,"Second result star timing incorrect");
        scene.update_result_animation(.3);check(scene._result_stars->collected()==2&&count(scene,"106-3")==1,"Third burst timing incorrect");
        scene.update_result_animation(.2);check(scene._result_stars->collected()==3,"Third result star timing incorrect");
        for(int i=0;i<1800&&!scene._result_animation_done;++i)scene.update_result_animation(1./60.);
        check(scene._result_animation_done&&scene._result_display_score==scene._result_score&&count(scene,"107-1")>0&&count(scene,"109-1")==1,"Result score and high-score sequence incomplete");
        scene._audio.clear();scene._finished=false;scene._event_pending=false;scene._death_pending=false;scene._elapsed=10;scene._stars=3;
        scene.finish(true);scene.continue_from_result();check(scene._result_animation_done,"First result confirmation must finish the presentation before leaving");
        std::vector<elysia::core::UiRenderCommand> skipped;scene._result_stars->submit_ui_render_commands(skipped);
        check(scene._result_stars->collected()==3&&skipped.size()==3,"Skipping settlement must light all stars and remove pending particles");
        for(const auto& [file,n]:scene._audio.events())check(elysia::resources::ResourceService::instance()->find_sound(audio::sound_key(file))!=nullptr,"Scene requested an unregistered sound");
    }
};
}
namespace game::testing {
inline void check_scene_audio(SDL_Renderer* renderer,const elysia::io::ContentRegistry& registry){
    const auto directory=std::filesystem::path("out/qa/audio-scene")/std::to_string(SDL_GetTicksNS());
    if(!ELYSIA_SAVE->initialize(directory))throw std::runtime_error("Audio fixture save initialization failed");
    if(!ELYSIA_AUDIO->initialize({}))throw std::runtime_error("Audio fixture mixer initialization failed");
    elysia::scene::SceneRuntimeContext context(renderer,registry,1280,720);
    game::progress::Store::instance().data=game::progress::Progress{};
    game::scene::FlightScene flight;
    elysia::scene::SceneTestAccess::bind(flight,context);elysia::scene::SceneTestAccess::enter(flight,game::scene::FlightPayload{1});
    game::scene::AudioTestAccess::run(flight);elysia::scene::SceneTestAccess::exit(flight);
    for(auto* track:elysia::audio::detail::mixer_backend().tracks)if(MIX_TrackPlaying(track)||MIX_TrackPaused(track))throw std::runtime_error("Scene exit leaked an audio track");
    game::scene::SelectionScene selection;elysia::scene::SceneTestAccess::bind(selection,context);
    game::progress::Store::instance().pending_unlocks.insert(2);
    elysia::scene::SceneTestAccess::enter(selection,game::scene::SelectionPayload{0});
    elysia::scene::SceneTestAccess::update(selection,.3);
    const auto playing=[&](const char* file){
        auto* sound=ELYSIA_RESOURCES->find_sound(game::audio::sound_key(file));
        return std::ranges::any_of(elysia::audio::detail::mixer_backend().tracks,[&](auto* track){return MIX_GetTrackAudio(track)==sound&&MIX_TrackPlaying(track);});
    };
    if(!playing("108-1"))throw std::runtime_error("Newly unlocked level did not play its reveal effect");
    auto& window=elysia::scene::SceneTestAccess::window(selection);window.update(0);
    auto& list=dynamic_cast<elysia::ui::UiListContainer&>(*window.child_at(0));
    auto& grid=dynamic_cast<game::frontend::ConstellationGrid&>(*list.child_at(2));
    auto* locked=grid.child_at(14);const auto point=locked->screen_rect().center();
    elysia::ui::UiInputEvent event{.type=elysia::ui::UiInputEventType::PointerPressed,.device=elysia::input::InputDevice::Mouse,.control=elysia::input::RawInputControl::MouseLeft,.mouse_x=static_cast<int>(point.x),.mouse_y=static_cast<int>(point.y)};
    window.on_ui_input_event(event);window.on_ui_input_frame({});window.update(1./60.);
    event.type=elysia::ui::UiInputEventType::PointerReleased;window.on_ui_input_event(event);
    if(!playing("7-4"))throw std::runtime_error("Clicking a locked level did not play the original rejection sound");
    elysia::scene::SceneTestAccess::exit(selection);
    ELYSIA_AUDIO->shutdown();ELYSIA_SAVE->shutdown();
    std::cout<<"PASS: real FlightScene entrance, script aliases, pickups, spike/death/restore, moon, result timing/skip/cleanup; SelectionScene unlock and locked-node sound routing\n";
}
}
