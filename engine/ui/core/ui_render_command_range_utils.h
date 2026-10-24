#pragma once

#include "../../core/render/render_command.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace elysia::ui::render_command_range_utils
{
// Applies a container opacity multiplier to commands appended after begin.
void apply_opacity_to_range(
    std::vector<elysia::core::UiRenderCommand>& out_commands,
    std::size_t begin,
    std::uint8_t opacity
) noexcept;
// Translates every UI-space geometry field in commands appended after begin.
void apply_translation_to_range(
    std::vector<elysia::core::UiRenderCommand>& out_commands,
    std::size_t begin,
    const elysia::core::Vector2& translation
) noexcept;
// Clips commands appended after begin to the supplied child-visible region.
void apply_clip_to_range(
    std::vector<elysia::core::UiRenderCommand>& out_commands,
    std::size_t begin,
    const elysia::core::Rect& clip_rect
);
// Finalizes a child command range with the parent's opacity and optional clipping rules.
void finalize_child_command_range(
    std::vector<elysia::core::UiRenderCommand>& out_commands,
    std::size_t begin,
    std::uint8_t opacity,
    bool clip_children,
    const elysia::core::Rect& clip_rect
);
}
