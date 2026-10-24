#pragma once

#include "../../core/render/render_command.h"
#include "ui_palette.h"
#include "ui_style.h"

#include <optional>

namespace elysia::ui
{
// Paired colors used when a widget switches between enabled and disabled state.
struct UiEnabledDisabledColors
{
    elysia::core::Color enabled = UiPalette::text_primary;
    elysia::core::Color disabled = UiPalette::text_disabled;
};

struct UiEnabledDisabledColorsOverrides
{
    std::optional<elysia::core::Color> enabled;
    std::optional<elysia::core::Color> disabled;
};

// Interactive chrome colors keyed by idle, focus, pressed, and disabled state.
struct UiInteractiveColors
{
    elysia::core::Color idle = UiPalette::surface_interactive_idle;
    elysia::core::Color focused = UiPalette::surface_interactive_focused;
    elysia::core::Color active = UiPalette::surface_interactive_active;
    elysia::core::Color disabled = UiPalette::surface_disabled;
};

struct UiInteractiveColorsOverrides
{
    std::optional<elysia::core::Color> idle;
    std::optional<elysia::core::Color> focused;
    std::optional<elysia::core::Color> active;
    std::optional<elysia::core::Color> disabled;
};

// Shared border/background styling used by interactive widgets.
struct UiChromeStyle
{
    float corner_radius = 0.0f;
    elysia::core::UiStrokeWidth border_width{};
    UiInteractiveColors background{};
    UiInteractiveColors border{
        UiPalette::border_default,
        UiPalette::border_focus,
        UiPalette::accent,
        UiPalette::border_disabled
    };
    bool draw_background = true;
    bool draw_border = true;
};

struct UiChromeStyleOverrides
{
    UiInteractiveColorsOverrides background{};
    UiInteractiveColorsOverrides border{};
    std::optional<float> corner_radius;
    std::optional<elysia::core::UiStrokeWidth> border_width;
    std::optional<bool> draw_background;
    std::optional<bool> draw_border;
};

[[nodiscard]] inline bool empty(const UiEnabledDisabledColorsOverrides& o) noexcept
{
    return !o.enabled && !o.disabled;
}

[[nodiscard]] inline bool empty(const UiInteractiveColorsOverrides& o) noexcept
{
    return !o.idle && !o.focused && !o.active && !o.disabled;
}

[[nodiscard]] inline bool empty(const UiChromeStyleOverrides& o) noexcept
{
    return empty(o.background) && empty(o.border) && !o.corner_radius
        && !o.border_width && !o.draw_background && !o.draw_border;
}

inline void apply_ui_style_overrides(UiEnabledDisabledColors& s,const UiEnabledDisabledColorsOverrides& o) noexcept
{
    apply_ui_style_override(s.enabled,o.enabled);
    apply_ui_style_override(s.disabled,o.disabled);
}

inline void apply_ui_style_overrides(UiInteractiveColors& s,const UiInteractiveColorsOverrides& o) noexcept
{
    apply_ui_style_override(s.idle,o.idle);
    apply_ui_style_override(s.focused,o.focused);
    apply_ui_style_override(s.active,o.active);
    apply_ui_style_override(s.disabled,o.disabled);
}

inline void apply_ui_style_overrides(UiChromeStyle& s,const UiChromeStyleOverrides& o) noexcept
{
    apply_ui_style_overrides(s.background,o.background);
    apply_ui_style_overrides(s.border,o.border);
    apply_ui_style_override(s.corner_radius,o.corner_radius);
    apply_ui_style_override(s.border_width,o.border_width);
    apply_ui_style_override(s.draw_background,o.draw_background);
    apply_ui_style_override(s.draw_border,o.draw_border);
}

// Resolves the color that matches the widget's current interaction state.
[[nodiscard]] inline elysia::core::Color resolve_interactive_color(
    const UiInteractiveColors& colors,
    bool enabled,
    bool focused,
    bool active
) noexcept
{
    if (!enabled)
        return colors.disabled;
    if (active)
        return colors.active;
    if (focused)
        return colors.focused;
    return colors.idle;
}

// Resolves the color that matches the widget's enabled state.
[[nodiscard]] inline elysia::core::Color resolve_enabled_disabled_color(
    const UiEnabledDisabledColors& colors,
    bool enabled
) noexcept
{
    return enabled ? colors.enabled : colors.disabled;
}
}
