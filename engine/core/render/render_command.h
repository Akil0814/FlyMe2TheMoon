#pragma once

#include <SDL3/SDL.h>

#include "color.h"
#include "../geometry/rect.h"
#include "../geometry/vector2.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <optional>
#include <vector>

namespace elysia::core
{
enum class SpriteFlip
{
    None,
    Horizontal,
    Vertical,
    Both
};

enum class RenderCommandType
{
    Texture,
    FillRect,
    DrawRect,
    FillCircle,
    DrawCircle,
    DrawLine,
    FillTriangle
};

enum class UiRenderCommandType
{
    Texture,
    FillRect,
    DrawRect,
    FillRoundedRect,
    DrawRoundedRect,
    DrawLine,
    FillCircle,
    DrawCircle
};

struct TextureColorModulation
{
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;

    [[nodiscard]] constexpr bool operator==(
        const TextureColorModulation&) const noexcept = default;
};

enum class UiStrokeWidthMode
{
    Hairline,
    Logical
};

struct UiStrokeWidth
{
    UiStrokeWidthMode mode = UiStrokeWidthMode::Hairline;
    float logical_width = 1.0f;

    [[nodiscard]] constexpr bool operator==(const UiStrokeWidth&) const noexcept = default;
};

struct RenderCommand
{
    RenderCommandType type = RenderCommandType::Texture;

    SDL_Texture* texture = nullptr;

    // Destination rectangle in world space.
    // This remains world-space until it is projected into a ScreenRenderCommand.
    Rect command_rect{};

    // Used by world-space primitive commands.
    Color color{};
    float stroke_width = 1.0f;
    Vector2 line_start{};
    Vector2 line_end{};
    Vector2 circle_center{};
    float circle_radius = 0.0f;
    std::array<Vector2,3> triangle_vertices{};

    std::uint8_t alpha = 255;
    std::optional<TextureColorModulation> texture_color_modulation;

    // Source rectangle in texture space.
    // Defines which part of the texture or sprite sheet should be drawn.
    bool use_src_rect = false;
    Rect src_rect{};

    // Clockwise rotation angle in degrees.
    double rotation_degrees = 0.0;

    // Normalized rotation origin inside _world_rect.
    // (0.5, 0.5) -> center, (0, 0) -> top-left
    Vector2 rotation_origin = Vector2(0.5f, 0.5f);

    SpriteFlip flip = SpriteFlip::None;
};

struct ScreenRenderCommand
{
    RenderCommandType type = RenderCommandType::Texture;

    SDL_Texture* texture = nullptr;

    // Destination rectangle in screen space.
    Rect screen_rect{};

    // Primitive geometry after camera projection. Stroke width and radius are
    // expressed in logical screen units.
    Color color{};
    float stroke_width = 1.0f;
    Vector2 line_start{};
    Vector2 line_end{};
    Vector2 circle_center{};
    float circle_radius = 0.0f;
    std::array<Vector2,3> triangle_vertices{};

    std::uint8_t alpha = 255;
    std::optional<TextureColorModulation> texture_color_modulation;

    bool use_src_rect = false;
    Rect src_rect{};

    double rotation_degrees = 0.0;
    Vector2 rotation_origin = Vector2(0.5f, 0.5f);
    SpriteFlip flip = SpriteFlip::None;
};

struct UiRenderCommand
{
    UiRenderCommandType type = UiRenderCommandType::Texture;

    SDL_Texture* texture = nullptr;

    // Destination rectangle in screen/UI space.
    // This is not affected by the world camera.
    Rect screen_rect{};

    // Optional clipping rectangle in screen/UI space.
    bool use_clip_rect = false;
    Rect clip_rect{};

    // Used by rectangle, rounded rectangle, and line commands.
    Color color{};

    // Used by outline commands. Factories normalize this before it reaches a
    // renderer backend.
    UiStrokeWidth stroke_width{};

    // Used by FillRoundedRect and DrawRoundedRect. Command factories guarantee
    // that this is finite and in [0, min(width, height) / 2].
    float corner_radius = 0.0f;

    // Used by DrawLine.
    Vector2 line_start{};
    Vector2 line_end{};

    // Used by FillCircle and DrawCircle.
    Vector2 circle_center{};
    float circle_radius = 0.0f;

    // Used by Texture.
    std::uint8_t alpha = 255;
    std::optional<TextureColorModulation> texture_color_modulation;

    // Source rectangle in texture space.
    // Defines which part of the texture or sprite sheet should be drawn.
    // If false, the full texture is drawn.
    bool use_src_rect = false;
    Rect src_rect{};

    // Clockwise rotation angle in degrees.
    double rotation_degrees = 0.0;

    // Normalized rotation origin inside screen_rect.
    // (0.5, 0.5) -> center, (0, 0) -> top-left.
    Vector2 rotation_origin = Vector2(0.5f, 0.5f);

