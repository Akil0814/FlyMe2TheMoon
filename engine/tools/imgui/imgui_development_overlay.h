#pragma once

#include "../development_overlay.h"

#include <vector>

struct ImGuiContext;

namespace elysia::tools
{
class ImGuiDevelopmentOverlay final : public IDevelopmentOverlay
{
public:
    ImGuiDevelopmentOverlay() = default;
    ~ImGuiDevelopmentOverlay() override;

    [[nodiscard]] std::expected<void, std::string> initialize(
        SDL_Window& window,
        SDL_Renderer& renderer) override;
    void process_event(const SDL_Event& event) override;
    void begin_frame(double delta_seconds) override;
    void render(SDL_Renderer& renderer) override;
    void shutdown() noexcept override;

    [[nodiscard]] elysia::input::InputCapture
        captured_input() const noexcept override;

    [[nodiscard]] DevelopmentPanelHandle register_panel(
        std::string stable_id,
        DrawCallback draw) override;
    [[nodiscard]] bool unregister_panel(
        DevelopmentPanelHandle handle) override;

private:
    struct PanelEntry
    {
        DevelopmentPanelHandle handle{};
        std::string stable_id;
        DrawCallback draw;
    };

    struct PendingOperation
    {
        bool add = false;
        PanelEntry panel;
        DevelopmentPanelHandle handle{};
    };

    [[nodiscard]] bool has_panel_id(const std::string& stable_id) const;
    [[nodiscard]] bool has_panel_handle(DevelopmentPanelHandle handle) const;
    void apply_pending_operations();
    void set_current_context() const noexcept;

    ImGuiContext* _context = nullptr;
    std::vector<PanelEntry> _panels;
    std::vector<PendingOperation> _pending_operations;
    std::uint64_t _next_panel_handle = 1;
    elysia::input::InputCapture _captured_input =
        elysia::input::InputCapture::None;
    bool _sdl_platform_initialized = false;
    bool _sdl_renderer_initialized = false;
    bool _frame_started = false;
    bool _drawing_panels = false;
};
}
