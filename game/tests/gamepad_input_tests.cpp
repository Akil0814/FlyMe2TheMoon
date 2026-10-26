#include "game/gameplay/flight_input.h"
#include "game/scene/selection_scene.h"
#include "game/scene/flight_scene.h"
#include "game/scene/main_menu_scene.h"
#include "game/progress/progress.h"
#include "engine/input/input_system.h"
#include "engine/io/path/path_manager.h"
#include "engine/io/loaders/content_registry_loader.h"
#include "engine/loading/game_content_loader.h"
#include "engine/resources/resource_service.h"
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
    static game::play::Player& player(Scene& scene){
        for(auto& layer:scene._object_layers)for(auto& object:layer)if(auto* player=dynamic_cast<game::play::Player*>(object.get()))return *player;
        throw std::runtime_error("Flight test player missing");
    }
    static game::frontend::StoryText& story_text(Scene& scene){
        for(auto& root:scene._ui_roots)if(auto* text=dynamic_cast<game::frontend::StoryText*>(root.get()))return *text;
        throw std::runtime_error("Story text missing");
    }
    static game::frontend::TutorialUi& tutorial(Scene& scene){
        for(auto& root:scene._ui_roots)if(auto* tutorial=dynamic_cast<game::frontend::TutorialUi*>(root.get()))return *tutorial;
        throw std::runtime_error("Tutorial composition missing");
    }
    static game::play::Entity& pickup(Scene& scene,int kind){
        for(auto& layer:scene._object_layers)for(auto& object:layer)if(auto* e=dynamic_cast<game::play::Entity*>(object.get());e&&e->kind==kind&&!e->collected)return *e;
        throw std::runtime_error("Pickup fixture missing");
    }
    static void move_player(Scene& scene,core::Vector2 position){
        if(!scene.physics_world().teleport_object(player(scene).physics_handle(),position,physics::TeleportVelocityMode::Clear))throw std::runtime_error("Pickup test teleport failed");
    }
    static game::frontend::StarRating& hud_stars(Scene& scene){
        for(auto& root:scene._ui_roots)if(auto* hud=dynamic_cast<game::frontend::StarRating*>(root.get()))return *hud;
        throw std::runtime_error("HUD star fixture missing");
    }
    static game::frontend::StarPickupEffects& star_pickups(Scene& scene){
        for(auto& root:scene._ui_roots)if(auto* effect=dynamic_cast<game::frontend::StarPickupEffects*>(root.get()))return *effect;
        throw std::runtime_error("Star pickup fixture missing");
    }
    static bool jet_visible(Scene& scene){
        for(auto& layer:scene._object_layers)for(auto& object:layer)if(auto* player=dynamic_cast<game::play::Player*>(object.get())){
            std::vector<core::RenderCommand> drawn;player->submit_render_commands(drawn);return drawn.size()>1;
        }
        throw std::runtime_error("Flight test player missing");
    }
    static void move_to_result(Scene& scene,bool success){
        game::play::Player* player=nullptr;game::play::Entity* moon=nullptr;
        for(auto& layer:scene._object_layers)for(auto& object:layer){
            if(auto* p=dynamic_cast<game::play::Player*>(object.get()))player=p;
            if(auto* e=dynamic_cast<game::play::Entity*>(object.get());e&&e->kind==101)moon=e;
        }
        if(!player||!moon)throw std::runtime_error("Result test player/moon missing");
        const auto position=success?moon->center()-(player->center()-player->position()):core::Vector2{-200,-200};
        if(!scene.physics_world().teleport_object(player->physics_handle(),position,physics::TeleportVelocityMode::Clear))throw std::runtime_error("Result test teleport failed");
    }
    static ui::UiLabel* result_notice(Scene& scene){
        for(auto& root:scene._ui_roots)if(auto* window=dynamic_cast<ui::UiWindow*>(root.get());window&&window->is_visible()){
            auto* list=dynamic_cast<ui::UiChildHost*>(window->child_at(1));
            if(list)return dynamic_cast<ui::UiLabel*>(list->child_at(4));
        }
        return nullptr;
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
    require(scene::SceneTestAccess::dialogue(flight)==first_line,"First A press must complete the typewriter text without advancing Lua");
    press(SDL_GAMEPAD_BUTTON_SOUTH);
    const auto second_line=scene::SceneTestAccess::dialogue(flight);
    require(!second_line.empty()&&second_line!=first_line,"A must still advance the actual scene's dialogue");
    press(SDL_GAMEPAD_BUTTON_SOUTH);press(SDL_GAMEPAD_BUTTON_SOUTH);press(SDL_GAMEPAD_BUTTON_SOUTH);press(SDL_GAMEPAD_BUTTON_SOUTH);
    require(scene::SceneTestAccess::dialogue(flight).empty()&&!flight.is_paused(),"A must finish the original story and restore flight");
    change(SDL_GAMEPAD_BUTTON_SOUTH,true);
    require(scene::SceneTestAccess::thrust(flight).length_squared()==0,"A must not thrust after leaving dialogue");
    change(SDL_GAMEPAD_BUTTON_SOUTH,false);
    system.begin_frame();axis(system,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,19660);send();
    require(near(scene::SceneTestAccess::thrust(flight).y,-.5f),"Actual flight must retain gradual RT thrust");
    scene::SceneTestAccess::update(flight,1./60.);
    require(scene::SceneTestAccess::jet_visible(flight),"Actual RT thrust must show the energy jet");
    system.begin_frame();axis(system,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,0);send();
    require(!scene::SceneTestAccess::jet_visible(flight),"Releasing RT must hide the jet immediately");
    press(SDL_GAMEPAD_BUTTON_WEST);
    require(requests.received.empty(),"X must not restart during flight");
    system.begin_frame();axis(system,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767);send();
    change(SDL_GAMEPAD_BUTTON_EAST,true);
    require(flight.is_paused()&&scene::SceneTestAccess::modal_visible(flight),"B must open the pause menu");
    require(!scene::SceneTestAccess::jet_visible(flight),"Pausing while firing must hide the jet");
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
    require(!scene::SceneTestAccess::jet_visible(flight),"Held RT must not leave a residual jet after resume");
    system.begin_frame();axis(system,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,0);send();
    system.begin_frame();SDL_Event pause_click{};pause_click.type=SDL_EVENT_MOUSE_BUTTON_DOWN;pause_click.button.button=SDL_BUTTON_LEFT;pause_click.button.x=1228;pause_click.button.y=42;pause_click.button.down=true;system.process_event(pause_click);send();
    require(!flight.is_paused(),"Pause icon must wait for the mouse release before activating");
    system.begin_frame();pause_click.type=SDL_EVENT_MOUSE_BUTTON_UP;pause_click.button.down=false;system.process_event(pause_click);send();
    require(flight.is_paused()&&scene::SceneTestAccess::modal_visible(flight),"The textured pause icon must open the real pause menu");
    press(SDL_GAMEPAD_BUTTON_EAST);
    require(!flight.is_paused(),"B must resume after opening pause with the textured mouse button");
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
void test_pickup_scene(SDL_Renderer* renderer,const io::ContentRegistry& registry){
    using Access=scene::SceneTestAccess;
    scene::SceneRuntimeContext context(renderer,registry,1280,720);
    game::scene::FlightScene flight;Access::bind(flight,context);Access::enter(flight,game::scene::FlightPayload{39,true});
    for(int i=0;i<600&&flight.is_paused();++i)Access::update(flight,1./60.);
    require(!flight.is_paused(),"Pickup scene must leave its opening");
    auto& star=Access::pickup(flight,3);
    Access::move_player(flight,star.center()+core::Vector2{0,13});Access::update(flight,0);
    auto& effects=Access::star_pickups(flight);auto& hud=Access::hud_stars(flight);
    require(star.collected&&!star.physics_state()->enabled&&effects.pending_count()==1&&hud.collected()==0,"Star pickup must disable collision immediately while delaying the HUD fill until arrival");
    auto& crystal=Access::pickup(flight,5);
    Access::move_player(flight,crystal.center()+core::Vector2{0,13});Access::update(flight,0);
    std::vector<core::RenderCommand> before;crystal.submit_render_commands(before);
    require(crystal.collected&&!crystal.physics_state()->enabled&&before.size()==1,"Crystal pickup must disable collision but retain its disappearing sprite");
    Access::update(flight,0);require(effects.pending_count()==1,"Repeated overlap must not launch the same star twice");
    input::InputSystem system;system.initialize();flight.set_ui_gamepad(input::InputSourceId::gamepad(pad_id));
    const auto press=[&]{for(bool down:{true,false}){system.begin_frame();button(system,SDL_GAMEPAD_BUTTON_EAST,down);Access::input(flight,system.snapshot());Access::layout(flight);}};
    press();require(flight.is_paused(),"B must pause during pickup effects");Access::update(flight,.5);
    std::vector<core::RenderCommand> paused;crystal.submit_render_commands(paused);
    require(effects.pending_count()==1&&hud.collected()==0&&paused.size()==1&&paused[0].command_rect==before[0].command_rect,"Pause menu must freeze both crystal and HUD star animations");
    press();require(!flight.is_paused(),"B must resume pickup effects");
    Access::move_player(flight,{20,20});
    for(int i=0;i<42;++i)Access::update(flight,1./60.);
    require(effects.pending_count()==0&&hud.collected()==1&&!crystal.is_visible(),"Resuming must finish the crystal animation and fill exactly one HUD slot");
    Access::exit(flight);Access::enter(flight,game::scene::FlightPayload{39,true});
    require(Access::star_pickups(flight).pending_count()==0&&Access::hud_stars(flight).collected()==0&&!Access::pickup(flight,5).collected,"Retry must start with fresh pickups and no leftover HUD flights");
    system.shutdown();Access::exit(flight);
}
void test_original_tutorial_sequence(SDL_Renderer* renderer,const io::ContentRegistry& registry){
    scene::SceneRuntimeContext context(renderer,registry,1280,720);
    const auto texts=game::content::read("text/"+game::content::language()+"/strings.json");
    for(int level:{1,2}){
        game::scene::FlightScene flight;scene::SceneTestAccess::bind(flight,context);
        scene::SceneTestAccess::enter(flight,game::scene::FlightPayload{level});
        input::InputSystem system;system.initialize();
        const auto tick=[&]{system.begin_frame();scene::SceneTestAccess::input(flight,system.snapshot());scene::SceneTestAccess::update(flight,1./60.);};
        const auto press=[&]{
            for(bool down:{true,false}){system.begin_frame();SDL_Event e{};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
                e.key.scancode=SDL_SCANCODE_RETURN;e.key.key=SDLK_RETURN;e.key.down=down;system.process_event(e);scene::SceneTestAccess::input(flight,system.snapshot());}
        };
        auto& player=scene::SceneTestAccess::player(flight);
        auto& text=scene::SceneTestAccess::story_text(flight);
        auto& diagram=scene::SceneTestAccess::tutorial(flight);
        for(int i=0;i<600&&flight.is_paused()&&scene::SceneTestAccess::dialogue(flight).empty();++i)tick();
        if(level==1){
            require(flight.is_paused()&&!player.body_visible()&&player.effect_time()==0,"First incantation must precede the role's entrance animation");
            for(int i=0;i<10;++i)tick();
            require(player.effect_time()==0&&!player.body_visible(),"Incantation pause must freeze the role's ball effect");
            const auto first=scene::SceneTestAccess::dialogue(flight);press();
            require(text.complete()&&scene::SceneTestAccess::dialogue(flight)==first,"Continue must first reveal the incantation");
            // A mouse press alone cannot close dialogue; the release completes it.
            for(bool down:{true,false}){
                system.begin_frame();SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.x=600;motion.motion.y=600;system.process_event(motion);
                SDL_Event e{};e.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;e.button.button=SDL_BUTTON_LEFT;e.button.down=down;e.button.x=600;e.button.y=600;system.process_event(e);
                scene::SceneTestAccess::input(flight,system.snapshot());
                require(flight.is_paused()==down,"Dialogue should resume role logic on mouse release, not press");
            }
            for(int i=0;i<20;++i)tick();
            require(player.effect_time()>0&&!player.entry_effect_finished(),"Role must unfold only after continuing the incantation");
            for(int i=0;i<45;++i)tick();
            require(scene::SceneTestAccess::dialogue(flight)=="~~~~~~"&&player.entry_effect_finished(),"Original one-second entrance delay must lead to the next line");
            if(!text.complete())press();press();
            require(player.look_time==3.5,"Look-around must start a full 3.5-second animation");
            for(int i=0;i<181;++i)tick();
            require(player.look_time>0&&player.look_time<.6,"Look-around tail must overlap the next line after the Lua's three-second wait");
        }else{
            // The original second-level delay overlaps the entrance animation.
            int frames=0;while(scene::SceneTestAccess::dialogue(flight).empty()&&frames++<70)tick();
            require(frames<=62&&!scene::SceneTestAccess::dialogue(flight).empty()&&player.entry_effect_finished(),"Second-level first line must follow one second, without a separate spawn wait");
        }
        for(int i=0;i<1800&&diagram.type()==0;++i){if(i%12==0&&!scene::SceneTestAccess::dialogue(flight).empty())press();tick();}
        require(diagram.type()==level,"Each original tutorial must select its own animated diagram");
        if(!text.complete())press();press();
        const int minimum=level==1?240:390;
        for(int i=0;i<minimum-20;++i)tick();press();
        require(diagram.type()==level,"First-time tutorial must retain its original minimum display duration");
        for(int i=0;i<24;++i)tick();press();
        require(diagram.type()==0,"Tutorial diagram must hide after the minimum time and continue");
        if(level==1){
            for(int i=0;i<1200&&scene::SceneTestAccess::dialogue(flight)!=texts.at("Level1-10").get<std::string>();++i)tick();
            require(scene::SceneTestAccess::dialogue(flight)==texts.at("Level1-10").get<std::string>(),"Original automatic demo must reach the moon and return to its handoff dialogue");
        }else{
            require(game::progress::Store::instance().data.story_flags.contains("lvl2_b1"),"Second-level direction tutorial must save its viewed flag");
        }
        system.shutdown();scene::SceneTestAccess::exit(flight);
    }
    // A normal re-entry of the now-seen second tutorial exposes its mouse Skip
    // button; that operation bypasses the 6.5-second teaching minimum.
    game::scene::FlightScene seen;scene::SceneTestAccess::bind(seen,context);
    scene::SceneTestAccess::enter(seen,game::scene::FlightPayload{2});
    input::InputSystem system;system.initialize();
    auto& diagram=scene::SceneTestAccess::tutorial(seen);
    for(int frame=0;frame<1800&&diagram.type()!=2;++frame){
        system.begin_frame();
        if(frame%12<2){SDL_Event event{};event.type=frame%12==0?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;event.key.scancode=SDL_SCANCODE_RETURN;event.key.key=SDLK_RETURN;event.key.down=frame%12==0;system.process_event(event);}
        scene::SceneTestAccess::input(seen,system.snapshot());scene::SceneTestAccess::update(seen,1./60.);
    }
    require(diagram.type()==2,"Seen-story mouse skip fixture must reach the direction tutorial");
    for(bool down:{true,false}){
        system.begin_frame();SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.x=1160;motion.motion.y=686;system.process_event(motion);
        SDL_Event event{};event.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;event.button.button=SDL_BUTTON_LEFT;event.button.down=down;event.button.x=1160;event.button.y=686;system.process_event(event);
        scene::SceneTestAccess::input(seen,system.snapshot());
    }
    for(int i=0;i<120&&diagram.type()!=0;++i)scene::SceneTestAccess::update(seen,1./60.);
    require(diagram.type()==0&&!seen.is_paused(),"Mouse Skip must bypass the seen tutorial's minimum and clean up its overlay");
    system.shutdown();scene::SceneTestAccess::exit(seen);
}
void test_tutorial_flight_animation(SDL_Renderer* renderer,const io::ContentRegistry& registry){
    scene::SceneRuntimeContext context(renderer,registry,1280,720);
    game::scene::FlightScene flight;scene::SceneTestAccess::bind(flight,context);
    scene::SceneTestAccess::enter(flight,game::scene::FlightPayload{1});
    input::InputSystem system;system.initialize();
    const auto texts=game::content::read("text/"+game::content::language()+"/strings.json");
    const auto demo_line=texts.at("Level1-8").get<std::string>();
    bool demo_started=false;
    // Follow the original Lua through its delays and minimum-duration tutorial.
    for(int frame=0;frame<2400&&!demo_started;++frame){
        system.begin_frame();
        if(frame%12==0||frame%12==1){
            SDL_Event event{};event.type=frame%12==0?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
            event.key.scancode=SDL_SCANCODE_RETURN;event.key.key=SDLK_RETURN;event.key.down=frame%12==0;
            system.process_event(event);
        }
        scene::SceneTestAccess::input(flight,system.snapshot());
        scene::SceneTestAccess::update(flight,1./60.);
        demo_started=scene::SceneTestAccess::dialogue(flight)==demo_line&&scene::SceneTestAccess::thrust(flight).y<-.1f;
    }
    require(demo_started,"First-level original Lua must start its automatic flight demo");
    auto& player=scene::SceneTestAccess::player(flight);
    for(int frame=0;frame<120;++frame){
        const auto recorded=player.thrust;
        require(recorded.y<-.1f,"Tutorial regression sample must remain in the recorded powered flight");
        system.begin_frame();
        // Real player controls must neither erase nor replace the recorded input.
        axis(system,SDL_GAMEPAD_AXIS_LEFTX,32767);
        scene::SceneTestAccess::input(flight,system.snapshot());
        require(player.thrust==recorded,"Live input routing erased the tutorial thrust before character animation");
        scene::SceneTestAccess::update(flight,1./240.);
        const auto action=recorded.x<-.2f?"charLeft":recorded.x>.2f?"charRight":"charUp";
        const auto* atlas=ELYSIA_RESOURCES->find_atlas(std::string("fly.CharActions.")+action);
        require(atlas!=nullptr,"Tutorial flight animation atlas missing");
        std::vector<core::RenderCommand> commands;player.submit_render_commands(commands);
        bool flying=false;
        for(const auto& command:commands)for(std::size_t index=0;index<atlas->size();++index)
            flying|=command.texture==atlas->frame_at(index)->_texture;
        require(flying,"Automatic flight must render a flying pose even between fixed physics steps");
    }
    require(scene::SceneTestAccess::jet_visible(flight),"Automatic flight must visibly emit its thrust particles");
    flight.pause();system.begin_frame();scene::SceneTestAccess::input(flight,system.snapshot());
    require(player.thrust.length_squared()==0&&!scene::SceneTestAccess::jet_visible(flight),"Pausing a tutorial must still stop its jet immediately");
    system.shutdown();scene::SceneTestAccess::exit(flight);
}
void test_result_records(SDL_Renderer* renderer,const io::ContentRegistry& registry){
    auto& data=game::progress::Store::instance().data;
    const auto run=[&](bool success,const std::string& expected){
        scene::SceneRuntimeContext context(renderer,registry,1280,720);
        game::scene::FlightScene flight;scene::SceneTestAccess::bind(flight,context);
        scene::SceneTestAccess::enter(flight,game::scene::FlightPayload{39,true});
        for(int i=0;i<600&&flight.is_paused();++i)scene::SceneTestAccess::update(flight,1./60.);
        require(!flight.is_paused(),"Record test must leave the opening and skipped story");
        scene::SceneTestAccess::move_to_result(flight,success);
        for(int i=0;i<600&&!scene::SceneTestAccess::modal_visible(flight);++i)scene::SceneTestAccess::update(flight,1./60.);
        auto* notice=scene::SceneTestAccess::result_notice(flight);
        require(notice&&notice->text_content().value==expected,"Result notice must reflect the saved score record, ties, failure and save errors");
        scene::SceneTestAccess::layout(flight);
        require(notice->screen_rect().height()>0&&notice->screen_rect().y()>300,"Record notice must occupy its own line below the score");
        scene::SceneTestAccess::exit(flight);
    };
    data.score_verified[38]=false;data.scores[38]=0;
    run(true,game::content::ui("result_new_record"));
    const int best=data.scores[38];require(best>0&&data.score_verified[38],"First clear must persist its verified score");
    run(true,"");require(data.scores[38]==best,"Equal score must retain the previous best without a record notice");
    data.scores[38]=best+100;run(true,"");
    data.scores[38]=best-1;run(true,game::content::ui("result_new_record"));
    run(false,"");
    data.scores[38]=0;
    require(ELYSIA_SAVE->close("fly_progress").has_value(),"Save-error test must close the isolated slot");
    run(true,game::content::ui("save_failure"));
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
        loading::GameContentLoader loader;const auto started=loader.start(renderer,*registry,{});
        require(started.has_value(),started?"":started.error().diagnostic.message.c_str());
        const auto deadline=SDL_GetTicks()+30000;
        while(loader.is_running()&&SDL_GetTicks()<deadline){loader.update();SDL_Delay(1);}
        require(loader.is_finished(),loader.failure()?loader.failure()->diagnostic.message.c_str():"Game content load timed out");
        test_flight_scene(renderer,*registry);
        test_original_tutorial_sequence(renderer,*registry);
        test_pickup_scene(renderer,*registry);
        test_tutorial_flight_animation(renderer,*registry);
        test_result_records(renderer,*registry);
        std::cout<<"PASS: analog flight, input suppression, menu A/B, scene B/Start pause, no A thrust or X restart, story A and menu Retry\n";result=0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';}
    ELYSIA_SAVE->shutdown();
    if(renderer)SDL_DestroyRenderer(renderer);if(surface)SDL_DestroySurface(surface);
    MIX_Quit();TTF_Quit();SDL_Quit();return result;
}
