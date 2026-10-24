#pragma once

#include "../../core/render/color.h"
#include "../../core/render/render_command.h"
#include "../effect_anchor.h"
#include <cstdint>
#include <optional>
#include <string>

namespace elysia::effects
{
enum class ScreenEffectFit { Stretch, Cover, Contain, Natural };
enum class ScreenEffectEnd { Timed, Manual, AnimationFinished };
enum class ScreenEffectLayer { BeforeUi, AfterUi };
enum class ScreenEffectClock { Unscaled, Scene };

struct ScreenEffectHandle
{
    // Opaque runtime identity. Scene exit and content cleanup invalidate it.
    std::uint32_t slot = 0;
    std::uint64_t generation = 0;
    bool operator==(const ScreenEffectHandle&) const noexcept = default;
};

struct ScreenEffectPlayback
{
    double target_opacity = 1.0;
    double fade_in_seconds = 0.0;
    ScreenEffectEnd end = ScreenEffectEnd::Timed;
    double hold_seconds = 0.6; // Starts after fade-in; ignored for Manual.
    double fade_out_seconds = 0.0;
    ScreenEffectLayer layer = ScreenEffectLayer::AfterUi;
    ScreenEffectClock clock = ScreenEffectClock::Unscaled;
};

struct ScreenColorEffectRequest
{
    elysia::core::Color color;
    ScreenEffectPlayback playback;
};

struct ScreenEffectPlacement
{
    EffectAnchor anchor = EffectAnchor::Center;
    elysia::core::Vector2 offset{0, 0};
    elysia::core::Vector2 scale{1, 1};
};

struct ScreenImageEffectRequest
{
    std::string texture_key;
    ScreenEffectFit fit = ScreenEffectFit::Stretch;
    ScreenEffectPlayback playback;
    ScreenEffectPlacement placement;
};

struct ScreenAnimationEffectRequest
{
    std::string effect_key;
    bool loop = false;
    ScreenEffectFit fit = ScreenEffectFit::Contain;
    ScreenEffectPlacement placement;
    std::optional<double> angle_degrees;
    elysia::core::SpriteFlip flip = elysia::core::SpriteFlip::None;
    ScreenEffectPlayback playback{
        .end = ScreenEffectEnd::AnimationFinished,
        .hold_seconds = 0.0
    };
};
}
