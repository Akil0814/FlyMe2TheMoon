#include "game/scene/selection_scene.h"
#include "game/scene/flight_scene.h"
#include "game/scene/main_menu_scene.h"
#include "game/progress/progress.h"
#include "engine/io/path/path_manager.h"
#include "engine/save/save_service.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/containers/ui_grid_container.h"
#include "engine/ui/window/ui_window.h"
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace elysia::scene {
class SceneTestAccess {
public:
    static void enter(Scene& scene, const ScenePayload& payload) { scene.lifecycle_enter(payload); }
    static ui::UiWindow& window(Scene& scene) {
        return dynamic_cast<ui::UiWindow&>(*scene._ui_roots.at(0));
    }
};
}

namespace {
using namespace elysia;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct Requests final : scene::SceneRequestObserver {
    std::vector<scene::SceneRequest> received;
    void on_scene_request(const scene::SceneRequest& request) override { received.push_back(request); }
};
void send(ui::UiWindow& window, ui::UiInputEvent event) {
    (void)window.on_ui_input_event(event);
    // A real update between press and release must preserve the pressed button.
    window.on_ui_input_frame({});
    window.update(1. / 60.);
    std::vector<core::UiRenderCommand> commands;
    window.submit_ui_render_commands(commands);
}
void click(ui::UiWindow& window, const ui::UiElement& button) {
    const auto center = button.screen_rect().center();
    ui::UiInputEvent event{.type=ui::UiInputEventType::MouseMoved,
        .device=input::InputDevice::Mouse, .control=input::RawInputControl::MouseLeft,
        .mouse_x=static_cast<int>(center.x), .mouse_y=static_cast<int>(center.y)};
    send(window,event);
    event.type=ui::UiInputEventType::PointerPressed; send(window,event);
    event.type=ui::UiInputEventType::PointerReleased; send(window,event);
}
void key(ui::UiWindow& window, ui::UiAction action) {
    send(window,{.action=action,.type=ui::UiInputEventType::ActionPressed,.device=input::InputDevice::Keyboard});
    send(window,{.action=action,.type=ui::UiInputEventType::ActionReleased,.device=input::InputDevice::Keyboard});
}
void check_flight(const Requests& requests) {
    require(requests.received.size()==1,"Level 1 must produce exactly one scene request");
    const auto& request=requests.received.back();
    const auto* payload=scene::try_scene_payload<game::scene::FlightPayload>(request.route.payload);
    require(request.type==scene::SceneRequestType::Switch && request.route.target==game::scene_keys::Flight
        && request.route.reload_mode==scene::SceneReloadMode::Recreate && payload && payload->level==1,
        "Level 1 must route to Flight with its level payload");
}
void test_mouse() {
    for (int chapter=0;chapter<4;++chapter) {
        game::scene::SelectionScene scene;
        Requests requests; scene.attach(&requests);
        scene::SceneTestAccess::enter(scene,game::scene::SelectionPayload{chapter});
        auto& window=scene::SceneTestAccess::window(scene); window.update(0);
        auto& list=dynamic_cast<ui::UiListContainer&>(*window.child_at(0));
        auto& chapters=dynamic_cast<ui::UiGridContainer&>(*list.child_at(1));
        auto& levels=dynamic_cast<ui::UiGridContainer&>(*list.child_at(2));
        if (chapter==0) {
            click(window,*levels.child_at(0)); check_flight(requests); requests.received.clear();
        }
        click(window,*levels.child_at(1));
        require(requests.received.empty(),"Locked level must not produce a scene request");
        for (int target=0;target<4;++target) {
            click(window,*chapters.child_at(target));
            require(requests.received.size()==1,"Every chapter button must respond once, including locked chapters");
            const auto& request=requests.received.back();
            const auto* payload=scene::try_scene_payload<game::scene::SelectionPayload>(request.route.payload);
            require(request.route.target==game::scene_keys::Selection && payload && payload->chapter==target
                && request.route.reload_mode==scene::SceneReloadMode::Recreate,"Chapter route or payload is incorrect");
            requests.received.clear();
        }
        click(window,*list.child_at(4));
        require(requests.received.size()==1 && requests.received.back().route.target==game::scene_keys::MainMenu,
            "Back button must remain clickable");
    }
}
void test_keyboard() {
    game::scene::SelectionScene scene;
    Requests requests; scene.attach(&requests);
    scene::SceneTestAccess::enter(scene,{});
    auto& window=scene::SceneTestAccess::window(scene); window.update(0);
    key(window,ui::UiAction::Confirm); check_flight(requests); requests.received.clear();
    key(window,ui::UiAction::NavigateUp);
    key(window,ui::UiAction::NavigateRight);
    key(window,ui::UiAction::Confirm);
    require(requests.received.size()==1,"Keyboard must reach and activate chapter buttons");
    const auto* payload=scene::try_scene_payload<game::scene::SelectionPayload>(requests.received.back().route.payload);
    require(payload && payload->chapter==1,"Right navigation must select chapter 2");
    requests.received.clear();
    key(window,ui::UiAction::NavigateDown);
    key(window,ui::UiAction::NavigateDown);
    key(window,ui::UiAction::Confirm);
    require(requests.received.size()==1 && requests.received.back().route.target==game::scene_keys::MainMenu,
        "Keyboard must reach Back across the level grid");
}
}

int main() {
    try {
        require(elysia::io::PathManager::instance()->initialize(std::filesystem::current_path()).has_value(),"Path initialize failed");
        const auto directory=std::filesystem::path("out/qa/selection")/
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(directory);
        require(ELYSIA_SAVE->initialize(directory).has_value(),"Isolated save initialize failed");
        test_mouse(); test_keyboard();
        ELYSIA_SAVE->shutdown();
        std::cout<<"PASS: SelectionScene mouse, keyboard, chapter routes, locked levels and Back\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n'; return 1;
    }
}