    SpriteFlip flip = SpriteFlip::None;
};

inline void set_ui_command_clip_rect(UiRenderCommand& command, const Rect& clip_rect) noexcept
{
    command.use_clip_rect = true;
    command.clip_rect = clip_rect;
}

// The single semantic normalization point for UI corner radii. Callers and
// render backends consume its result without applying their own radius policy.
[[nodiscard]] inline float normalize_ui_corner_radius(
    const Rect& rect,
    float requested_radius
) noexcept
{
    if (rect.is_empty() || !std::isfinite(requested_radius) || requested_radius <= 0.0f)
        return 0.0f;
    return std::min(requested_radius,0.5f * std::min(rect.width(),rect.height()));
}

[[nodiscard]] inline UiStrokeWidth normalize_ui_stroke_width(
    UiStrokeWidth requested_width
) noexcept
{
    switch (requested_width.mode)
    {
    case UiStrokeWidthMode::Hairline:
        return UiStrokeWidth{};

    case UiStrokeWidthMode::Logical:
        if (!std::isfinite(requested_width.logical_width)
            || requested_width.logical_width <= 0.0f)
        {
            requested_width.logical_width = 1.0f;
        }
        return requested_width;
    }

    return UiStrokeWidth{};
}

[[nodiscard]] inline float normalize_world_stroke_width(float requested_width) noexcept
{
    return std::isfinite(requested_width) && requested_width > 0.0f
        ? requested_width
        : 1.0f;
}

[[nodiscard]] inline RenderCommand make_world_fill_rect_command(
    const Rect& world_rect,
    Color color
) noexcept
{
    RenderCommand command;
    command.type = RenderCommandType::FillRect;
    command.command_rect = world_rect;
    command.color = color;
    return command;
}

[[nodiscard]] inline RenderCommand make_world_draw_rect_command(
    const Rect& world_rect,
    Color color,
    float stroke_width = 1.0f
) noexcept
{
    RenderCommand command;
    command.type = RenderCommandType::DrawRect;
    command.command_rect = world_rect;
    command.color = color;
    command.stroke_width = normalize_world_stroke_width(stroke_width);
    return command;
}

[[nodiscard]] inline RenderCommand make_world_fill_circle_command(
    const Vector2& center,
    float radius,
    Color color
) noexcept
{
    RenderCommand command;
    command.type = RenderCommandType::FillCircle;
    command.circle_center = center;
    command.circle_radius = radius;
    command.color = color;
    return command;
}

[[nodiscard]] inline RenderCommand make_world_draw_circle_command(
    const Vector2& center,
    float radius,
    Color color,
    float stroke_width = 1.0f
) noexcept
{
    RenderCommand command;
    command.type = RenderCommandType::DrawCircle;
    command.circle_center = center;
    command.circle_radius = radius;
    command.color = color;
    command.stroke_width = normalize_world_stroke_width(stroke_width);
    return command;
}

[[nodiscard]] inline RenderCommand make_world_draw_line_command(
    const Vector2& start,
    const Vector2& end,
    Color color,
    float stroke_width = 1.0f
) noexcept
{
    RenderCommand command;
    command.type = RenderCommandType::DrawLine;
    command.line_start = start;
    command.line_end = end;
    command.color = color;
    command.stroke_width = normalize_world_stroke_width(stroke_width);
    return command;
}

[[nodiscard]] inline RenderCommand make_world_fill_triangle_command(
    const Vector2& a,
    const Vector2& b,
    const Vector2& c,
    Color color
) noexcept
{
    RenderCommand command;
    command.type = RenderCommandType::FillTriangle;
    command.triangle_vertices = { a,b,c };
    command.color = color;
    return command;
}

[[nodiscard]] inline UiRenderCommand make_ui_texture_command(
    SDL_Texture* texture,
    const Rect& screen_rect,
    std::uint8_t alpha = 255
) noexcept
{
    UiRenderCommand command;
    command.type = UiRenderCommandType::Texture;
    command.texture = texture;
    command.screen_rect = screen_rect;
    command.alpha = alpha;
    return command;
}

[[nodiscard]] inline UiRenderCommand make_ui_texture_command(
    SDL_Texture* texture,
    const Rect& screen_rect,
    const Rect& clip_rect,
    std::uint8_t alpha = 255
) noexcept
{
    UiRenderCommand command = make_ui_texture_command(texture, screen_rect, alpha);
    set_ui_command_clip_rect(command, clip_rect);
    return command;
}

[[nodiscard]] inline UiRenderCommand make_ui_fill_rect_command(
    const Rect& screen_rect,
    Color color,
    float corner_radius = 0.0f
) noexcept
{
    UiRenderCommand command;
    command.corner_radius = normalize_ui_corner_radius(screen_rect,corner_radius);
    command.type = command.corner_radius > 0.0f
        ? UiRenderCommandType::FillRoundedRect
        : UiRenderCommandType::FillRect;
    command.screen_rect = screen_rect;
    command.color = color;
    return command;
}

[[nodiscard]] inline UiRenderCommand make_ui_fill_rect_command(
    const Rect& screen_rect,
    Color color,
    const Rect& clip_rect,
    float corner_radius = 0.0f
) noexcept
{
    UiRenderCommand command = make_ui_fill_rect_command(screen_rect,color,corner_radius);
    set_ui_command_clip_rect(command, clip_rect);
    return command;
}

[[nodiscard]] inline UiRenderCommand make_ui_draw_rect_command(
    const Rect& screen_rect,
    Color color,
    float corner_radius = 0.0f,
    UiStrokeWidth stroke_width = {}
) noexcept
{
    UiRenderCommand command;
    command.corner_radius = normalize_ui_corner_radius(screen_rect,corner_radius);
    command.type = command.corner_radius > 0.0f
        ? UiRenderCommandType::DrawRoundedRect
        : UiRenderCommandType::DrawRect;
    command.screen_rect = screen_rect;
    command.color = color;
    command.stroke_width = normalize_ui_stroke_width(stroke_width);
    return command;
}

[[nodiscard]] inline UiRenderCommand make_ui_draw_rect_command(
    const Rect& screen_rect,
    Color color,
    const Rect& clip_rect,
    float corner_radius = 0.0f,
    UiStrokeWidth stroke_width = {}
) noexcept
{
    UiRenderCommand command = make_ui_draw_rect_command(
        screen_rect,color,corner_radius,stroke_width);
    set_ui_command_clip_rect(command, clip_rect);
    return command;
}

// Emits a header fill whose top corners match its outer surface while its
// bottom edge remains square. Radius normalization happens exactly once here.
inline void append_ui_fill_top_rounded_rect_commands(std::vector<UiRenderCommand>& out_commands,
    const Rect& outer_rect,const Rect& header_rect,
    Color color,float corner_radius) noexcept
{
    if (header_rect.is_empty())
        return;

    const float radius = normalize_ui_corner_radius(outer_rect,corner_radius);
    if (radius <= 0.0f)
    {
        out_commands.push_back(make_ui_fill_rect_command(header_rect,color));
        return;
    }

    Rect cap_rect = header_rect;
    cap_rect.set_height(std::max(header_rect.height(),2.0f * radius));
    UiRenderCommand cap;
    cap.type = UiRenderCommandType::FillRoundedRect;
    cap.screen_rect = cap_rect;
    cap.color = color;
    cap.corner_radius = radius;
    set_ui_command_clip_rect(cap,header_rect);
    out_commands.push_back(cap);

    const float square_top = std::min(header_rect.bottom(),header_rect.top() + radius);
    if (square_top < header_rect.bottom())
    {
        out_commands.push_back(make_ui_fill_rect_command(
            Rect{ header_rect.left(),square_top,header_rect.width(),header_rect.bottom() - square_top },
            color));
    }
}

[[nodiscard]] inline UiRenderCommand make_ui_draw_line_command(
    const Vector2& line_start,
    const Vector2& line_end,
    Color color,
    UiStrokeWidth stroke_width = {}
) noexcept
{
    UiRenderCommand command;
    command.type = UiRenderCommandType::DrawLine;
    command.line_start = line_start;
    command.line_end = line_end;
    command.color = color;
    command.stroke_width = normalize_ui_stroke_width(stroke_width);
    return command;
}

[[nodiscard]] inline UiRenderCommand make_ui_draw_line_command(
    const Vector2& line_start,
    const Vector2& line_end,
    Color color,
    const Rect& clip_rect,
    UiStrokeWidth stroke_width = {}
) noexcept
{
    UiRenderCommand command = make_ui_draw_line_command(
        line_start,line_end,color,stroke_width);
    set_ui_command_clip_rect(command, clip_rect);
    return command;
}

[[nodiscard]] inline UiRenderCommand make_ui_fill_circle_command(
    const Vector2& circle_center,
    float circle_radius,
    Color color
) noexcept
{
    UiRenderCommand command;
    command.type = UiRenderCommandType::FillCircle;
    command.circle_center = circle_center;
    command.circle_radius = circle_radius;
    command.color = color;
    return command;
}

[[nodiscard]] inline UiRenderCommand make_ui_fill_circle_command(
    const Vector2& circle_center,
    float circle_radius,
    Color color,
    const Rect& clip_rect
) noexcept
{
    UiRenderCommand command = make_ui_fill_circle_command(circle_center, circle_radius, color);
    set_ui_command_clip_rect(command, clip_rect);
    return command;
}

[[nodiscard]] inline UiRenderCommand make_ui_draw_circle_command(
    const Vector2& circle_center,
    float circle_radius,
    Color color,
    UiStrokeWidth stroke_width = {}
) noexcept
{
    UiRenderCommand command;
    command.type = UiRenderCommandType::DrawCircle;
    command.circle_center = circle_center;
    command.circle_radius = circle_radius;
    command.color = color;
    command.stroke_width = normalize_ui_stroke_width(stroke_width);
    return command;
}

[[nodiscard]] inline UiRenderCommand make_ui_draw_circle_command(
    const Vector2& circle_center,
    float circle_radius,
    Color color,
    const Rect& clip_rect,
    UiStrokeWidth stroke_width = {}
) noexcept
{
    UiRenderCommand command = make_ui_draw_circle_command(
        circle_center,circle_radius,color,stroke_width);
    set_ui_command_clip_rect(command, clip_rect);
    return command;
}

}
