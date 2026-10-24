#include "glyph_run_layout.h"

#include <algorithm>
#include <cmath>

namespace elysia::core
{
GlyphRunLayout layout_glyph_run(
    std::span<const Vector2> source_sizes,
    const GlyphRunLayoutOptions& options
)
{
    GlyphRunLayout layout;
    if (!std::isfinite(options.target_height) || options.target_height <= 0.0f)
        return layout;

    const float spacing = std::isfinite(options.spacing)
        ? std::max(0.0f,options.spacing)
        : 0.0f;
    const std::optional<float> fixed_advance = options.fixed_advance.has_value()
        && std::isfinite(*options.fixed_advance)
        ? std::optional<float>(std::max(0.0f,*options.fixed_advance))
        : std::nullopt;

    layout.glyphs.reserve(source_sizes.size());
    float cursor_x = 0.0f;
    for (std::size_t index = 0; index < source_sizes.size(); ++index)
    {
        const Vector2 source_size = source_sizes[index];
        if (!std::isfinite(source_size.x)
            || !std::isfinite(source_size.y)
            || source_size.x <= 0.0f
            || source_size.y <= 0.0f)
        {
            continue;
        }

        if (!layout.glyphs.empty())
            cursor_x += spacing;

        const float scale = options.target_height / source_size.y;
        const float render_width = source_size.x * scale;
        layout.glyphs.push_back(GlyphPlacement{
            index,
            Rect{ cursor_x,0.0f,render_width,options.target_height }
        });
        cursor_x += fixed_advance.value_or(render_width);
    }

    layout.width = cursor_x;
    layout.height = layout.glyphs.empty() ? 0.0f : options.target_height;
    return layout;
}
}
