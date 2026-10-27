#include "development_overlay.h"
#include <utility>
#if ELYSIA_ENABLE_IMGUI
#include "engine/tools/debug_draw.h"
#include "engine/core/time.h"
#include "game/platform/idle_cursor.h"
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#endif

namespace game::development {
#if ELYSIA_ENABLE_IMGUI
namespace {
struct CursorChangeGuard {
    ImGuiIO* io=nullptr;
    bool previously_disabled=false;
    explicit CursorChangeGuard(bool hidden){
        if(!hidden)return;
        io=&ImGui::GetIO();previously_disabled=(io->ConfigFlags&ImGuiConfigFlags_NoMouseCursorChange)!=0;
        io->ConfigFlags|=ImGuiConfigFlags_NoMouseCursorChange;
    }
    ~CursorChangeGuard(){if(io&&!previously_disabled)io->ConfigFlags&=~ImGuiConfigFlags_NoMouseCursorChange;}
};
}
#endif
std::expected<void,std::string> DevelopmentOverlay::initialize(SDL_Window& window,SDL_Renderer& renderer){
    if(_initialized)return {};
#if ELYSIA_ENABLE_IMGUI
    if(auto result=_overlay.initialize(window,renderer);!result)return result;
    _context=ImGui::GetCurrentContext();
    const auto tools=_overlay.register_panel("fly.development.tools",[this]{if(_panels_visible)draw_tools();});
    const auto fps=_overlay.register_panel("fly.development.fps",[this]{if(_show_fps)draw_fps();});
    if(!tools.is_valid()||!fps.is_valid()){shutdown();return std::unexpected("Developer tool registration failed");}
#else
    (void)renderer;
#endif
    _idle_cursor.start(&window);_initialized=true;
    return {};
}
void DevelopmentOverlay::begin_frame(double delta){
    if(!_initialized)return;
    _idle_cursor.update();
#if ELYSIA_ENABLE_IMGUI
    // Global cursor sampling continues while all developer UI is hidden.
    if(!_panels_visible&&!_show_fps)return;
    {
        if(_context)ImGui::SetCurrentContext(_context);
        CursorChangeGuard cursor(_context&&game::platform::IdleCursor::hides_cursor());
        _overlay.begin_frame(delta);
    }
    if(_context&&std::isfinite(delta)&&delta>0){
        _frame_times[_frame_offset]=static_cast<float>(delta*1000);
        _frame_offset=(_frame_offset+1)%static_cast<int>(_frame_times.size());
        _frame_count=std::min(_frame_count+1,static_cast<int>(_frame_times.size()));
    }
#else
    (void)delta;
#endif
}
void DevelopmentOverlay::process_event(const SDL_Event& event){
#if ELYSIA_ENABLE_IMGUI
    _overlay.process_event(event);
#else
    (void)event;
#endif
}
void DevelopmentOverlay::render(SDL_Renderer& renderer){
#if ELYSIA_ENABLE_IMGUI
    if(_initialized&&(_panels_visible||_show_fps))_overlay.render(renderer);
#else
    (void)renderer;
#endif
}
void DevelopmentOverlay::set_panels_visible(bool visible)noexcept{
    _panels_visible=visible;
#if ELYSIA_ENABLE_IMGUI
    if(_context&&!visible){
        ImGui::SetCurrentContext(_context);
        // A hidden panel will not receive releases for previously held input.
        auto& io=ImGui::GetIO();io.ClearEventsQueue();io.ClearInputKeys();io.ClearInputMouse();
    }
#endif
}
elysia::input::InputCapture DevelopmentOverlay::captured_input()const noexcept{
#if ELYSIA_ENABLE_IMGUI
    return _panels_visible?_overlay.captured_input():elysia::input::InputCapture::None;
#else
    return elysia::input::InputCapture::None;
#endif
}
elysia::tools::DevelopmentPanelHandle DevelopmentOverlay::register_panel(std::string stable_id,DrawCallback draw){
#if ELYSIA_ENABLE_IMGUI
    if(!draw)return elysia::tools::InvalidDevelopmentPanelHandle;
    return _overlay.register_panel(std::move(stable_id),[this,draw=std::move(draw)]{if(_panels_visible)draw();});
#else
    (void)stable_id;(void)draw;return elysia::tools::InvalidDevelopmentPanelHandle;
#endif
}
bool DevelopmentOverlay::unregister_panel(elysia::tools::DevelopmentPanelHandle handle){
#if ELYSIA_ENABLE_IMGUI
    return _overlay.unregister_panel(handle);
#else
    (void)handle;return false;
#endif
}
void DevelopmentOverlay::shutdown()noexcept{
    _idle_cursor.stop();_initialized=false;_panels_visible=false;
#if ELYSIA_ENABLE_IMGUI
    _overlay.shutdown();_context=nullptr;_panels_visible=false;_show_fps=false;
    _frame_times.fill(0);_frame_offset=0;_frame_count=0;
#endif
}
#if ELYSIA_ENABLE_IMGUI
void DevelopmentOverlay::draw_tools(){
    using namespace elysia::tools;
    ImGui::SetNextWindowPos({16,88},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({390,610},ImGuiCond_FirstUseEver);
    if(ImGui::Begin(MainPanelTitle)){
        const auto& io=ImGui::GetIO();
        ImGui::Text("%.1f FPS | %.2f ms / frame",io.Framerate,io.Framerate>0?1000.f/io.Framerate:0.f);
        ImGui::Checkbox("Show FPS overlay",&_show_fps);
        ImGui::PlotLines("##frame-times",_frame_times.data(),_frame_count,
            _frame_count==static_cast<int>(_frame_times.size())?_frame_offset:0,
            "Frame time (ms)",0,FLT_MAX,{-1,50});
        ImGui::SeparatorText("Debug draw");
        auto* debug=DebugDraw::instance();bool enabled=debug->enabled();
        if(ImGui::Checkbox("Enable debug draw",&enabled))debug->set_enabled(enabled);
        ImGui::TextDisabled("Changes appear on the next scene update.");
        if(ImGui::CollapsingHeader("Draw categories",ImGuiTreeNodeFlags_DefaultOpen)){
            struct Option{const char* label;DebugDrawCategory category;};
            static constexpr Option options[]{
                {"Colliders",DebugDrawCategory::PhysicsCollider},{"Contacts",DebugDrawCategory::PhysicsContact},
                {"Contact normals",DebugDrawCategory::PhysicsContactNormal},{"Broad phase / AABB",DebugDrawCategory::PhysicsBroadPhase},
                {"Pose history",DebugDrawCategory::PhysicsPoseHistory},{"Velocities",DebugDrawCategory::PhysicsVelocity},
                {"Joints",DebugDrawCategory::PhysicsJoint},{"Joint details",DebugDrawCategory::PhysicsJointDetails},
                {"Chain details",DebugDrawCategory::PhysicsChainDetails},{"Targets",DebugDrawCategory::PhysicsTarget},
                {"Camera",DebugDrawCategory::Camera},{"Gameplay",DebugDrawCategory::Gameplay}};
            auto categories=debug->enabled_categories();
            if(ImGui::Button("All"))categories=DebugDrawCategory::All;ImGui::SameLine();
            if(ImGui::Button("None"))categories=DebugDrawCategory::None;ImGui::SameLine();
            if(ImGui::Button("Defaults"))categories=DebugDrawCategory::Default;
            if(ImGui::BeginTable("draw-categories",2)){
                for(const auto& option:options){
                    ImGui::TableNextColumn();bool selected=(categories&option.category)!=DebugDrawCategory::None;
                    if(ImGui::Checkbox(option.label,&selected)){
                        const auto bits=static_cast<std::uint32_t>(categories),mask=static_cast<std::uint32_t>(option.category);
                        categories=static_cast<DebugDrawCategory>(selected?bits|mask:bits&~mask);
                    }
                }
                ImGui::EndTable();
            }
            debug->set_enabled_categories(categories);
        }
        ImGui::SeparatorText("Simulation");
        auto* time=ELYSIA_TIME;float scale=static_cast<float>(time->time_scale());
        if(ImGui::SliderFloat("Time scale",&scale,0,2,"%.2fx"))time->set_time_scale(scale);
        if(ImGui::Button("Pause"))time->set_time_scale(0);ImGui::SameLine();
        if(ImGui::Button("Normal speed"))time->set_time_scale(1);
        ImGui::TextDisabled("F2 hides panels; FPS overlay stays visible.");
    }
    ImGui::End();
}
void DevelopmentOverlay::draw_fps(){
    const auto* viewport=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x+viewport->WorkSize.x-12,viewport->WorkPos.y+12},ImGuiCond_Always,{1,0});
    ImGui::SetNextWindowBgAlpha(.55f);
    constexpr ImGuiWindowFlags flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoInputs;
    if(ImGui::Begin("FPS##development-hud",nullptr,flags)){
        const float fps=ImGui::GetIO().Framerate;ImGui::Text("FPS %.1f | %.2f ms",fps,fps>0?1000.f/fps:0.f);
    }
    ImGui::End();
}
#endif
}
