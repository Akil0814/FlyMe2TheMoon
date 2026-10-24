#include "application_sdl_presentation.h"
#include <SDL3/SDL.h>
namespace elysia::application::detail
{
std::expected<void,std::string> configure_sdl_render_hints(const ApplicationRenderSettings& settings)
{
    if (settings.texture_filter != ApplicationTextureFilter::Nearest && settings.texture_filter != ApplicationTextureFilter::Linear)
        return std::unexpected("Unsupported application texture filter.");
    return {};
}
std::expected<void,std::string> configure_sdl_texture_filter(SDL_Renderer* renderer,const ApplicationRenderSettings& settings)
{
    if (auto valid = configure_sdl_render_hints(settings); !valid) return valid;
    if (!SDL_SetDefaultTextureScaleMode(renderer,settings.texture_filter == ApplicationTextureFilter::Nearest ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR))
        return std::unexpected(std::string(SDL_GetError()));
    return {};
}
std::expected<void,std::string> configure_sdl_renderer_presentation(SDL_Renderer* renderer,int width,int height)
{
    if (!renderer) return std::unexpected("SDL renderer presentation requires a renderer.");
    if (!SDL_SetRenderLogicalPresentation(renderer,width,height,SDL_LOGICAL_PRESENTATION_LETTERBOX))
        return std::unexpected(std::string(SDL_GetError()));
    return {};
}
}
