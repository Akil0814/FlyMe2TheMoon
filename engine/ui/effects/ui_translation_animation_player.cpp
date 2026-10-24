#include "ui_translation_animation_player.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace elysia::ui
{
void UiTranslationAnimationPlayer::bind(std::string name,UiTranslationAnimation animation)
{
    if (name.empty())
        return;
    _definitions[std::move(name)] = animation;
}

bool UiTranslationAnimationPlayer::remove(std::string_view name)
{
    const auto found = _definitions.find(std::string(name));
    if (found == _definitions.end())
        return false;

    const bool was_active = _has_active_animation && _active_name == found->first;
    _definitions.erase(found);
    if (was_active)
        stop();
    return true;
}

void UiTranslationAnimationPlayer::clear() noexcept
{
    _definitions.clear();
    _active_name.clear();
    _active_animation = {};
    _translation = {};
    _elapsed_seconds = 0.0;
    _is_playing = false;
    _has_active_animation = false;
}

bool UiTranslationAnimationPlayer::play(std::string_view name) noexcept
{
    const auto found = _definitions.find(std::string(name));
    if (found == _definitions.end())
        return false;

    _active_name = found->first;
    _active_animation = found->second;
    _translation = _active_animation.from;
    _elapsed_seconds = 0.0;
    _has_active_animation = true;
    _is_playing = _active_animation.duration_seconds > 0.0;
    if (!_is_playing)
        _translation = _active_animation.to;
    return true;
}

void UiTranslationAnimationPlayer::stop() noexcept
{
    _is_playing = false;
    _has_active_animation = false;
    _active_name.clear();
    _elapsed_seconds = 0.0;
}

bool UiTranslationAnimationPlayer::update(double delta_seconds) noexcept
{
    if (!_is_playing)
        return false;

    const elysia::core::Vector2 previous = _translation;
    _elapsed_seconds += std::max(0.0,delta_seconds);
    const double duration = _active_animation.duration_seconds;
    const double ratio = duration > 0.0 ? std::clamp(_elapsed_seconds / duration,0.0,1.0) : 1.0;
    const double eased = apply_easing(_active_animation.easing,ratio);
    _translation = _active_animation.from
        + (_active_animation.to - _active_animation.from) * static_cast<float>(eased);
    if (ratio >= 1.0)
        _is_playing = false;
    return !previous.nearly_equals(_translation);
}

std::optional<std::string> UiTranslationAnimationPlayer::active_name() const
{
    if (!_has_active_animation)
        return std::nullopt;
    return _active_name;
}

double UiTranslationAnimationPlayer::apply_easing(UiTranslationAnimationEasing easing,double value) noexcept
{
    const double t = std::clamp(value,0.0,1.0);
    if (easing == UiTranslationAnimationEasing::Linear)
        return t;

    constexpr double kPi = 3.14159265358979323846;
    return 0.5 - 0.5 * std::cos(kPi * t);
}
}
