#pragma once

#include "../input/input_capture.h"

#include <cstdint>
#include <expected>
#include <functional>
#include <string>

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;

namespace elysia::tools
{
struct DevelopmentPanelHandle
{
    std::uint64_t value = 0;

    [[nodiscard]] constexpr bool is_valid() const noexcept
    {
        return value != 0;
    }

    [[nodiscard]] constexpr bool operator==(
        const DevelopmentPanelHandle&) const noexcept = default;
};

inline constexpr DevelopmentPanelHandle InvalidDevelopmentPanelHandle{};

class IDevelopmentPanelRegistry
{
public:
    using DrawCallback = std::function<void()>;

    virtual ~IDevelopmentPanelRegistry() = default;

    [[nodiscard]] virtual DevelopmentPanelHandle register_panel(
        std::string stable_id,
        DrawCallback draw) = 0;
    [[nodiscard]] virtual bool unregister_panel(
        DevelopmentPanelHandle handle) = 0;
};

class IDevelopmentOverlay : public IDevelopmentPanelRegistry
{
public:
    ~IDevelopmentOverlay() override = default;

    [[nodiscard]] virtual std::expected<void, std::string> initialize(
        SDL_Window& window,
        SDL_Renderer& renderer) = 0;
    virtual void process_event(const SDL_Event& event) = 0;
    virtual void begin_frame(double delta_seconds) = 0;
    virtual void render(SDL_Renderer& renderer) = 0;
    virtual void shutdown() noexcept = 0;

    [[nodiscard]] virtual elysia::input::InputCapture
        captured_input() const noexcept = 0;
};
}
