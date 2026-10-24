#include "ui_render_command_range_utils.h"

namespace elysia::ui::render_command_range_utils
{
namespace
{
[[nodiscard]] std::uint8_t multiply_alpha(std::uint8_t a,std::uint8_t b) noexcept
{
    return static_cast<std::uint8_t>((static_cast<unsigned int>(a) * static_cast<unsigned int>(b)) / 255U);
}
}

void apply_opacity_to_range(
    std::vector<elysia::core::UiRenderCommand>& out_commands,
    std::size_t begin,
    std::uint8_t opacity
) noexcept
{
    if (opacity == 255)
        return;

    for (std::size_t index = begin; index < out_commands.size(); ++index)
    {
        elysia::core::UiRenderCommand& command = out_commands[index];
        switch (command.type)
        {
        case elysia::core::UiRenderCommandType::Texture:
            command.alpha = multiply_alpha(command.alpha,opacity);
            break;
        case elysia::core::UiRenderCommandType::FillRect:
        case elysia::core::UiRenderCommandType::DrawRect:
        case elysia::core::UiRenderCommandType::DrawLine:
            command.color.a = multiply_alpha(command.color.a,opacity);
            break;
        default:
            break;
        }
    }
}

void apply_translation_to_range(
    std::vector<elysia::core::UiRenderCommand>& out_commands,
    std::size_t begin,
    const elysia::core::Vector2& translation
) noexcept
{
    if (translation.is_zero())
        return;

    for (std::size_t index = begin; index < out_commands.size(); ++index)
    {
        elysia::core::UiRenderCommand& command = out_commands[index];
        command.screen_rect = command.screen_rect.translated(translation);
        if (command.use_clip_rect)
            command.clip_rect = command.clip_rect.translated(translation);
        command.line_start += translation;
        command.line_end += translation;
        command.circle_center += translation;
    }
}

void apply_clip_to_range(
    std::vector<elysia::core::UiRenderCommand>& out_commands,
    std::size_t begin,
    const elysia::core::Rect& clip_rect
)
{
    if (clip_rect.is_empty())
    {
        out_commands.erase(out_commands.begin() + static_cast<std::ptrdiff_t>(begin),out_commands.end());
        return;
    }

    for (std::size_t index = out_commands.size(); index > begin; --index)
    {
        elysia::core::UiRenderCommand& command = out_commands[index - 1];
        const elysia::core::Rect final_clip = command.use_clip_rect ? command.clip_rect.intersection(clip_rect) : clip_rect;
        if (final_clip.is_empty())
        {
            out_commands.erase(out_commands.begin() + static_cast<std::ptrdiff_t>(index - 1));
            continue;
        }

        command.use_clip_rect = true;
        command.clip_rect = final_clip;
    }
}

void finalize_child_command_range(
    std::vector<elysia::core::UiRenderCommand>& out_commands,
    std::size_t begin,
    std::uint8_t opacity,
    bool clip_children,
    const elysia::core::Rect& clip_rect
)
{
    if (begin >= out_commands.size())
        return;

    apply_opacity_to_range(out_commands,begin,opacity);
    if (clip_children)
        apply_clip_to_range(out_commands,begin,clip_rect);
}
}
