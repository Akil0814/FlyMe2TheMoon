#pragma once

#include <SDL3/SDL.h>

#include "color.h"
#include "../geometry/rect.h"
#include "../geometry/vector2.h"

#include <algorithm>
#include <cmath>

namespace elysia::core
{
[[nodiscard]] inline SDL_Color to_sdl_color(Color color) noexcept
{
    return SDL_Color{ color.r, color.g, color.b, color.a };
}

[[nodiscard]] inline SDL_FColor to_sdl_fcolor(SDL_Color color) noexcept
{
    return {color.r/255.0f,color.g/255.0f,color.b/255.0f,color.a/255.0f};
}
[[nodiscard]] inline SDL_FRect to_sdl_frect(SDL_Rect rect) noexcept
{
    return {static_cast<float>(rect.x),static_cast<float>(rect.y),static_cast<float>(rect.w),static_cast<float>(rect.h)};
}
[[nodiscard]] inline Color from_sdl_color(SDL_Color color) noexcept
{
    return Color{ color.r, color.g, color.b, color.a };
}

[[nodiscard]] inline SDL_Point to_sdl_point(const Vector2& point) noexcept
{
    return SDL_Point{
        static_cast<int>(point.x),
        static_cast<int>(point.y)
    };
}

[[nodiscard]] inline SDL_Rect to_sdl_rect(const Rect& rect) noexcept
{
    SDL_Rect sdl_rect{};
    sdl_rect.x = static_cast<int>(rect.x());
    sdl_rect.y = static_cast<int>(rect.y());

    const int width = static_cast<int>(rect.width());
    const int height = static_cast<int>(rect.height());
    sdl_rect.w = width > 0 ? width : 0;
    sdl_rect.h = height > 0 ? height : 0;
    return sdl_rect;
}

[[nodiscard]] inline SDL_FRect to_sdl_frect(const Rect& rect) noexcept
{
    SDL_FRect sdl_rect{};
    sdl_rect.x = rect.x();
    sdl_rect.y = rect.y();
    sdl_rect.w = rect.width();
    sdl_rect.h = rect.height();
    return sdl_rect;
}

[[nodiscard]] inline SDL_Rect to_sdl_covering_rect(const Rect& rect) noexcept
{
    const float left = std::floor(rect.left());
    const float top = std::floor(rect.top());
    const float right = std::ceil(rect.right());
    const float bottom = std::ceil(rect.bottom());

    return SDL_Rect{
        static_cast<int>(left),
        static_cast<int>(top),
        std::max(0,static_cast<int>(right - left)),
        std::max(0,static_cast<int>(bottom - top))
    };
}

}
