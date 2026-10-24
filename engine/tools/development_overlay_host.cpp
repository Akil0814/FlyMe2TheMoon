#include "development_overlay_host.h"

#include <SDL3/SDL.h>

#include <utility>

namespace elysia::tools
{
namespace
{
[[nodiscard]] bool is_toggle_event(const SDL_Event& event) noexcept
{
    return (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)
        && event.key.key == SDLK_F2;
}
}

DevelopmentOverlayHost::~DevelopmentOverlayHost()
{
    shutdown();
}

void DevelopmentOverlayHost::set_overlay(
    std::unique_ptr<IDevelopmentOverlay> overlay) noexcept
{
    shutdown();
    _overlay = std::move(overlay);
}

std::expected<void, std::string> DevelopmentOverlayHost::initialize(
    SDL_Window& window,
    SDL_Renderer& renderer)
{
    if (!_overlay)
        return {};
    if (_initialized)
        return {};

    std::expected<void, std::string> result;
    try
    {
        result = _overlay->initialize(window, renderer);
    }
    catch (...)
    {
        _overlay->shutdown();
        throw;
    }
    if (!result)
    {
        _overlay->shutdown();
        return result;
    }
    _initialized = true;
    _visible = false;
    return {};
}

bool DevelopmentOverlayHost::process_event(const SDL_Event& event)
{
    if (!_initialized || !_overlay)
        return false;

    if (is_toggle_event(event))
    {
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.repeat == 0)
            _visible = !_visible;
        return true;
    }

    if (_visible)
        _overlay->process_event(event);
    return false;
}

void DevelopmentOverlayHost::begin_frame(double delta_seconds)
{
    if (_initialized && _visible && _overlay)
        _overlay->begin_frame(delta_seconds);
}

void DevelopmentOverlayHost::render(SDL_Renderer& renderer)
{
    if (_initialized && _visible && _overlay)
        _overlay->render(renderer);
}

void DevelopmentOverlayHost::shutdown() noexcept
{
    if (_overlay && _initialized)
        _overlay->shutdown();
    _overlay.reset();
    _initialized = false;
    _visible = false;
}

bool DevelopmentOverlayHost::configured() const noexcept
{
    return _overlay != nullptr;
}

bool DevelopmentOverlayHost::initialized() const noexcept
{
    return _initialized;
}

bool DevelopmentOverlayHost::visible() const noexcept
{
    return _initialized && _visible;
}

IDevelopmentPanelRegistry* DevelopmentOverlayHost::panel_registry() noexcept
{
    return _initialized ? _overlay.get() : nullptr;
}

const IDevelopmentPanelRegistry*
DevelopmentOverlayHost::panel_registry() const noexcept
{
    return _initialized ? _overlay.get() : nullptr;
}

elysia::input::InputCapture
DevelopmentOverlayHost::captured_input() const noexcept
{
    return _initialized && _visible && _overlay
        ? _overlay->captured_input()
        : elysia::input::InputCapture::None;
}
}
