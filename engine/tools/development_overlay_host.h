#pragma once

#include "development_overlay.h"

#include <memory>

namespace elysia::tools
{
class DevelopmentOverlayHost final
{
public:
    DevelopmentOverlayHost() = default;
    ~DevelopmentOverlayHost();

    DevelopmentOverlayHost(const DevelopmentOverlayHost&) = delete;
    DevelopmentOverlayHost& operator=(const DevelopmentOverlayHost&) = delete;
    DevelopmentOverlayHost(DevelopmentOverlayHost&&) = delete;
    DevelopmentOverlayHost& operator=(DevelopmentOverlayHost&&) = delete;

    void set_overlay(std::unique_ptr<IDevelopmentOverlay> overlay) noexcept;
    [[nodiscard]] std::expected<void, std::string> initialize(
        SDL_Window& window,
        SDL_Renderer& renderer);
    [[nodiscard]] bool process_event(const SDL_Event& event);
    void begin_frame(double delta_seconds);
    void render(SDL_Renderer& renderer);
    void shutdown() noexcept;

    [[nodiscard]] bool configured() const noexcept;
    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] bool visible() const noexcept;
    [[nodiscard]] IDevelopmentPanelRegistry* panel_registry() noexcept;
    [[nodiscard]] const IDevelopmentPanelRegistry* panel_registry() const noexcept;
    [[nodiscard]] elysia::input::InputCapture
        captured_input() const noexcept;

private:
    std::unique_ptr<IDevelopmentOverlay> _overlay;
    bool _initialized = false;
    bool _visible = false;
};
}
