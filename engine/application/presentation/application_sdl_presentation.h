#pragma once

#include "../application_presentation_settings.h"

#include <expected>
#include <string>

struct SDL_Renderer;

namespace elysia::application::detail
{
[[nodiscard]] std::expected<void,std::string> configure_sdl_texture_filter(
    SDL_Renderer* renderer, const ApplicationRenderSettings& settings);

[[nodiscard]] std::expected<void,std::string> configure_sdl_render_hints(
    const ApplicationRenderSettings& settings);

[[nodiscard]] std::expected<void,std::string>
    configure_sdl_renderer_presentation(
        SDL_Renderer* renderer,
        int logical_width,
        int logical_height);
}
