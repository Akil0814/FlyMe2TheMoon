#pragma once
#include "game/platform/web_input.h"
#include "engine/input/input_system.h"
namespace game::scene {
class WebInputTestAccess {
    static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
public:
    static void run_web_input(FlightScene& scene){
        using namespace elysia;
        scene._opening_phase=FlightScene::OpeningPhase::Ready;scene._camera_move={};scene._event_pending=false;
        scene._story_started=false;scene._input_enabled=true;scene._demo=false;scene.resume();
        scene._player->present_at(std::nullopt);scene._player->play_effect(play::RoleEffect::None);
        scene.camera_runtime().snap_to_focus(camera::CameraSlot::Main);
        const auto world=scene._player->position()+core::Vector2{0,140};
        const auto screen=scene.camera().world_to_screen(world);
        check((scene.camera().screen_to_world(screen)-world).length()<.01f,"Web pointer projection changed");
        SDL_Event event{};while(SDL_PollEvent(&event)){}
        input::InputSystem inputs;inputs.initialize();inputs.begin_frame();
        event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_LEFT;event.button.x=screen.x;event.button.y=screen.y;
        inputs.process_event(event);elysia::scene::SceneTestAccess::input(scene,inputs.snapshot());
        check(scene._player->thrust.y<0,"Web drag below the role must thrust upward");
        inputs.begin_frame();platform::cancel_web_pointer();while(SDL_PollEvent(&event))inputs.process_event(event);
        elysia::scene::SceneTestAccess::input(scene,inputs.snapshot());
        check(scene._player->thrust.length_squared()==0,"Browser cancellation must release thrust");
        inputs.begin_frame();elysia::scene::SceneTestAccess::input(scene,inputs.snapshot());
        check(scene._player->thrust.length_squared()==0,"Cancelled thrust must not reappear");
        platform::request_web_pause();scene.on_before_update(0);
        check(scene._pause_menu_open&&scene.is_paused(),"Browser pause request must open the existing pause menu");
        scene.on_before_update(0);check(scene._pause_menu_open,"Browser resume must not unpause automatically");
        scene.close_pause_menu();inputs.shutdown();
    }
};
}
namespace game::testing {
inline void check_web_input(SDL_Renderer* renderer,const elysia::io::ContentRegistry& registry){
    const auto directory=std::filesystem::path("out/qa/web-input")/std::to_string(SDL_GetTicksNS());
    std::filesystem::create_directories(directory);
    if(!ELYSIA_SAVE->initialize(directory))throw std::runtime_error("Web input save fixture failed");
    if(!ELYSIA_AUDIO->initialize({}))throw std::runtime_error("Web input audio fixture failed");
    const auto previous=game::progress::Store::instance().data;
    elysia::scene::SceneRuntimeContext context(renderer,registry,1280,720);
    game::scene::FlightScene flight;elysia::scene::SceneTestAccess::bind(flight,context);
    elysia::scene::SceneTestAccess::enter(flight,game::scene::FlightPayload{1});
    game::scene::WebInputTestAccess::run_web_input(flight);
    elysia::scene::SceneTestAccess::exit(flight);
    game::progress::Store::instance().data=previous;ELYSIA_AUDIO->shutdown();ELYSIA_SAVE->shutdown();
    std::cout<<"PASS: landscape Web pointer projection, input cancellation and one-way pause\n";
}
}
