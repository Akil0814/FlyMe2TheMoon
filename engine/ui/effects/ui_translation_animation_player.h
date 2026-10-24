#pragma once

#include "../../core/geometry/vector2.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace elysia::ui
{
enum class UiTranslationAnimationEasing
{
    Linear,
    EaseInOut
};

// A layout-independent presentation translation animation.
struct UiTranslationAnimation
{
    elysia::core::Vector2 from{};
    elysia::core::Vector2 to{};
    double duration_seconds = 0.0;
    UiTranslationAnimationEasing easing = UiTranslationAnimationEasing::EaseInOut;
};

class UiTranslationAnimationPlayer
{
public:
    void bind(std::string name,UiTranslationAnimation animation);
    bool remove(std::string_view name);
    void clear() noexcept;

    [[nodiscard]] bool play(std::string_view name) noexcept;
    void stop() noexcept;
    // Advances the active animation and returns whether its visible translation changed.
    [[nodiscard]] bool update(double delta_seconds) noexcept;

    [[nodiscard]] const elysia::core::Vector2& translation() const noexcept { return _translation; }
    [[nodiscard]] bool is_playing() const noexcept { return _is_playing; }
    [[nodiscard]] std::optional<std::string> active_name() const;

private:
    [[nodiscard]] static double apply_easing(UiTranslationAnimationEasing easing,double value) noexcept;

private:
    std::unordered_map<std::string,UiTranslationAnimation> _definitions;
    UiTranslationAnimation _active_animation{};
    std::string _active_name;
    elysia::core::Vector2 _translation{};
    double _elapsed_seconds = 0.0;
    bool _is_playing = false;
    bool _has_active_animation = false;
};
}
