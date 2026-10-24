#include "speech_bubble.h"

#include <cmath>

namespace elysia::gameplay::ui
{
SpeechBubble::SpeechBubble()
{
    _text.set_typography_role(elysia::typography::UiTypographyRole::DialogBody);
    _text.set_max_width(160.0f);
}

void SpeechBubble::set_style(SpeechBubbleStyle style) noexcept
{
    for (float* value : {&style.border_width, &style.padding, &style.tail_width, &style.tail_height})
        if (!std::isfinite(*value) || *value < 0.0f)
            *value = 0.0f;
    _style = style;
}

elysia::core::Rect SpeechBubble::body_rect(elysia::core::Vector2 tip) const
{
    if (!std::isfinite(tip.x) || !std::isfinite(tip.y))
        return {};
    const auto size = _text.content_size();
    if (size.x <= 0.0f || size.y <= 0.0f)
        return {};
    const float width = size.x + 2.0f * _style.padding;
    const float height = size.y + 2.0f * _style.padding;
    return {tip.x - width * 0.5f, tip.y - _style.tail_height - height, width, height};
}

void SpeechBubble::submit_render_commands(std::vector<elysia::core::RenderCommand>& commands,
    elysia::core::Vector2 tip) const
{
    if (!_visible || !_text.is_visible())
        return;
    using namespace elysia::core;
    const auto body = body_rect(tip);
    if (body.is_empty())
        return;
    const float half_tail = std::min(_style.tail_width, body.width()) * 0.5f;
    const Vector2 left{tip.x - half_tail, body.bottom()};
    const Vector2 right{tip.x + half_tail, body.bottom()};
    const bool has_tail = half_tail > 0.0f && _style.tail_height > 0.0f;
    if (_style.background.a != 0)
    {
        commands.push_back(make_world_fill_rect_command(body, _style.background));
        if (has_tail)
            commands.push_back(make_world_fill_triangle_command(left, tip, right, _style.background));
    }
    if (_style.border_width > 0.0f && _style.border.a != 0)
    {
        const auto line = [&](Vector2 start, Vector2 end) {
            if (!(end - start).is_zero())
                commands.push_back(make_world_draw_line_command(start, end, _style.border, _style.border_width));
        };
        line(body.top_left(), body.top_right());
        line(body.top_right(), body.bottom_right());
        line(body.bottom_right(), has_tail ? right : body.bottom_left());
        if (has_tail)
        {
            line(right, tip);
            line(tip, left);
            line(left, body.bottom_left());
        }
        line(body.bottom_left(), body.top_left());
    }
    _text.submit_render_commands(commands, body.position() + Vector2{_style.padding, _style.padding});
}
}
