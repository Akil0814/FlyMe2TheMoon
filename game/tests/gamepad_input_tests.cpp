#include "game/gameplay/flight_input.h"
#include "game/scene/selection_scene.h"
#include "game/scene/flight_scene.h"
#include "game/scene/main_menu_scene.h"
#include "engine/input/input_system.h"
#include "engine/io/path/path_manager.h"
#include "engine/io/loaders/content_registry_loader.h"
#include "engine/loading/game_content_loader.h"
#include "engine/save/save_service.h"
#include "engine/ui/window/ui_window.h"
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace elysia::scene {
class SceneTestAccess {
public:
    static void enter(Scene& scene,const ScenePayload& payload={}){scene.lifecycle_enter(payload);}
    static void bind(Scene& scene,const SceneRuntimeContext& context){scene.bind_runtime_context(context);}
    static void update(Scene& scene,double delta){scene.lifecycle_update(delta);}
    static void exit(Scene& scene){scene.lifecycle_exit();}
    static void input(Scene& scene,const input::InputSnapshot& snapshot){scene.lifecycle_input(snapshot);}
    static void layout(Scene& scene){for(auto& root:scene._ui_roots)if(auto* window=dynamic_cast<ui::UiWindow*>(root.get()))window->update(0);}
    static bool modal_visible(Scene& scene){
        for(auto& root:scene._ui_roots)if(dynamic_cast<ui::UiWindow*>(root.get())&&root->is_visible())return true;
        return false;
    }
    static std::string dialogue(Scene& scene){
        for(auto& root:scene._ui_roots)if(auto* text=dynamic_cast<ui::UiTextBlock*>(root.get());text&&text->is_visible())return text->text_content().value;
        return {};
    }
    static core::Vector2 thrust(Scene& scene){
        for(auto& layer:scene._object_layers)for(auto& object:layer)if(auto* player=dynamic_cast<game::play::Player*>(object.get()))return player->thrust;
        throw std::runtime_error("Flight test player missing");
    }
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
    state.set_axis(A::GamepadLeftX,.19f);state.set_axis(A::GamepadLeftY,-.19f);state.set_axis(A::GamepadRightTrigger,.19f);
    require(game::play::gamepad_thrust(state).length_squared()==0,"Stick and trigger noise must remain inside their dead zones");
    state.clear();state.set_axis(A::GamepadLeftY,-.6f);
    require(near(game::play::gamepad_thrust(state).y,-.5f),"Stick must smoothly rescale power after its dead zone");
    state.set_axis(A::GamepadLeftY,1);
    require(game::play::gamepad_thrust(state).length_squared()==0,"Down alone must not fire the jet");
    state.clear();state.set_axis(A::GamepadRightTrigger,.6f);
    require(near(game::play::gamepad_thrust(state).y,-.5f),"RT must provide gradual vertical thrust");
    state.clear();state.set_pressed(C::GamepadSouth,true);
    require(game::play::gamepad_thrust(state).length_squared()==0,"A must not fire the jet");
    state.clear();
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
    require(flight.thrust.length_squared()==0,"SDL A button must not fire the jet");
    system.begin_frame();axis(system,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767);send();
    require(near(flight.thrust.y,-1),"RT must still fire while A is held");
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
void test_flight_scene(SDL_Renderer* renderer,const io::ContentRegistry& registry){
    scene::SceneRuntimeContext context(renderer,registry,1280,720);
    game::scene::FlightScene flight;Requests requests;flight.attach(&requests);
    scene::SceneTestAccess::bind(flight,context);
    scene::SceneTestAccess::enter(flight,game::scene::FlightPayload{39});
    flight.set_ui_gamepad(input::InputSourceId::gamepad(pad_id));
    input::InputSystem system;system.initialize();
    const auto send=[&]{scene::SceneTestAccess::input(flight,system.snapshot());scene::SceneTestAccess::layout(flight);};
    const auto change=[&](SDL_GamepadButton control,bool down){system.begin_frame();button(system,control,down);send();};
    const auto press=[&](SDL_GamepadButton control){change(control,true);change(control,false);};
    send();
    for(int i=0;i<600&&scene::SceneTestAccess::dialogue(flight).empty();++i)scene::SceneTestAccess::update(flight,1./60.);
    const auto first_line=scene::SceneTestAccess::dialogue(flight);
    require(!first_line.empty(),"Original level 39 story must reach its first dialogue");
    press(SDL_GAMEPAD_BUTTON_SOUTH);
    const auto second_line=scene::SceneTestAccess::dialogue(flight);
    require(!second_line.empty()&&second_line!=first_line,"A must still advance the actual scene's dialogue");
    press(SDL_GAMEPAD_BUTTON_SOUTH);press(SDL_GAMEPAD_BUTTON_SOUTH);
    require(scene::SceneTestAccess::dialogue(flight).empty()&&!flight.is_paused(),"A must finish the original story and restore flight");
    change(SDL_GAMEPAD_BUTTON_SOUTH,true);
    require(scene::SceneTestAccess::thrust(flight).length_squared()==0,"A must not thrust after leaving dialogue");
    change(SDL_GAMEPAD_BUTTON_SOUTH,false);
    system.begin_frame();axis(system,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,19660);send();
    require(near(scene::SceneTestAccess::thrust(flight).y,-.5f),"Actual flight must retain gradual RT thrust");
    system.begin_frame();axis(system,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,0);send();
    press(SDL_GAMEPAD_BUTTON_WEST);
    require(requests.received.empty(),"X must not restart during flight");
    change(SDL_GAMEPAD_BUTTON_EAST,true);
    require(flight.is_paused()&&scene::SceneTestAccess::modal_visible(flight),"B must open the pause menu");
    system.begin_frame();send();
    require(scene::SceneTestAccess::modal_visible(flight),"Holding B must not close the newly opened pause menu");
    change(SDL_GAMEPAD_BUTTON_EAST,false);
    require(scene::SceneTestAccess::modal_visible(flight),"Releasing B must not close the newly opened pause menu");
    press(SDL_GAMEPAD_BUTTON_WEST);
    require(requests.received.empty()&&scene::SceneTestAccess::modal_visible(flight),"X must not restart from the pause menu");
    change(SDL_GAMEPAD_BUTTON_EAST,true);
    require(!flight.is_paused()&&!scene::SceneTestAccess::modal_visible(flight),"B must close the pause menu without reopening it");
    system.begin_frame();send();change(SDL_GAMEPAD_BUTTON_EAST,false);
    require(!flight.is_paused()&&!scene::SceneTestAccess::modal_visible(flight),"Holding or releasing B after resume must not reopen pause");
    press(SDL_GAMEPAD_BUTTON_START);
    require(flight.is_paused()&&scene::SceneTestAccess::modal_visible(flight),"Start must still open pause");
    press(SDL_GAMEPAD_BUTTON_START);
    require(!flight.is_paused()&&!scene::SceneTestAccess::modal_visible(flight),"Start must still resume");
    press(SDL_GAMEPAD_BUTTON_EAST);press(SDL_GAMEPAD_BUTTON_DPAD_DOWN);press(SDL_GAMEPAD_BUTTON_SOUTH);
    require(requests.received.size()==1&&requests.received.back().route.target==game::scene_keys::Flight,"A must activate Retry selected in the pause menu exactly once");
    const auto* payload=scene::try_scene_payload<game::scene::FlightPayload>(requests.received.back().route.payload);
    require(payload&&payload->level==39&&payload->restart,"Menu Retry must retain the current level and restart flag");
    system.shutdown();scene::SceneTestAccess::exit(flight);
}
}
int main(){
    SDL_Surface* surface=nullptr;SDL_Renderer* renderer=nullptr;int result=1;
    try{
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
        require(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD),SDL_GetError());
        require(TTF_Init(),SDL_GetError());require(MIX_Init(),SDL_GetError());
        require(io::PathManager::instance()->initialize(std::filesystem::current_path()).has_value(),"Path initialization failed");
        const auto directory=std::filesystem::path("out/qa/gamepad")/std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(directory);require(ELYSIA_SAVE->initialize(directory).has_value(),"Isolated save initialization failed");
        test_thrust();test_routing();test_menu();
        surface=SDL_CreateSurface(1280,720,SDL_PIXELFORMAT_RGBA32);require(surface!=nullptr,SDL_GetError());
        renderer=SDL_CreateSoftwareRenderer(surface);require(renderer!=nullptr,SDL_GetError());
        const auto registry=io::ContentRegistryLoader{}.load(io::PathManager::instance()->content_registry());
        require(registry.has_value(),"Content registry load failed");
        loading::GameContentLoader loader;require(loader.start(renderer,*registry,{}).has_value(),"Game content load failed to start");
        const auto deadline=SDL_GetTicks()+30000;
        while(loader.is_running()&&SDL_GetTicks()<deadline){loader.update();SDL_Delay(1);}
        require(loader.is_finished(),loader.failure()?loader.failure()->diagnostic.message.c_str():"Game content load timed out");
        test_flight_scene(renderer,*registry);
        std::cout<<"PASS: analog flight, input suppression, menu A/B, scene B/Start pause, no A thrust or X restart, story A and menu Retry\n";result=0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';}
    ELYSIA_SAVE->shutdown();
    if(renderer)SDL_DestroyRenderer(renderer);if(surface)SDL_DestroySurface(surface);
    MIX_Quit();TTF_Quit();SDL_Quit();return result;
}
