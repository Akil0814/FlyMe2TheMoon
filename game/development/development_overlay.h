#pragma once
#if ELYSIA_ENABLE_IMGUI
#include "engine/tools/imgui/imgui_development_overlay.h"
#include <array>

namespace game::development {
// The engine owns ImGui/SDL integration; the game owns these developer tools.
class DevelopmentOverlay final:public elysia::tools::IDevelopmentOverlay {
public:
    static constexpr const char* MainPanelTitle="Development Panel (F2)";
    ~DevelopmentOverlay()override{shutdown();}
    std::expected<void,std::string> initialize(SDL_Window& window,SDL_Renderer& renderer)override;
    void process_event(const SDL_Event& event)override{_overlay.process_event(event);}
    void begin_frame(double delta)override;
    void render(SDL_Renderer& renderer)override{_overlay.render(renderer);}
    void shutdown()noexcept override;
    void set_panels_visible(bool visible)noexcept override;
    bool has_background_content()const noexcept override{return _show_fps;}
    elysia::input::InputCapture captured_input()const noexcept override;
    elysia::tools::DevelopmentPanelHandle register_panel(std::string stable_id,DrawCallback draw)override;
    bool unregister_panel(elysia::tools::DevelopmentPanelHandle handle)override{return _overlay.unregister_panel(handle);}
private:
    void draw_tools();
    void draw_fps();
    elysia::tools::ImGuiDevelopmentOverlay _overlay;
    ImGuiContext* _context=nullptr;
    bool _panels_visible=false,_show_fps=false;
    std::array<float,120> _frame_times{};
    int _frame_offset=0,_frame_count=0;
};
}
#endif
