#include "game/gameplay/flight_input.h"
#include "game/scene/selection_scene.h"
#include "game/scene/flight_scene.h"
#include "game/scene/main_menu_scene.h"
#include "engine/input/input_system.h"
#include "engine/io/path/path_manager.h"
#include "engine/save/save_service.h"
#include "engine/ui/window/ui_window.h"
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace elysia::scene {
class SceneTestAccess {
public:
    static void enter(Scene& scene){scene.lifecycle_enter({});}
    static void input(Scene& scene,const input::InputSnapshot& snapshot){scene.lifecycle_input(snapshot);}
    static void layout(Scene& scene){for(auto& root:scene._ui_roots)if(auto* window=dynamic_cast<ui::UiWindow*>(root.get()))window->update(0);}
};
}

namespace {
using namespace elysia;
using C=input::RawInputControl;
using A=input::RawInputAxis;
constexpr SDL_JoystickID pad_id=12345;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(float a,float b){return std::abs(a-b)<.001f;}
void axis(input::InputSystem& system,SDL_GamepadAxis axis,Sint16 value){
    SDL_Event event{};event.type=SDL_EVENT_GAMEPAD_AXIS_MOTION;
    event.gaxis.which=pad_id;event.gaxis.axis=axis;event.gaxis.value=value;system.process_event(event);
}
void button(input::InputSystem& system,SDL_GamepadButton button,bool down){
    SDL_Event event{};event.type=down?SDL_EVENT_GAMEPAD_BUTTON_DOWN:SDL_EVENT_GAMEPAD_BUTTON_UP;
    event.gbutton.which=pad_id;event.gbutton.button=button;event.gbutton.down=down;system.process_event(event);
}
void test_thrust(){
    input::RawInputState state;
    require(game::play::gamepad_thrust(state).length_squared()==0,"Neutral pad must not thrust");
    state.set_axis(A::GamepadLeftX,.19f);state.set_axis(A::GamepadLeftY,-.19f);state.set_axis(A::GamepadRightTrigger,.09f);
    require(game::play::gamepad_thrust(state).length_squared()==0,"Stick and trigger noise must remain inside their dead zones");
    state.clear();state.set_axis(A::GamepadLeftY,-.6f);
    require(near(game::play::gamepad_thrust(state).y,-.5f),"Stick must smoothly rescale power after its dead zone");
    state.set_axis(A::GamepadLeftY,1);
    require(game::play::gamepad_thrust(state).length_squared()==0,"Down alone must not fire the jet");
    state.clear();state.set_axis(A::GamepadRightTrigger,.6f);
    require(near(game::play::gamepad_thrust(state).y,-.5f),"RT must provide gradual vertical thrust");
    state.clear();state.set_pressed(C::GamepadSouth,true);
    require(near(game::play::gamepad_thrust(state).y,-1),"A must give full vertical thrust");
    state.set_axis(A::GamepadLeftX,1);const auto right=game::play::gamepad_thrust(state);
    state.set_axis(A::GamepadLeftX,-1);const auto left=game::play::gamepad_thrust(state);
    require(near(right.length(),1)&&near(right.x,-left.x)&&near(right.y,left.y)&&near(right.x,std::sin(50*std::numbers::pi_v<float>/180)),"Stick steering must be symmetric and limited to the keyboard angle");
    state.clear();state.set_pressed(C::GamepadDPadLeft,true);
    require(near(game::play::gamepad_thrust(state).x,left.x),"D-pad must match full stick steering");
    state.set_pressed(C::GamepadDPadRight,true);
    require(near(game::play::gamepad_thrust(state).x,0),"Opposing D-pad directions must cancel steering");
    state.clear();state.set_pressed(C::GamepadDPadUp,true);
    require(near(game::play::gamepad_thrust(state).y,-1),"D-pad Up must give vertical thrust");
}

class FlightProbe final:public scene::Scene {
public:
    core::Vector2 thrust;
    FlightProbe(){set_ui_interaction_mode(scene::UiInteractionMode::Pointer);set_ui_gamepad(input::InputSourceId::gamepad(pad_id));}
    void on_enter(const scene::ScenePayload&)override{}
    void on_exit()override{}
    void on_reset()override{}
    void on_routed_input(const input::InputSnapshot& snapshot)override{
        thrust={};for(const auto& source:snapshot.sources)if(source.source.is_gamepad())thrust=game::play::gamepad_thrust(source.frame.state);
    }
};
void test_routing(){
    input::InputSystem system;system.initialize();
    FlightProbe flight;scene::SceneTestAccess::enter(flight);
    const auto send=[&]{scene::SceneTestAccess::input(flight,system.snapshot());};
    send();system.begin_frame();axis(system,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767);send();
    require(near(flight.thrust.y,-1),"SDL trigger input must reach routed flight controls");
    flight.pause();system.begin_frame();send();
    require(flight.thrust.length_squared()==0,"Paused flight must have no thrust");
    flight.resume();system.begin_frame();send();
    require(flight.thrust.length_squared()==0,"Held trigger must remain suppressed after resume");
    system.begin_frame();axis(system,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,0);send();
    system.begin_frame();axis(system,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767);send();
    require(near(flight.thrust.y,-1),"Trigger must work again after release");
    system.begin_frame();system.set_development_input_capture(input::InputCapture::Gamepad);send();
    require(flight.thrust.length_squared()==0,"Captured gamepad input must not drive flight");
    system.set_development_input_capture(input::InputCapture::None);
    system.begin_frame();axis(system,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,0);send();
    system.begin_frame();axis(system,SDL_GAMEPAD_AXIS_LEFTY,-32768);send();
    require(near(flight.thrust.y,-1),"Negative SDL stick endpoint must produce full upward thrust");
    system.begin_frame();SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_LOST;system.process_event(focus);send();
    require(flight.thrust.length_squared()==0,"Focus loss must stop thrust");
    system.begin_frame();send();require(flight.thrust.length_squared()==0,"Focus regain must require stick neutral");
    system.begin_frame();axis(system,SDL_GAMEPAD_AXIS_LEFTY,0);send();
    system.begin_frame();button(system,SDL_GAMEPAD_BUTTON_SOUTH,true);send();
    require(near(flight.thrust.y,-1),"SDL A button must reach flight controls");
    system.begin_frame();SDL_Event removed{};removed.type=SDL_EVENT_GAMEPAD_REMOVED;removed.gdevice.which=pad_id;system.process_event(removed);send();
    require(flight.thrust.length_squared()==0&&!system.snapshot().find(input::InputSourceId::gamepad(pad_id)),"Disconnect must remove the source and stop thrust");
    system.shutdown();
}
struct Requests final:scene::SceneRequestObserver {
    std::vector<scene::SceneRequest> received;
    void on_scene_request(const scene::SceneRequest& request)override{received.push_back(request);}
};
void test_menu(){
    game::scene::SelectionScene menu;Requests requests;menu.attach(&requests);
    scene::SceneTestAccess::enter(menu);scene::SceneTestAccess::layout(menu);
    input::InputSystem system;system.initialize();
    const auto send=[&]{scene::SceneTestAccess::input(menu,system.snapshot());scene::SceneTestAccess::layout(menu);};
    send();system.begin_frame();button(system,SDL_GAMEPAD_BUTTON_SOUTH,true);send();
    require(menu.ui_gamepad()==input::InputSourceId::gamepad(pad_id)&&requests.received.empty(),"First button must claim UI without activating a level");
    const auto press=[&](SDL_GamepadButton control){
        system.begin_frame();button(system,control,false);send();
        system.begin_frame();button(system,control,true);send();
        system.begin_frame();button(system,control,false);send();
    };
    press(SDL_GAMEPAD_BUTTON_SOUTH);
    require(requests.received.size()==1&&requests.received.back().route.target==game::scene_keys::Flight,"A must activate the focused level exactly once");requests.received.clear();
    press(SDL_GAMEPAD_BUTTON_DPAD_UP);press(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);press(SDL_GAMEPAD_BUTTON_SOUTH);
    require(requests.received.size()==1,"D-pad must reach chapter tabs");
    const auto* chapter=scene::try_scene_payload<game::scene::SelectionPayload>(requests.received.back().route.payload);
    require(chapter&&chapter->chapter==1,"D-pad Right must select chapter two");requests.received.clear();
    press(SDL_GAMEPAD_BUTTON_EAST);
    require(requests.received.size()==1&&requests.received.back().route.target==game::scene_keys::MainMenu,"B must return to the main menu");
    system.shutdown();
}
}
int main(){
    try{
        require(SDL_Init(SDL_INIT_GAMEPAD),SDL_GetError());
        require(io::PathManager::instance()->initialize(std::filesystem::current_path()).has_value(),"Path initialization failed");
        const auto directory=std::filesystem::path("out/qa/gamepad")/std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(directory);require(ELYSIA_SAVE->initialize(directory).has_value(),"Isolated save initialization failed");
        test_thrust();test_routing();test_menu();
        ELYSIA_SAVE->shutdown();SDL_Quit();
        std::cout<<"PASS: analog flight, SDL routing, pause/capture/focus/disconnect suppression and gamepad menu navigation\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
