#pragma once

struct SDL_Renderer;

namespace elysia::io
{
struct ContentRegistry;
}

namespace elysia::typography
{
class FontResolver;
}

namespace elysia::tools
{
class IDevelopmentPanelRegistry;
}

namespace elysia::scene
{
class SceneRuntimeContext
{
public:
    SceneRuntimeContext(
        SDL_Renderer* renderer,
        const elysia::io::ContentRegistry& content_registry,
        int logical_width,
        int logical_height,
        elysia::typography::FontResolver* font_resolver = nullptr,
        elysia::tools::IDevelopmentPanelRegistry* development_panels = nullptr
    ) noexcept;

    [[nodiscard]] SDL_Renderer* renderer() const noexcept;
    [[nodiscard]] const elysia::io::ContentRegistry& content_registry() const noexcept;
    [[nodiscard]] int logical_width() const noexcept;
    [[nodiscard]] int logical_height() const noexcept;
    [[nodiscard]] elysia::typography::FontResolver* font_resolver() const noexcept;
    [[nodiscard]] elysia::tools::IDevelopmentPanelRegistry*
        development_panels() const noexcept;

private:
    SDL_Renderer* _renderer = nullptr;
    const elysia::io::ContentRegistry* _content_registry = nullptr;
    int _logical_width = 0;
    int _logical_height = 0;
    elysia::typography::FontResolver* _font_resolver = nullptr;
    elysia::tools::IDevelopmentPanelRegistry* _development_panels = nullptr;
};
}
