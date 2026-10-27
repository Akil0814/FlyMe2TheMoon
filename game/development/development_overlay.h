#pragma once
#include "engine/tools/development_overlay.h"
#include "game/platform/idle_cursor.h"
#if ELYSIA_ENABLE_IMGUI
#include "engine/tools/imgui/imgui_development_overlay.h"
#include <array>
#endif

namespace game::development {
// The application host runs the game cursor policy globally, even without ImGui.
// Developer panels share that lifetime and use the engine's ImGui/SDL integration.
class DevelopmentOverlay final:public elysia::tools::IDevelopmentOverlay {
public:
    explicit DevelopmentOverlay(game::platform::IdleCursor::Sampler sampler=nullptr):_idle_cursor(sampler){}
    static constexpr const char* MainPanelTitle="Development Panel (F2)";
    ~DevelopmentOverlay()override{shutdown();}
    std::expected<void,std::string> initialize(SDL_Window& window,SDL_Renderer& renderer)override;
    void process_event(const SDL_Event& event)override;
    void begin_frame(double delta)override;
    void render(SDL_Renderer& renderer)override;
    void shutdown()noexcept override;
    void set_panels_visible(bool visible)noexcept override;
    bool has_background_content()const noexcept override{return _initialized;}
    elysia::input::InputCapture captured_input()const noexcept override;
    elysia::tools::DevelopmentPanelHandle register_panel(std::string stable_id,DrawCallback draw)override;
    bool unregister_panel(elysia::tools::DevelopmentPanelHandle handle)override;
private:
    game::platform::IdleCursor _idle_cursor;
    bool _initialized=false,_panels_visible=false;
#if ELYSIA_ENABLE_IMGUI
    void draw_tools();
    void draw_fps();
    elysia::tools::ImGuiDevelopmentOverlay _overlay;
    ImGuiContext* _context=nullptr;
    bool _show_fps=false;
    std::array<float,120> _frame_times{};
    int _frame_offset=0,_frame_count=0;
#endif
};
}
