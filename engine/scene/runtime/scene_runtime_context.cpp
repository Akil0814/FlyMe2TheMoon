#include "scene_runtime_context.h"

#include "../../io/loaders/asset_config_types.h"

namespace elysia::scene
{
SceneRuntimeContext::SceneRuntimeContext(
    SDL_Renderer* renderer,
    const elysia::io::ContentRegistry& content_registry,
    int logical_width,
    int logical_height,
    elysia::typography::FontResolver* font_resolver,
    elysia::tools::IDevelopmentPanelRegistry* development_panels
) noexcept
    : _renderer(renderer)
    , _content_registry(&content_registry)
    , _logical_width(logical_width)
    , _logical_height(logical_height)
    , _font_resolver(font_resolver)
    , _development_panels(development_panels)
{
}

SDL_Renderer* SceneRuntimeContext::renderer() const noexcept
{
    return _renderer;
}

const elysia::io::ContentRegistry& SceneRuntimeContext::content_registry() const noexcept
{
    return *_content_registry;
}

int SceneRuntimeContext::logical_width() const noexcept
{
    return _logical_width;
}

int SceneRuntimeContext::logical_height() const noexcept
{
    return _logical_height;
}

elysia::typography::FontResolver* SceneRuntimeContext::font_resolver() const noexcept
{
    return _font_resolver;
}

elysia::tools::IDevelopmentPanelRegistry*
SceneRuntimeContext::development_panels() const noexcept
{
    return _development_panels;
}
}
