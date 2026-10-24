#pragma once

#include "ui_interaction_style.h"
#include "../window/ui_overlay.h"

#include <optional>

namespace elysia::ui
{
// Visual settings for text-only label widgets.
struct UiLabelStyle
{
    float corner_radius = 0.0f;
    elysia::core::Color text{};
    elysia::core::Color background{};
    bool draw_background = false;
};

// Visual settings for wrapped long-form text blocks.
struct UiTextBlockStyle
{
    float corner_radius = 0.0f;
    elysia::core::Color text{};
    elysia::core::Color background{};
    bool draw_background = false;
};

// Layout settings for a transient dropdown menu. Child colors come from existing themes.
struct UiDropdownStyle
{
    float popup_gap = 4.0f;
    float option_height = 42.0f;
    float popup_max_height = 240.0f;
};

// Visual settings for numeric glyph widgets.
struct UiNumberStyle
{
    float corner_radius = 0.0f;
    elysia::core::Color text{};
    elysia::core::Color background{};
    bool draw_background = false;
};

// Visual settings for value bars and progress-like fills.
struct UiBarStyle
{
    float corner_radius = 0.0f;
    elysia::core::UiStrokeWidth border_width{};
    elysia::core::Color background{};
    elysia::core::Color fill{};
    elysia::core::Color border{};
    bool draw_border = true;
};

// Visual settings for simple panel containers.
struct UiPanelStyle
{
    float corner_radius = 0.0f;
    elysia::core::UiStrokeWidth border_width{};
    bool draw_background = true;
    bool draw_border = true;
    elysia::core::Color background{};
    elysia::core::Color border{};
};

// Visual settings for containers with a framed body and optional header chrome.
struct UiChromeContainerStyle
{
    float corner_radius = 0.0f;
    elysia::core::UiStrokeWidth border_width{};
    bool draw_background = true;
    bool draw_border = true;
    bool draw_header_background = true;
    elysia::core::Color background{};
    elysia::core::Color border{};
    elysia::core::Color header_background{};
};

// Visual settings for top-level UI windows.
struct UiWindowStyle
{
    float corner_radius = 0.0f;
    elysia::core::UiStrokeWidth border_width{};
    bool draw_background = true;
    bool draw_border = true;
    elysia::core::Color background{};
    elysia::core::Color border{};
};

// Layout defaults for composite reading dialogs.
struct UiDialogStyle
{
    float corner_radius = 0.0f;
    UiOverlayOptions overlay_defaults{};
    float close_button_height = 42.0f;
    float body_footer_spacing = 12.0f;
    int body_padding = 18;
    int text_padding = 10;
};

struct UiLabelStyleOverrides { std::optional<float> corner_radius; std::optional<elysia::core::Color> text; std::optional<elysia::core::Color> background; std::optional<bool> draw_background; };
struct UiTextBlockStyleOverrides { std::optional<float> corner_radius; std::optional<elysia::core::Color> text; std::optional<elysia::core::Color> background; std::optional<bool> draw_background; };
struct UiDropdownStyleOverrides { std::optional<float> popup_gap; std::optional<float> option_height; std::optional<float> popup_max_height; };
struct UiNumberStyleOverrides { std::optional<float> corner_radius; std::optional<elysia::core::Color> text; std::optional<elysia::core::Color> background; std::optional<bool> draw_background; };
struct UiBarStyleOverrides { std::optional<float> corner_radius; std::optional<elysia::core::UiStrokeWidth> border_width; std::optional<elysia::core::Color> background; std::optional<elysia::core::Color> fill; std::optional<elysia::core::Color> border; std::optional<bool> draw_border; };
struct UiPanelStyleOverrides { std::optional<float> corner_radius; std::optional<elysia::core::UiStrokeWidth> border_width; std::optional<bool> draw_background; std::optional<bool> draw_border; std::optional<elysia::core::Color> background; std::optional<elysia::core::Color> border; };
struct UiChromeContainerStyleOverrides { std::optional<float> corner_radius; std::optional<elysia::core::UiStrokeWidth> border_width; std::optional<bool> draw_background; std::optional<bool> draw_border; std::optional<bool> draw_header_background; std::optional<elysia::core::Color> background; std::optional<elysia::core::Color> border; std::optional<elysia::core::Color> header_background; };
struct UiWindowStyleOverrides { std::optional<float> corner_radius; std::optional<elysia::core::UiStrokeWidth> border_width; std::optional<bool> draw_background; std::optional<bool> draw_border; std::optional<elysia::core::Color> background; std::optional<elysia::core::Color> border; };
struct UiOverlayOptionsOverrides { std::optional<bool> open; std::optional<bool> modal; std::optional<bool> close_on_cancel; std::optional<bool> close_on_outside_click; std::optional<UiOverlayPlacement> placement; std::optional<UiOverlayTransition> transition; std::optional<elysia::core::Vector2> fallback_size; std::optional<int> order; };
struct UiDialogStyleOverrides { std::optional<float> corner_radius; UiOverlayOptionsOverrides overlay_defaults{}; std::optional<float> close_button_height; std::optional<float> body_footer_spacing; std::optional<int> body_padding; std::optional<int> text_padding; };

#define ELYSIA_UI_DEFINE_SIMPLE_STYLE_TRAITS(StyleType,OverridesType,EmptyExpr,ApplyBody) \
template<> struct UiStyleOverrideTraits<StyleType> { using Overrides = OverridesType; \
static bool empty(const Overrides& o) noexcept { return (EmptyExpr); } \
static void apply(StyleType& s,const Overrides& o) noexcept { ApplyBody } };

ELYSIA_UI_DEFINE_SIMPLE_STYLE_TRAITS(UiLabelStyle,UiLabelStyleOverrides,!o.corner_radius&&!o.text&&!o.background&&!o.draw_background,
    apply_ui_style_override(s.corner_radius,o.corner_radius); apply_ui_style_override(s.text,o.text); apply_ui_style_override(s.background,o.background); apply_ui_style_override(s.draw_background,o.draw_background);)
ELYSIA_UI_DEFINE_SIMPLE_STYLE_TRAITS(UiTextBlockStyle,UiTextBlockStyleOverrides,!o.corner_radius&&!o.text&&!o.background&&!o.draw_background,
    apply_ui_style_override(s.corner_radius,o.corner_radius); apply_ui_style_override(s.text,o.text); apply_ui_style_override(s.background,o.background); apply_ui_style_override(s.draw_background,o.draw_background);)
ELYSIA_UI_DEFINE_SIMPLE_STYLE_TRAITS(UiDropdownStyle,UiDropdownStyleOverrides,!o.popup_gap&&!o.option_height&&!o.popup_max_height,
    apply_ui_style_override(s.popup_gap,o.popup_gap); apply_ui_style_override(s.option_height,o.option_height); apply_ui_style_override(s.popup_max_height,o.popup_max_height);)
ELYSIA_UI_DEFINE_SIMPLE_STYLE_TRAITS(UiNumberStyle,UiNumberStyleOverrides,!o.corner_radius&&!o.text&&!o.background&&!o.draw_background,
    apply_ui_style_override(s.corner_radius,o.corner_radius); apply_ui_style_override(s.text,o.text); apply_ui_style_override(s.background,o.background); apply_ui_style_override(s.draw_background,o.draw_background);)
ELYSIA_UI_DEFINE_SIMPLE_STYLE_TRAITS(UiBarStyle,UiBarStyleOverrides,!o.corner_radius&&!o.border_width&&!o.background&&!o.fill&&!o.border&&!o.draw_border,
    apply_ui_style_override(s.corner_radius,o.corner_radius); apply_ui_style_override(s.border_width,o.border_width); apply_ui_style_override(s.background,o.background); apply_ui_style_override(s.fill,o.fill); apply_ui_style_override(s.border,o.border); apply_ui_style_override(s.draw_border,o.draw_border);)
ELYSIA_UI_DEFINE_SIMPLE_STYLE_TRAITS(UiPanelStyle,UiPanelStyleOverrides,!o.corner_radius&&!o.border_width&&!o.draw_background&&!o.draw_border&&!o.background&&!o.border,
    apply_ui_style_override(s.corner_radius,o.corner_radius); apply_ui_style_override(s.border_width,o.border_width); apply_ui_style_override(s.draw_background,o.draw_background); apply_ui_style_override(s.draw_border,o.draw_border); apply_ui_style_override(s.background,o.background); apply_ui_style_override(s.border,o.border);)
ELYSIA_UI_DEFINE_SIMPLE_STYLE_TRAITS(UiChromeContainerStyle,UiChromeContainerStyleOverrides,!o.corner_radius&&!o.border_width&&!o.draw_background&&!o.draw_border&&!o.draw_header_background&&!o.background&&!o.border&&!o.header_background,
    apply_ui_style_override(s.corner_radius,o.corner_radius); apply_ui_style_override(s.border_width,o.border_width); apply_ui_style_override(s.draw_background,o.draw_background); apply_ui_style_override(s.draw_border,o.draw_border); apply_ui_style_override(s.draw_header_background,o.draw_header_background); apply_ui_style_override(s.background,o.background); apply_ui_style_override(s.border,o.border); apply_ui_style_override(s.header_background,o.header_background);)
ELYSIA_UI_DEFINE_SIMPLE_STYLE_TRAITS(UiWindowStyle,UiWindowStyleOverrides,!o.corner_radius&&!o.border_width&&!o.draw_background&&!o.draw_border&&!o.background&&!o.border,
    apply_ui_style_override(s.corner_radius,o.corner_radius); apply_ui_style_override(s.border_width,o.border_width); apply_ui_style_override(s.draw_background,o.draw_background); apply_ui_style_override(s.draw_border,o.draw_border); apply_ui_style_override(s.background,o.background); apply_ui_style_override(s.border,o.border);)

template<> struct UiStyleOverrideTraits<UiDialogStyle> { using Overrides=UiDialogStyleOverrides;
static bool empty(const Overrides& o) noexcept { const auto& v=o.overlay_defaults; return !o.corner_radius&&!v.open&&!v.modal&&!v.close_on_cancel&&!v.close_on_outside_click&&!v.placement&&!v.transition&&!v.fallback_size&&!v.order&&!o.close_button_height&&!o.body_footer_spacing&&!o.body_padding&&!o.text_padding; }
static void apply(UiDialogStyle& s,const Overrides& o) noexcept { apply_ui_style_override(s.corner_radius,o.corner_radius); const auto& v=o.overlay_defaults; apply_ui_style_override(s.overlay_defaults.open,v.open); apply_ui_style_override(s.overlay_defaults.modal,v.modal); apply_ui_style_override(s.overlay_defaults.close_on_cancel,v.close_on_cancel); apply_ui_style_override(s.overlay_defaults.close_on_outside_click,v.close_on_outside_click); apply_ui_style_override(s.overlay_defaults.placement,v.placement); apply_ui_style_override(s.overlay_defaults.transition,v.transition); apply_ui_style_override(s.overlay_defaults.fallback_size,v.fallback_size); apply_ui_style_override(s.overlay_defaults.order,v.order); apply_ui_style_override(s.close_button_height,o.close_button_height); apply_ui_style_override(s.body_footer_spacing,o.body_footer_spacing); apply_ui_style_override(s.body_padding,o.body_padding); apply_ui_style_override(s.text_padding,o.text_padding); }};

#undef ELYSIA_UI_DEFINE_SIMPLE_STYLE_TRAITS
}
