#pragma once

#include "../../core/render/colors.h"

namespace elysia::ui
{
// Shared default color tokens used by built-in UI styles.
struct UiPalette
{
    static inline constexpr elysia::core::Color text_primary = elysia::core::colors::frosted_white;
    static inline constexpr elysia::core::Color text_secondary = elysia::core::colors::alice_blue;
    static inline constexpr elysia::core::Color text_muted = elysia::core::colors::powder_blue;
    static inline constexpr elysia::core::Color text_disabled = elysia::core::colors::steel_blue;
    static inline constexpr elysia::core::Color text_placeholder = elysia::core::colors::gray_500;

    static inline constexpr elysia::core::Color surface_base = elysia::core::colors::abyss_blue;
    static inline constexpr elysia::core::Color surface_elevated = elysia::core::colors::midnight_blue;
    static inline constexpr elysia::core::Color surface_interactive_idle = elysia::core::colors::cobalt_blue;
    static inline constexpr elysia::core::Color surface_interactive_focused = elysia::core::colors::royal_blue;
    static inline constexpr elysia::core::Color surface_interactive_active = elysia::core::colors::deep_cobalt_blue;
    static inline constexpr elysia::core::Color surface_disabled = elysia::core::colors::gray_700;

    static inline constexpr elysia::core::Color border_default = elysia::core::colors::sky_blue;
    static inline constexpr elysia::core::Color border_focus = elysia::core::colors::alice_blue;
    static inline constexpr elysia::core::Color border_disabled = elysia::core::colors::steel_blue;

    static inline constexpr elysia::core::Color accent = elysia::core::colors::glacial_white;
    static inline constexpr elysia::core::Color accent_soft = elysia::core::colors::powder_blue;
    static inline constexpr elysia::core::Color accent_fill = elysia::core::colors::glacial_white;
    static inline constexpr elysia::core::Color caret = elysia::core::colors::glacial_white;

    static inline constexpr elysia::core::Color scrollbar_track_idle = elysia::core::colors::midnight_blue;
    static inline constexpr elysia::core::Color scrollbar_track_focused = elysia::core::colors::cobalt_blue;
    static inline constexpr elysia::core::Color scrollbar_track_active = elysia::core::colors::royal_blue;
    static inline constexpr elysia::core::Color scrollbar_track_disabled = elysia::core::colors::steel_blue;
    static inline constexpr elysia::core::Color scrollbar_thumb_idle = elysia::core::colors::sky_blue;
    static inline constexpr elysia::core::Color scrollbar_thumb_focused = elysia::core::colors::alice_blue;
    static inline constexpr elysia::core::Color scrollbar_thumb_active = elysia::core::colors::glacial_white;
    static inline constexpr elysia::core::Color scrollbar_thumb_disabled = elysia::core::colors::steel_blue;
};
}

