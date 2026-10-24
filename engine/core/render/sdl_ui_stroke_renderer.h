#pragma once

#include <SDL3/SDL.h>

#include "render_command.h"
#include "sdl_convert.h"
#include "sdl_render_boundary.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace elysia::core
{
namespace detail
{
inline constexpr float kUiPi = 3.14159265358979323846f;
inline constexpr int kUiCornerSegments = 8;
inline constexpr int kUiCircleSegments = 64;

[[nodiscard]] inline float snap_ui_unit_component(float value) noexcept
{
    if (std::fabs(value) < 1e-6f)
        return 0.0f;
    if (std::fabs(value - 1.0f) < 1e-6f)
        return 1.0f;
    if (std::fabs(value + 1.0f) < 1e-6f)
        return -1.0f;
    return value;
}

struct UiResolvedStrokeWidth
{
    float x = 1.0f;
    float y = 1.0f;

    [[nodiscard]] float radial() const noexcept
    {
        return std::max(x,y);
    }
};

[[nodiscard]] inline float valid_renderer_scale(float scale) noexcept
{
    return std::isfinite(scale) && std::fabs(scale) > Vector2::kEpsilon
        ? std::fabs(scale)
        : 1.0f;
}

inline void ui_output_transform(SDL_Renderer* renderer,float& sx,float& sy,float& ox,float& oy)
{
    checked_render_operation("SDL_GetRenderScale",[&] { return SDL_GetRenderScale(renderer,&sx,&sy); });
    int width=0,height=0;
    SDL_RendererLogicalPresentation mode{};
    checked_render_operation("SDL_GetRenderLogicalPresentation",[&] { return SDL_GetRenderLogicalPresentation(renderer,&width,&height,&mode); });
    SDL_FRect presentation{};
    checked_render_operation("SDL_GetRenderLogicalPresentationRect",[&] { return SDL_GetRenderLogicalPresentationRect(renderer,&presentation); });
    ox=0; oy=0;
    if (mode != SDL_LOGICAL_PRESENTATION_DISABLED && width > 0 && height > 0)
    {
        sx *= presentation.w / width; sy *= presentation.h / height;
        ox=presentation.x; oy=presentation.y;
    }
    sx=valid_renderer_scale(sx); sy=valid_renderer_scale(sy);
    SDL_Rect viewport{};
    checked_render_operation("SDL_GetRenderViewport",[&] { return SDL_GetRenderViewport(renderer,&viewport); });
    ox += viewport.x*sx; oy += viewport.y*sy;
}

[[nodiscard]] inline UiResolvedStrokeWidth resolve_ui_stroke_width(
    SDL_Renderer* renderer,
    UiStrokeWidth stroke_width
)
{
    stroke_width = normalize_ui_stroke_width(stroke_width);
    if (stroke_width.mode == UiStrokeWidthMode::Logical)
        return { stroke_width.logical_width,stroke_width.logical_width };

    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float ox=0,oy=0;
    ui_output_transform(renderer,scale_x,scale_y,ox,oy);
    return {
        1.0f / valid_renderer_scale(scale_x),
        1.0f / valid_renderer_scale(scale_y)
    };
}

[[nodiscard]] inline Vector2 snap_ui_point_to_output_pixel(
    SDL_Renderer* renderer,
    const Vector2& point
)
{
    float sx=1,sy=1,ox=0,oy=0;
    ui_output_transform(renderer,sx,sy,ox,oy);
    return {(std::round(ox+point.x*sx)-ox)/sx,(std::round(oy+point.y*sy)-oy)/sy};
}

[[nodiscard]] inline float bias_ui_outer_edge(
    float value,
    float direction
) noexcept
{
    constexpr float kEdgeBias = 0.001f;
    return value + (direction < 0.0f ? -kEdgeBias : kEdgeBias);
}

[[nodiscard]] inline SDL_Vertex make_ui_vertex(
    const Vector2& position,
    SDL_Color color
) noexcept
{
    return SDL_Vertex{
        SDL_FPoint{ position.x,position.y },
        to_sdl_fcolor(color),
        SDL_FPoint{}
    };
}

inline void render_ui_geometry(
    SDL_Renderer* renderer,
    const std::vector<SDL_Vertex>& vertices,
    const std::vector<int>& indices
)
{
    if (vertices.empty() || indices.empty())
        return;

    checked_render_operation("SDL_RenderGeometry",[&] {
        return SDL_RenderGeometry(renderer,nullptr,vertices.data(),static_cast<int>(vertices.size()),
            indices.data(),static_cast<int>(indices.size()));
    });
}

inline void render_ui_solid_polygon(
    SDL_Renderer* renderer,
    const std::vector<Vector2>& perimeter,
    SDL_Color color
)
{
    if (perimeter.size() < 3)
        return;

    std::vector<SDL_Vertex> vertices;
    vertices.reserve(perimeter.size());
    for (const Vector2& point : perimeter)
        vertices.push_back(make_ui_vertex(point,color));

    std::vector<int> indices;
    indices.reserve((perimeter.size() - 2) * 3);
    for (std::size_t i = 1; i + 1 < perimeter.size(); ++i)
    {
        indices.push_back(0);
        indices.push_back(static_cast<int>(i));
        indices.push_back(static_cast<int>(i + 1));
    }
    render_ui_geometry(renderer,vertices,indices);
}

inline void render_ui_ring(
    SDL_Renderer* renderer,
    const std::vector<Vector2>& outer,
    const std::vector<Vector2>& inner,
    SDL_Color color
)
{
    if (outer.size() < 3 || outer.size() != inner.size())
        return;

    std::vector<SDL_Vertex> vertices;
    vertices.reserve(outer.size() * 2);
    for (std::size_t i = 0; i < outer.size(); ++i)
    {
        vertices.push_back(make_ui_vertex(outer[i],color));
        vertices.push_back(make_ui_vertex(inner[i],color));
    }

    std::vector<int> indices;
    indices.reserve(outer.size() * 6);
    // Keep the two halves of each strip apart. SDL's software renderer merges
    // adjacent triangle pairs into rectangles, truncating scaled hairlines to
    // zero pixels; retain triangle rasterization for these explicit meshes.
    for (std::size_t i = 0; i < outer.size(); ++i)
    {
        const std::size_t next = (i + 1) % outer.size();
        const int outer_i = static_cast<int>(i * 2);
        const int inner_i = outer_i + 1;
        const int outer_next = static_cast<int>(next * 2);
        const int inner_next = outer_next + 1;

        indices.insert(indices.end(),{ outer_i,outer_next,inner_next });
    }
    for (std::size_t i = 0; i < outer.size(); ++i)
    {
        const int outer_i = static_cast<int>(i * 2);
        const int inner_next = static_cast<int>(((i + 1) % outer.size()) * 2 + 1);
        indices.insert(indices.end(),{ outer_i,inner_next,outer_i + 1 });
    }
    render_ui_geometry(renderer,vertices,indices);
}

[[nodiscard]] inline std::vector<Vector2> make_ui_rect_perimeter(
    float left,
    float top,
    float right,
    float bottom
)
{
    return {
        { left,top },
        { right,top },
        { right,bottom },
        { left,bottom }
    };
}

inline void append_ui_arc(
    std::vector<Vector2>& perimeter,
    const Vector2& center,
    float radius_x,
    float radius_y,
    float start_angle,
    float end_angle
)
{
    for (int i = 0; i <= kUiCornerSegments; ++i)
    {
        const float t = static_cast<float>(i)
            / static_cast<float>(kUiCornerSegments);
        const float angle = start_angle + (end_angle - start_angle) * t;
        const float cosine = snap_ui_unit_component(std::cos(angle));
        const float sine = snap_ui_unit_component(std::sin(angle));
        perimeter.emplace_back(
            center.x + cosine * radius_x,
            center.y + sine * radius_y);
    }
}

[[nodiscard]] inline std::vector<Vector2> make_ui_rounded_rect_perimeter(
    float left,
    float top,
    float right,
    float bottom,
    float radius_x,
    float radius_y
)
{
    radius_x = std::clamp(radius_x,0.0f,0.5f * std::max(0.0f,right - left));
    radius_y = std::clamp(radius_y,0.0f,0.5f * std::max(0.0f,bottom - top));

    std::vector<Vector2> perimeter;
    perimeter.reserve(static_cast<std::size_t>(4 * (kUiCornerSegments + 1)));
    append_ui_arc(
        perimeter,{ left + radius_x,top + radius_y },
        radius_x,radius_y,kUiPi,1.5f * kUiPi);
    append_ui_arc(
        perimeter,{ right - radius_x,top + radius_y },
        radius_x,radius_y,-0.5f * kUiPi,0.0f);
    append_ui_arc(
        perimeter,{ right - radius_x,bottom - radius_y },
        radius_x,radius_y,0.0f,0.5f * kUiPi);
    append_ui_arc(
        perimeter,{ left + radius_x,bottom - radius_y },
        radius_x,radius_y,0.5f * kUiPi,kUiPi);
    return perimeter;
}

inline void render_ui_rect_stroke(
    SDL_Renderer* renderer,
    const UiRenderCommand& command
)
{
    Vector2 top_left = snap_ui_point_to_output_pixel(
        renderer,command.screen_rect.top_left());
    Vector2 bottom_right = snap_ui_point_to_output_pixel(
        renderer,command.screen_rect.bottom_right());
    bottom_right.x = bias_ui_outer_edge(bottom_right.x,1.0f);
    bottom_right.y = bias_ui_outer_edge(bottom_right.y,1.0f);
    if (bottom_right.x <= top_left.x || bottom_right.y <= top_left.y)
        return;

    const UiResolvedStrokeWidth width = resolve_ui_stroke_width(
        renderer,command.stroke_width);
    const float inner_left = std::min(top_left.x + width.x,bottom_right.x);
    const float inner_top = std::min(top_left.y + width.y,bottom_right.y);
    const float inner_right = std::max(bottom_right.x - width.x,top_left.x);
    const float inner_bottom = std::max(bottom_right.y - width.y,top_left.y);
    const SDL_Color color = to_sdl_color(command.color);
    const std::vector<Vector2> outer = make_ui_rect_perimeter(
        top_left.x,top_left.y,bottom_right.x,bottom_right.y);

    if (inner_right <= inner_left || inner_bottom <= inner_top)
    {
        render_ui_solid_polygon(renderer,outer,color);
        return;
    }
    render_ui_ring(
        renderer,
        outer,
        make_ui_rect_perimeter(inner_left,inner_top,inner_right,inner_bottom),
        color);
}

inline void render_ui_rounded_rect_stroke(
    SDL_Renderer* renderer,
    const UiRenderCommand& command
)
{
    Vector2 top_left = snap_ui_point_to_output_pixel(
        renderer,command.screen_rect.top_left());
    Vector2 bottom_right = snap_ui_point_to_output_pixel(
        renderer,command.screen_rect.bottom_right());
    bottom_right.x = bias_ui_outer_edge(bottom_right.x,1.0f);
    bottom_right.y = bias_ui_outer_edge(bottom_right.y,1.0f);
    if (bottom_right.x <= top_left.x || bottom_right.y <= top_left.y)
        return;

    const UiResolvedStrokeWidth width = resolve_ui_stroke_width(
        renderer,command.stroke_width);
    const float outer_radius = command.corner_radius;
    const float inner_left = top_left.x + width.x;
    const float inner_top = top_left.y + width.y;
    const float inner_right = bottom_right.x - width.x;
    const float inner_bottom = bottom_right.y - width.y;
    const SDL_Color color = to_sdl_color(command.color);
    const std::vector<Vector2> outer = make_ui_rounded_rect_perimeter(
        top_left.x,top_left.y,bottom_right.x,bottom_right.y,
        outer_radius,outer_radius);

    if (inner_right <= inner_left || inner_bottom <= inner_top)
    {
        render_ui_solid_polygon(renderer,outer,color);
        return;
    }

    const float inner_radius = std::max(0.0f,outer_radius - width.radial());
    render_ui_ring(
        renderer,
        outer,
        make_ui_rounded_rect_perimeter(
            inner_left,inner_top,inner_right,inner_bottom,
            inner_radius,inner_radius),
        color);
}

[[nodiscard]] inline std::vector<Vector2> make_ui_ellipse_perimeter(
    const Vector2& center,
    float radius_x,
    float radius_y
)
{
    std::vector<Vector2> perimeter;
    perimeter.reserve(kUiCircleSegments);
    for (int i = 0; i < kUiCircleSegments; ++i)
    {
        const float angle = 2.0f * kUiPi * static_cast<float>(i)
            / static_cast<float>(kUiCircleSegments);
        const float cosine = snap_ui_unit_component(std::cos(angle));
        const float sine = snap_ui_unit_component(std::sin(angle));
        perimeter.emplace_back(
            center.x + cosine * radius_x,
            center.y + sine * radius_y);
    }
    return perimeter;
}

inline void render_ui_circle_stroke(
    SDL_Renderer* renderer,
    const UiRenderCommand& command
)
{
    if (!std::isfinite(command.circle_radius) || command.circle_radius <= 0.0f)
        return;

    Vector2 outer_top_left = snap_ui_point_to_output_pixel(
        renderer,
        { command.circle_center.x - command.circle_radius,
          command.circle_center.y - command.circle_radius });
    Vector2 outer_bottom_right = snap_ui_point_to_output_pixel(
        renderer,
        { command.circle_center.x + command.circle_radius,
          command.circle_center.y + command.circle_radius });
    outer_bottom_right.x = bias_ui_outer_edge(outer_bottom_right.x,1.0f);
    outer_bottom_right.y = bias_ui_outer_edge(outer_bottom_right.y,1.0f);
    const Vector2 center{
        0.5f * (outer_top_left.x + outer_bottom_right.x),
        0.5f * (outer_top_left.y + outer_bottom_right.y)
    };
    const float radius_x = 0.5f * (outer_bottom_right.x - outer_top_left.x);
    const float radius_y = 0.5f * (outer_bottom_right.y - outer_top_left.y);
    if (radius_x <= 0.0f || radius_y <= 0.0f)
        return;

    const UiResolvedStrokeWidth width = resolve_ui_stroke_width(
        renderer,command.stroke_width);
    const float inner_radius_x = radius_x - width.x;
    const float inner_radius_y = radius_y - width.y;
    const SDL_Color color = to_sdl_color(command.color);
    const std::vector<Vector2> outer = make_ui_ellipse_perimeter(
        center,radius_x,radius_y);

    if (inner_radius_x <= 0.0f || inner_radius_y <= 0.0f)
    {
        render_ui_solid_polygon(renderer,outer,color);
        return;
    }
    render_ui_ring(
        renderer,
        outer,
        make_ui_ellipse_perimeter(center,inner_radius_x,inner_radius_y),
        color);
}

inline void render_ui_line_stroke(
    SDL_Renderer* renderer,
    const UiRenderCommand& command
)
{
    const Vector2 start = snap_ui_point_to_output_pixel(
        renderer,command.line_start);
    const Vector2 end = snap_ui_point_to_output_pixel(
        renderer,command.line_end);
    const Vector2 line = end - start;
    const float length = line.length();
    const UiResolvedStrokeWidth width = resolve_ui_stroke_width(
        renderer,command.stroke_width);
    const float half_width = 0.5f * width.radial();
    const SDL_Color color = to_sdl_color(command.color);

    if (length <= Vector2::kEpsilon)
    {
        render_ui_solid_polygon(
            renderer,
            make_ui_ellipse_perimeter(start,half_width,half_width),
            color);
        return;
    }

    const Vector2 direction = line / length;
    const Vector2 normal{ -direction.y,direction.x };
    const Vector2 offset = normal * half_width;
    const Vector2 capped_start = start - direction * half_width;
    const Vector2 capped_end = end + direction * half_width;
    render_ui_solid_polygon(
        renderer,
        {
            capped_start + offset,
            capped_end + offset,
            capped_end - offset,
            capped_start - offset
        },
        color);
}
}
}
