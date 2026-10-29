#include "game/scene/selection_scene.h"
#include "game/scene/flight_scene.h"
#include "game/scene/main_menu_scene.h"
#include "game/scene/settings_scene.h"
#include "game/scene/credits_scene.h"
#include "game/frontend/moon_ui.h"
#include "game/progress/progress.h"
#include "engine/io/path/path_manager.h"
#include "engine/io/loaders/asset_config_types.h"
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
    static void enter(Scene& scene, const ScenePayload& payload) {
        // SceneManager supplies this in the game; isolated UI fixtures need it too.
        if(!scene._runtime_context){
            static const io::ContentRegistry registry;
            static const SceneRuntimeContext context(nullptr,registry,640,960);
            scene.bind_runtime_context(context);
        }
        scene.lifecycle_enter(payload);
    }
    static void bind(Scene& scene,const SceneRuntimeContext& context){scene.bind_runtime_context(context);}
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
        require(window.screen_rect()==core::Rect{0,0,640,960}&&chapters.column_count()==2&&levels.column_count()==3&&levels.child_count()==15,"Phone selection must display all 15 levels in a three-column viewport");
        require(chapters.screen_rect().nearly_equals({24,96,592,124})&&levels.screen_rect().nearly_equals({24,236,592,553})&&list.child_at(3)->screen_rect().nearly_equals({24,805,592,48})&&list.child_at(4)->screen_rect().nearly_equals({147.5f,869,345,67}),"Phone selection areas must match their no-scroll layout");
        if (chapter==0) {
            click(window,*levels.child_at(0)); check_flight(requests); requests.received.clear();
        }
        click(window,*levels.child_at(1));
        require(requests.received.empty(),"Locked level must not produce a scene request");
        for (int target=0;target<4;++target) {
            require(dynamic_cast<ui::UiButton&>(*chapters.child_at(target)).style().chrome.corner_radius>0,"Chapter tabs must retain rounded geometry when selected");
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
    // From the upper chapter row, pass through the lower chapter row and
    // the only unlocked level before reaching Back.
    key(window,ui::UiAction::NavigateDown);
    key(window,ui::UiAction::NavigateDown);
    key(window,ui::UiAction::NavigateDown);
    key(window,ui::UiAction::Confirm);
    require(requests.received.size()==1 && requests.received.back().route.target==game::scene_keys::MainMenu,
        "Keyboard must reach Back across the level grid");
}
void test_level_scores() {
    auto& store=game::progress::Store::instance();
    const auto original=store.data;
    for(int index=0;index<60;++index){
        store.data.stars[index]=index%3==0?-1:2;
        store.data.score_verified[index]=index%3==1;
        store.data.scores[index]=index==1?0:10000+index;
        store.data.legacy_scores[index]=999999;
    }
    for(int chapter=0;chapter<4;++chapter){
        game::scene::SelectionScene scene;
        scene::SceneTestAccess::enter(scene,game::scene::SelectionPayload{chapter});
        auto& window=scene::SceneTestAccess::window(scene);
        auto& list=dynamic_cast<ui::UiListContainer&>(*window.child_at(0));
        auto& levels=dynamic_cast<ui::UiGridContainer&>(*list.child_at(2));
        for(int index=0;index<15;++index){
            const int saved_index=chapter*15+index;
            const auto& node=dynamic_cast<const game::frontend::LevelNode&>(*levels.child_at(index));
            const auto expected=saved_index%3==1?std::optional<int>{store.data.scores[saved_index]}:std::nullopt;
            require(node.best_score()==expected,"Level cards must display their own verified best score, including zero; missing and legacy results need a placeholder");
        }
    }
    store.data=original;
}
void test_unlocked_routes(){
    auto& store=game::progress::Store::instance();const auto original=store.data;
    store.data.stars.fill(3);
    for(int chapter=0;chapter<4;++chapter){
        game::scene::SelectionScene scene;Requests requests;scene.attach(&requests);
        scene::SceneTestAccess::enter(scene,game::scene::SelectionPayload{chapter});
        auto& window=scene::SceneTestAccess::window(scene);window.update(0);
        auto& list=dynamic_cast<ui::UiListContainer&>(*window.child_at(0));
        auto& levels=dynamic_cast<ui::UiGridContainer&>(*list.child_at(2));
        for(int index=0;index<15;++index){
            click(window,*levels.child_at(index));
            require(requests.received.size()==1,"Every unlocked phone level must respond once to a click");
            const auto* payload=scene::try_scene_payload<game::scene::FlightPayload>(requests.received.back().route.payload);
            require(payload&&payload->level==chapter*15+index+1&&requests.received.back().route.target==game::scene_keys::Flight,"Phone level selection routed to the wrong level");requests.received.clear();
        }
    }
    game::scene::SelectionScene scene;Requests requests;scene.attach(&requests);
    scene::SceneTestAccess::enter(scene,{});auto& window=scene::SceneTestAccess::window(scene);window.update(0);
    for(int row=0;row<4;++row)key(window,ui::UiAction::NavigateDown);
    key(window,ui::UiAction::NavigateRight);key(window,ui::UiAction::NavigateRight);key(window,ui::UiAction::Confirm);
    const auto* payload=requests.received.empty()?nullptr:scene::try_scene_payload<game::scene::FlightPayload>(requests.received.back().route.payload);
    require(payload&&payload->level==15,"Keyboard navigation must reach the fifth row and its last level");requests.received.clear();
    key(window,ui::UiAction::NavigateDown);key(window,ui::UiAction::Confirm);
    require(requests.received.size()==1&&requests.received.back().route.target==game::scene_keys::MainMenu,"Back must remain reachable below the five-row grid");
    store.data=original;
}
void test_menu_routes(){
    io::ContentRegistry registry;
    scene::SceneRuntimeContext context(nullptr,registry,1280,720);
    const std::array<scene::SceneKey,4> targets{game::scene_keys::Selection,game::scene_keys::Achievements,game::scene_keys::Settings,game::scene_keys::Credits};
    for(int target=0;target<4;++target){
        game::scene::MainMenuScene menu;Requests requests;menu.attach(&requests);scene::SceneTestAccess::bind(menu,context);
        scene::SceneTestAccess::enter(menu,{});auto& window=scene::SceneTestAccess::window(menu);window.update(0);
        bool full_background=false;
        for(std::size_t index=0;index<window.child_count();++index)if(auto* background=dynamic_cast<game::frontend::MoonBackdrop*>(window.child_at(index)))full_background=background->screen_rect()==window.screen_rect();
        require(full_background&&window.child_count()==3&&!window.child_at(2)->is_visible(),"Main menu must show its background and navigation with the exit confirmation initially hidden");
        for(int step=0;step<target;++step)key(window,ui::UiAction::NavigateDown);
        key(window,ui::UiAction::Confirm);
        require(requests.received.size()==1&&requests.received.back().route.target==targets[target],"Every main menu entry must be reachable and activate once from the keyboard");
    }
}
void test_achievement_pages(){
    int total=0;
    io::ContentRegistry registry;scene::SceneRuntimeContext context(nullptr,registry,640,960);
    for(int page=0;page<3;++page){
        game::scene::AchievementsScene album;Requests requests;album.attach(&requests);
        scene::SceneTestAccess::bind(album,context);
        scene::SceneTestAccess::enter(album,game::scene::AchievementsPayload{page});
        auto& window=scene::SceneTestAccess::window(album);window.update(0);
        auto& list=dynamic_cast<ui::UiListContainer&>(*window.child_at(0));
        auto& cards=dynamic_cast<ui::UiGridContainer&>(*list.child_at(1));
        auto& pages=dynamic_cast<ui::UiGridContainer&>(*list.child_at(2));
        require(window.screen_rect()==core::Rect{0,0,640,960}&&cards.column_count()==2,"Phone achievement pages must use the portrait viewport and two columns");
        for(std::size_t index=0;index<cards.child_count();++index){const auto rect=cards.child_at(index)->screen_rect();if(!(rect.x()>=24-.001f&&rect.right()<=616+.001f&&rect.y()>=96-.001f&&rect.bottom()<=pages.screen_rect().y()-16+.001f))throw std::runtime_error("Phone achievement card escaped: "+std::to_string(rect.x())+","+std::to_string(rect.y())+","+std::to_string(rect.width())+","+std::to_string(rect.height())+"; pages y="+std::to_string(pages.screen_rect().y()));}
        key(window,ui::UiAction::Confirm);
        require(requests.received.size()==1,"Current achievement page must retain keyboard focus after rebuilding");
        const auto* focused_page=scene::try_scene_payload<game::scene::AchievementsPayload>(requests.received.back().route.payload);
        require(focused_page&&focused_page->page==page,"Keyboard focus must follow the selected achievement page");requests.received.clear();
        require(cards.child_count()==(page==2?5:6),"Achievement album must contain all 17 original descriptions across 3 pages");total+=static_cast<int>(cards.child_count());
        for(int target=0;target<3;++target){
            require(dynamic_cast<ui::UiButton&>(*pages.child_at(target)).style().chrome.corner_radius>0,"Achievement tabs must retain rounded geometry when selected");
            click(window,*pages.child_at(target));
            require(requests.received.size()==1,"Every achievement page must be clickable exactly once");
            const auto& request=requests.received.back();const auto* payload=scene::try_scene_payload<game::scene::AchievementsPayload>(request.route.payload);
            require(request.route.target==game::scene_keys::Achievements&&payload&&payload->page==target&&request.route.reload_mode==scene::SceneReloadMode::Recreate,"Achievement page route must retain its selected page");requests.received.clear();
        }
        click(window,*list.child_at(3));require(requests.received.size()==1&&requests.received.back().route.target==game::scene_keys::MainMenu,"Achievement Back must remain available on every page");
    }
    require(total==17,"Achievement album lost original content");
}
}

int main() {
    try {
        require(elysia::io::PathManager::instance()->initialize(std::filesystem::current_path()).has_value(),"Path initialize failed");
        const auto directory=std::filesystem::path("out/qa/selection")/
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(directory);
        require(ELYSIA_SAVE->initialize(directory).has_value(),"Isolated save initialize failed");
        test_mouse(); test_keyboard();test_menu_routes();test_achievement_pages();test_level_scores();test_unlocked_routes();
        ELYSIA_SAVE->shutdown();
        std::cout<<"PASS: selection mouse/keyboard, main menu routes, all 17 achievements and all album page / Back buttons\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n'; return 1;
    }
}
