#include "floating_number_effect.h"

#include "../../core/render/glyph_run_layout.h"
#include "../../core/render/render_command.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace elysia::effects
{
namespace
{
bool is_finite_vector(const elysia::core::Vector2& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

float lerp(float from, float to, float progress) noexcept
{
    return from + (to - from) * progress;
}
}

FloatingNumberEffect::FloatingNumberEffect(
    std::vector<FloatingNumberGlyph> glyphs,
    const elysia::core::Vector2& position,
    FloatingNumberAlignment alignment,
    float target_height,
    double lifetime_seconds,
    FloatingNumberEffects effects,
    Callback on_finished
)
    : elysia::core::GameObject(elysia::core::DepthLayer::EffectFront),
      _glyphs(std::move(glyphs)),
      _origin_position(position),
      _render_position(position),
      _alignment(alignment),
      _target_height(target_height),
      _lifetime_seconds(lifetime_seconds),
      _effects(std::move(effects)),
      _on_finished(std::move(on_finished))
{
    set_position(position);
    update_visual_state();
}

void FloatingNumberEffect::submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const
{
    if (is_destroyed() || _start_delay_remaining_seconds > 0.0 || _alpha == 0 || _scale <= 0.0f)
        return;

    std::vector<elysia::core::Vector2> source_sizes;
    source_sizes.reserve(_glyphs.size());
    for (const FloatingNumberGlyph& glyph : _glyphs)
        source_sizes.push_back(glyph.source_size);

    const elysia::core::GlyphRunLayout layout = elysia::core::layout_glyph_run(
        source_sizes,
        elysia::core::GlyphRunLayoutOptions{ .target_height = _target_height * _scale }
    );

    float origin_x = _render_position.x;
    switch (_alignment)
    {
    case FloatingNumberAlignment::Center:
        origin_x -= layout.width * 0.5f;
        break;
    case FloatingNumberAlignment::Right:
        origin_x -= layout.width;
        break;
    case FloatingNumberAlignment::Left:
    default:
        break;
    }

    for (const elysia::core::GlyphPlacement& placement : layout.glyphs)
    {
        if (placement.glyph_index >= _glyphs.size() || !_glyphs[placement.glyph_index].texture)
            continue;

        elysia::core::RenderCommand command;
        command.texture = _glyphs[placement.glyph_index].texture.get();
        command.command_rect = elysia::core::Rect(
            origin_x + placement.local_rect.x(),
            _render_position.y - placement.local_rect.height() * 0.5f,
            placement.local_rect.width(),
            placement.local_rect.height()
        );
        command.alpha = _alpha;
        out_commands.push_back(command);
    }
}

void FloatingNumberEffect::update(double delta)
{
    if (is_destroyed())
        return;

    double effect_delta = scaled_delta(delta);
    if (!_started)
    {
        if (_start_delay_remaining_seconds > effect_delta)
        {
            _start_delay_remaining_seconds -= effect_delta;
            return;
        }

        effect_delta -= _start_delay_remaining_seconds;
        _start_delay_remaining_seconds = 0.0;
        _started = true;
    }

    _elapsed_seconds = std::min(_lifetime_seconds, _elapsed_seconds + effect_delta);
    update_visual_state();
    if (_elapsed_seconds >= _lifetime_seconds)
        finish();
}

void FloatingNumberEffect::set_start_delay(double delay_seconds) noexcept
{
    if (!_started)
        _start_delay_remaining_seconds = std::max(0.0, delay_seconds);
}

bool FloatingNumberEffect::is_started() const noexcept
{
    return _started;
}

bool FloatingNumberEffect::is_valid_effects(const FloatingNumberEffects& effects) noexcept
{
    if (effects.motion.has_value())
    {
        const bool valid_motion = std::visit([](const auto& motion)
        {
            if (!is_valid_time_range(motion.time_range) || !is_finite_vector(motion.offset))
                return false;

            if constexpr (std::is_same_v<std::decay_t<decltype(motion)>, FloatingNumberArcMotion>)
                return std::isfinite(motion.arc_height) && motion.arc_height >= 0.0f;

            return true;
        }, *effects.motion);
        if (!valid_motion)
            return false;
    }

    if (effects.scale.has_value()
        && (!is_valid_time_range(effects.scale->time_range)
            || !std::isfinite(effects.scale->from_scale)
            || !std::isfinite(effects.scale->to_scale)
            || effects.scale->from_scale < 0.0f
            || effects.scale->to_scale < 0.0f))
    {
        return false;
    }

    return !effects.fade.has_value() || is_valid_time_range(effects.fade->time_range);
}

void FloatingNumberEffect::update_visual_state()
{
    const float progress = static_cast<float>(std::clamp(_elapsed_seconds / _lifetime_seconds, 0.0, 1.0));
    _render_position = _origin_position;
    _scale = 1.0f;
    _alpha = 255;

    if (_effects.motion.has_value())
    {
        std::visit([this, progress](const auto& motion)
        {
            const float motion_progress = range_progress(motion.time_range, progress);
            _render_position += motion.offset * motion_progress;
            if constexpr (std::is_same_v<std::decay_t<decltype(motion)>, FloatingNumberArcMotion>)
                _render_position.y -= 4.0f * motion.arc_height * motion_progress * (1.0f - motion_progress);
        }, *_effects.motion);
    }

    if (_effects.scale.has_value())
    {
        const FloatingNumberScale& scale = *_effects.scale;
        _scale = lerp(scale.from_scale, scale.to_scale, range_progress(scale.time_range, progress));
    }

    if (_effects.fade.has_value())
    {
        const FloatingNumberFade& fade = *_effects.fade;
        const float alpha = lerp(
            static_cast<float>(fade.from_alpha),
            static_cast<float>(fade.to_alpha),
            range_progress(fade.time_range, progress)
        );
        _alpha = static_cast<std::uint8_t>(std::clamp(alpha, 0.0f, 255.0f));
    }

    set_position(_render_position);
}

void FloatingNumberEffect::finish()
{
    if (!_finished_callback_invoked && _on_finished)
    {
        _finished_callback_invoked = true;
        _on_finished(*this);
    }
    destroy();
}

bool FloatingNumberEffect::is_valid_time_range(const FloatingNumberEffectTimeRange& range) noexcept
{
    return std::isfinite(range.start_progress)
        && std::isfinite(range.end_progress)
        && range.start_progress >= 0.0f
        && range.start_progress < range.end_progress
        && range.end_progress <= 1.0f;
}

float FloatingNumberEffect::range_progress(const FloatingNumberEffectTimeRange& range, float progress) noexcept
{
    return std::clamp(
        (progress - range.start_progress) / (range.end_progress - range.start_progress),
        0.0f,
        1.0f
    );
}
}
