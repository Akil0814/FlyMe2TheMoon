#include "debug_draw_projection.h"

#include <type_traits>
#include <variant>

namespace elysia::core
{
namespace
{
[[nodiscard]] bool category_is_enabled(elysia::tools::DebugDrawCategory enabled_categories,elysia::tools::DebugDrawCategory category) noexcept
{
    using enum elysia::tools::DebugDrawCategory;
    return (enabled_categories & category) != None;
}

[[nodiscard]] UiStrokeWidth debug_stroke(float thickness) noexcept
{
    return UiStrokeWidth{
        UiStrokeWidthMode::Logical,
        thickness
    };
}
}

void append_projected_debug_draw_commands(
    std::span<const elysia::tools::DebugDrawCommand> debug_commands,
    elysia::tools::DebugDrawCategory enabled_categories,
    const elysia::camera::Camera& camera,
    std::vector<UiRenderCommand>& out_commands)
{
    for (const elysia::tools::DebugDrawCommand& debug_command : debug_commands)
    {
        if (!category_is_enabled(enabled_categories, debug_command.category))
            continue;

        std::visit(
            [&](const auto& primitive)
            {
                using Primitive = std::decay_t<decltype(primitive)>;

                if constexpr (std::is_same_v<Primitive, elysia::tools::DebugDrawRect>)
                {
                    out_commands.push_back(make_ui_draw_rect_command(
                        camera.world_to_screen(primitive.rect),
                        debug_command.color,
                        0.0f,
                        debug_stroke(debug_command.thickness)
                    ));
                }
                else if constexpr (std::is_same_v<Primitive, elysia::tools::DebugDrawCircle>)
                {
                    out_commands.push_back(make_ui_draw_circle_command(
                        camera.world_to_screen(primitive.center),
                        primitive.radius * camera.zoom(),
                        debug_command.color,
                        debug_stroke(debug_command.thickness)
                    ));
                }
                else if constexpr (std::is_same_v<Primitive, elysia::tools::DebugDrawLine>)
                {
                    out_commands.push_back(make_ui_draw_line_command(
                        camera.world_to_screen(primitive.start),
                        camera.world_to_screen(primitive.end),
                        debug_command.color,
                        debug_stroke(debug_command.thickness)
                    ));
                }
                else if constexpr (std::is_same_v<Primitive, elysia::tools::DebugDrawPoint>)
                {
                    out_commands.push_back(make_ui_fill_circle_command(
                        camera.world_to_screen(primitive.position),
                        primitive.size * 0.5f,
                        debug_command.color
                    ));
                }
            },
            debug_command.primitive
        );
    }
}
}
