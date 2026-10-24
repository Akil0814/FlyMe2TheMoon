#pragma once

#include <SDL3/SDL.h>
#include <SDL3_gfxPrimitives.h>

#include "render_command.h"
#include "sdl_convert.h"
#include "sdl_render_boundary.h"
#include "sdl_ui_stroke_renderer.h"

#include <cmath>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

namespace elysia::core
{
[[nodiscard]] inline std::int16_t clamp_circle_component(float value) noexcept
{
    const long rounded = std::lround(value);
    if (rounded < static_cast<long>(std::numeric_limits<std::int16_t>::min()))
        return std::numeric_limits<std::int16_t>::min();
    if (rounded > static_cast<long>(std::numeric_limits<std::int16_t>::max()))
        return std::numeric_limits<std::int16_t>::max();
    return static_cast<std::int16_t>(rounded);
}

[[nodiscard]] inline SDL_FlipMode to_sdl_renderer_flip(SpriteFlip flip) noexcept
{
    switch (flip)
    {
    case SpriteFlip::Horizontal:
        return SDL_FLIP_HORIZONTAL;

    case SpriteFlip::Vertical:
        return SDL_FLIP_VERTICAL;

    case SpriteFlip::Both:
        return static_cast<SDL_FlipMode>(SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL);

    case SpriteFlip::None:
    default:
        return SDL_FLIP_NONE;
    }
}

inline void execute_textured_render_command(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const Rect& destination_rect_value,
    std::uint8_t alpha,
    const std::optional<TextureColorModulation>& texture_color_modulation,
    bool use_src_rect,
    const Rect& src_rect_value,
    double rotation_degrees,
    const Vector2& rotation_origin,
    SpriteFlip flip
)
{
    if (!texture)
        return;

    SDL_FRect destination_rect = to_sdl_frect(to_sdl_rect(destination_rect_value));
    SDL_FPoint sdl_rotation_origin{
        static_cast<float>(static_cast<int>(rotation_origin.x * destination_rect.w)),
        static_cast<float>(static_cast<int>(rotation_origin.y * destination_rect.h))
    };

    const SDL_FRect* src_rect = nullptr;
    SDL_FRect converted_src_rect{};
    if (use_src_rect)
    {
        converted_src_rect = to_sdl_frect(to_sdl_rect(src_rect_value));
        src_rect = &converted_src_rect;
    }

    detail::TextureState state(texture,texture_color_modulation.has_value());
    detail::with_render_state(state,[&] {
        state.set_alpha(alpha);
        if (texture_color_modulation)
            state.set_color(texture_color_modulation->r,texture_color_modulation->g,texture_color_modulation->b);
        detail::checked_render_operation("SDL_RenderTextureRotated",[&] {
            return SDL_RenderTextureRotated(renderer,texture,src_rect,&destination_rect,
                rotation_degrees,&sdl_rotation_origin,to_sdl_renderer_flip(flip));
        });
    });
}

namespace detail
{
inline void execute_render_command_impl(SDL_Renderer* renderer,const UiRenderCommand& render_command);
}

[[nodiscard]] inline bool finite_render_vector(const Vector2& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] inline bool valid_render_rect(const Rect& rect) noexcept
{
    return finite_render_vector(rect.position())
        && finite_render_vector(rect.size())
        && !rect.is_empty();
}

namespace detail
{
inline void execute_world_fill_rect_command(
    SDL_Renderer* renderer,
    const ScreenRenderCommand& render_command)
{
    if (!renderer || !valid_render_rect(render_command.screen_rect))
        return;

    RendererState state(renderer);
    with_render_state(state,[&] {
        state.set_clip(nullptr);
        state.set_blend(SDL_BLENDMODE_BLEND);
        state.set_color(to_sdl_color(render_command.color));
        SDL_FRect rect = to_sdl_frect(render_command.screen_rect);
        // Preserve the shared-cell coverage bias.
        constexpr float kEdgeBias = 0.001f;
        rect.w += kEdgeBias;
        rect.h += kEdgeBias;
        checked_render_operation("SDL_RenderFillRect",[&] { return SDL_RenderFillRect(renderer,&rect); });
    });
}
}

[[nodiscard]] inline bool valid_render_triangle(
    const std::array<Vector2,3>& vertices
) noexcept
{
    if (!finite_render_vector(vertices[0])
        || !finite_render_vector(vertices[1])
        || !finite_render_vector(vertices[2]))
    {
        return false;
    }

    const float twice_signed_area =
        (vertices[1] - vertices[0]).cross(vertices[2] - vertices[0]);
    return std::isfinite(twice_signed_area)
        && std::fabs(twice_signed_area) > Vector2::kEpsilon;
}

inline void execute_filled_triangle_render_command(
    SDL_Renderer* renderer,
    const ScreenRenderCommand& render_command
)
{
    if (!renderer || !valid_render_triangle(render_command.triangle_vertices))
        return;

    detail::RendererState state(renderer);
    detail::with_render_state(state,[&] {
    state.set_clip(nullptr);
    state.set_blend(SDL_BLENDMODE_BLEND);
    const SDL_Color color = to_sdl_color(render_command.color);
    const std::array<SDL_Vertex,3> vertices{
        SDL_Vertex{ SDL_FPoint{ render_command.triangle_vertices[0].x,
                                render_command.triangle_vertices[0].y },to_sdl_fcolor(color),SDL_FPoint{} },
        SDL_Vertex{ SDL_FPoint{ render_command.triangle_vertices[1].x,
                                render_command.triangle_vertices[1].y },to_sdl_fcolor(color),SDL_FPoint{} },
        SDL_Vertex{ SDL_FPoint{ render_command.triangle_vertices[2].x,
                                render_command.triangle_vertices[2].y },to_sdl_fcolor(color),SDL_FPoint{} }
    };
    detail::checked_render_operation("SDL_RenderGeometry",[&] {
        return SDL_RenderGeometry(renderer,nullptr,vertices.data(),static_cast<int>(vertices.size()),nullptr,0);
    });
    });
}

namespace detail
{
inline void execute_render_command_impl(SDL_Renderer* renderer, const ScreenRenderCommand& render_command)
{
    if (render_command.type == RenderCommandType::Texture)
    {
        execute_textured_render_command(
            renderer,
            render_command.texture,
            render_command.screen_rect,
            render_command.alpha,
            render_command.texture_color_modulation,
            render_command.use_src_rect,
            render_command.src_rect,
            render_command.rotation_degrees,
            render_command.rotation_origin,
            render_command.flip
        );
        return;
    }

    if (render_command.type == RenderCommandType::FillTriangle)
    {
        execute_filled_triangle_render_command(renderer,render_command);
        return;
    }

    UiRenderCommand primitive;
    primitive.color = render_command.color;
    primitive.stroke_width = {
        UiStrokeWidthMode::Logical,
        normalize_world_stroke_width(render_command.stroke_width)};
    switch (render_command.type)
    {
    case RenderCommandType::FillRect:
        detail::execute_world_fill_rect_command(renderer,render_command);
        return;

    case RenderCommandType::DrawRect:
        if (!valid_render_rect(render_command.screen_rect))
            return;
        primitive.type = UiRenderCommandType::DrawRect;
        primitive.screen_rect = render_command.screen_rect;
        break;

    case RenderCommandType::FillCircle:
    case RenderCommandType::DrawCircle:
        if (!finite_render_vector(render_command.circle_center)
            || !std::isfinite(render_command.circle_radius)
            || render_command.circle_radius <= 0.0f)
        {
            return;
        }
        primitive.type = render_command.type == RenderCommandType::FillCircle
            ? UiRenderCommandType::FillCircle
            : UiRenderCommandType::DrawCircle;
        primitive.circle_center = render_command.circle_center;
        primitive.circle_radius = render_command.circle_radius;
        break;

    case RenderCommandType::DrawLine:
        if (!finite_render_vector(render_command.line_start)
            || !finite_render_vector(render_command.line_end)
            || (render_command.line_end - render_command.line_start).is_zero())
        {
            return;
        }
        primitive.type = UiRenderCommandType::DrawLine;
        primitive.line_start = render_command.line_start;
        primitive.line_end = render_command.line_end;
        break;

    case RenderCommandType::Texture:
    case RenderCommandType::FillTriangle:
        return;
    }
    execute_render_command_impl(renderer,primitive);
}

inline void execute_render_command_impl(SDL_Renderer* renderer, const UiRenderCommand& render_command)
{

    detail::RendererState state(renderer);
    detail::with_render_state(state,[&] {
        SDL_Rect clip_rect{};
        if (render_command.use_clip_rect)
        {
            clip_rect = to_sdl_covering_rect(render_command.clip_rect);
            state.set_clip(&clip_rect);
        }
        else state.set_clip(nullptr);
        if (render_command.type != UiRenderCommandType::Texture)
            state.set_blend(SDL_BLENDMODE_BLEND);
    switch (render_command.type)
    {
    case UiRenderCommandType::Texture:
        execute_textured_render_command(
            renderer,
            render_command.texture,
            render_command.screen_rect,
            render_command.alpha,
            render_command.texture_color_modulation,
            render_command.use_src_rect,
            render_command.src_rect,
            render_command.rotation_degrees,
            render_command.rotation_origin,
            render_command.flip
        );
        break;

    case UiRenderCommandType::FillRect:
    {
        if (render_command.screen_rect.is_empty())
            break;

        SDL_FRect rect = to_sdl_frect(to_sdl_rect(render_command.screen_rect));
        const SDL_Color color = to_sdl_color(render_command.color);

        state.set_color(color);

        detail::checked_render_operation("SDL_RenderFillRect",[&] { return SDL_RenderFillRect(renderer,&rect); });
        break;
    }

    case UiRenderCommandType::DrawRect:
        detail::render_ui_rect_stroke(renderer,render_command);
        break;

    case UiRenderCommandType::FillRoundedRect:
    {
        if (render_command.screen_rect.is_empty())
            break;

        assert(std::isfinite(render_command.corner_radius));
        assert(render_command.corner_radius > 0.0f);
        assert(render_command.corner_radius <= 0.5f * std::min(
            render_command.screen_rect.width(),render_command.screen_rect.height()));

        const SDL_Rect rect = to_sdl_rect(render_command.screen_rect);
        const SDL_Color color = to_sdl_color(render_command.color);
        const std::int16_t x1 = clamp_circle_component(static_cast<float>(rect.x));
        const std::int16_t y1 = clamp_circle_component(static_cast<float>(rect.y));
        const std::int16_t x2 = clamp_circle_component(static_cast<float>(rect.x + rect.w - 1));
        const std::int16_t y2 = clamp_circle_component(static_cast<float>(rect.y + rect.h - 1));
        const std::int16_t radius = clamp_circle_component(render_command.corner_radius);

        detail::checked_render_operation("roundedBoxRGBA",[&] { state.mark_gfx_changes(); return roundedBoxRGBA(renderer,x1,y1,x2,y2,radius,color.r,color.g,color.b,color.a); });
        break;
    }

    case UiRenderCommandType::DrawRoundedRect:
        detail::render_ui_rounded_rect_stroke(renderer,render_command);
        break;

    case UiRenderCommandType::DrawLine:
        detail::render_ui_line_stroke(renderer,render_command);
        break;

    case UiRenderCommandType::FillCircle:
    {
        const std::int16_t radius = clamp_circle_component(render_command.circle_radius);
        if (radius <= 0)
            break;

        const SDL_Color color = to_sdl_color(render_command.color);
        const std::int16_t x = clamp_circle_component(render_command.circle_center.x);
        const std::int16_t y = clamp_circle_component(render_command.circle_center.y);

        detail::checked_render_operation("filledCircleRGBA",[&] { state.mark_gfx_changes(); return filledCircleRGBA(renderer,x,y,radius,color.r,color.g,color.b,color.a); });
        break;
    }

    case UiRenderCommandType::DrawCircle:
        detail::render_ui_circle_stroke(renderer,render_command);
        break;
    }

    });
}
} // namespace detail

[[nodiscard]] inline RenderResult execute_render_command(SDL_Renderer* renderer,const ScreenRenderCommand& command)
{
    return detail::render_boundary(renderer,[&] { detail::execute_render_command_impl(renderer,command); });
}

[[nodiscard]] inline RenderResult execute_render_command(SDL_Renderer* renderer,const UiRenderCommand& command)
{
    return detail::render_boundary(renderer,[&] { detail::execute_render_command_impl(renderer,command); });
}

template<typename Command>
[[nodiscard]] inline RenderResult execute_render_commands(
    SDL_Renderer* renderer,const std::vector<Command>& render_commands)
{
    return detail::render_boundary(renderer,[&] {
        for (const Command& command : render_commands)
            detail::execute_render_command_impl(renderer,command);
    });
}

}
