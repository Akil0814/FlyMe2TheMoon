#pragma once

#include "../geometry/rect.h"
#include "../geometry/vector2.h"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace elysia::core
{
struct GlyphRunLayoutOptions
{
    float target_height = 0.0f;
    float spacing = 0.0f;
    std::optional<float> fixed_advance;
};

struct GlyphPlacement
{
    std::size_t glyph_index = 0;
    Rect local_rect;
};

struct GlyphRunLayout
{
    std::vector<GlyphPlacement> glyphs;
    float width = 0.0f;
    float height = 0.0f;
};

[[nodiscard]] GlyphRunLayout layout_glyph_run(
    std::span<const Vector2> source_sizes,
    const GlyphRunLayoutOptions& options
);
}
