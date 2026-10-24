#pragma once

#include "floating_number_glyph_cache.h"
#include "../../core/game_object.h"
#include "../../core/interface/updatable.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace elysia::effects
{
enum class FloatingNumberAlignment
{
    Left,
    Center,
    Right
};

struct FloatingNumberEffectTimeRange
{
    float start_progress = 0.0f;
    float end_progress = 1.0f;
};

struct FloatingNumberLinearMotion
{
    elysia::core::Vector2 offset;
    FloatingNumberEffectTimeRange time_range;
};

struct FloatingNumberArcMotion
{
    elysia::core::Vector2 offset;
    float arc_height = 0.0f;
    FloatingNumberEffectTimeRange time_range;
};

using FloatingNumberMotion = std::variant<FloatingNumberLinearMotion, FloatingNumberArcMotion>;

struct FloatingNumberScale
{
    float from_scale = 1.0f;
    float to_scale = 1.0f;
    FloatingNumberEffectTimeRange time_range;
};

struct FloatingNumberFade
{
    std::uint8_t from_alpha = 255;
    std::uint8_t to_alpha = 0;
    FloatingNumberEffectTimeRange time_range;
};

struct FloatingNumberEffects
{
    std::optional<FloatingNumberMotion> motion;
    std::optional<FloatingNumberScale> scale;
    std::optional<FloatingNumberFade> fade;
};

class FloatingNumberEffect final : public elysia::core::GameObject, public elysia::core::Updatable
{
public:
    using Callback = std::function<void(FloatingNumberEffect&)>;

    FloatingNumberEffect(
        std::vector<FloatingNumberGlyph> glyphs,
        const elysia::core::Vector2& position,
        FloatingNumberAlignment alignment,
        float target_height,
        double lifetime_seconds,
        FloatingNumberEffects effects,
        Callback on_finished
    );

    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const override;
    void update(double delta) override;

    void set_start_delay(double delay_seconds) noexcept;
    [[nodiscard]] bool is_started() const noexcept;
    [[nodiscard]] static bool is_valid_effects(const FloatingNumberEffects& effects) noexcept;

private:
    void update_visual_state();
    void finish();

    [[nodiscard]] static bool is_valid_time_range(const FloatingNumberEffectTimeRange& range) noexcept;
    [[nodiscard]] static float range_progress(const FloatingNumberEffectTimeRange& range, float progress) noexcept;

private:
    std::vector<FloatingNumberGlyph> _glyphs;
    elysia::core::Vector2 _origin_position;
    elysia::core::Vector2 _render_position;
    FloatingNumberAlignment _alignment = FloatingNumberAlignment::Center;
    float _target_height = 20.0f;
    double _lifetime_seconds = 0.6;
    double _start_delay_remaining_seconds = 0.0;
    double _elapsed_seconds = 0.0;
    FloatingNumberEffects _effects;
    Callback _on_finished;
    float _scale = 1.0f;
    std::uint8_t _alpha = 255;
    bool _started = false;
    bool _finished_callback_invoked = false;
};
}
