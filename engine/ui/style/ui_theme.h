#pragma once

#include "ui_theme_id.h"
#include "ui_visual_roles.h"
#include "ui_visual_styles.h"
#include "../containers/ui_scroll_container.h"
#include "../widgets/ui_button.h"
#include "../widgets/ui_checkbox.h"
#include "../widgets/ui_drag_handle.h"
#include "../widgets/ui_radio_button.h"
#include "../widgets/ui_slider.h"
#include "../widgets/ui_text_input.h"

#include <array>
#include <cstddef>

namespace elysia::ui
{
// Color-only theme payload for chrome with independent interaction colors for background and border.
struct UiChromeThemeColors
{
    UiInteractiveColors background{};
    UiInteractiveColors border{};
};

struct UiLabelThemeColors
{
    elysia::core::Color text{};
    elysia::core::Color background{};
};

struct UiNumberThemeColors
{
    elysia::core::Color text{};
    elysia::core::Color background{};
};

struct UiBarThemeColors
{
    elysia::core::Color background{};
    elysia::core::Color fill{};
    elysia::core::Color border{};
};

struct UiPanelThemeColors
{
    elysia::core::Color background{};
    elysia::core::Color border{};
};

struct UiWindowThemeColors
{
    elysia::core::Color background{};
    elysia::core::Color border{};
};

struct UiChromeContainerThemeColors
{
    elysia::core::Color background{};
    elysia::core::Color border{};
    elysia::core::Color header_background{};
};

struct UiButtonThemeColors
{
    UiChromeThemeColors chrome{};
    UiEnabledDisabledColors text{};
};

// Theme colors for dialog-owned controls that should not depend on generic button roles.
struct UiDialogThemeColors
{
    UiButtonThemeColors action_button{};
};

struct UiCheckboxThemeColors
{
    UiChromeThemeColors chrome{};
    UiEnabledDisabledColors mark{};
};

struct UiRadioButtonThemeColors
{
    UiChromeThemeColors chrome{};
    UiEnabledDisabledColors mark{};
};

struct UiDragHandleThemeColors
{
    UiChromeThemeColors chrome{};
};

struct UiSliderThemeColors
{
    UiChromeThemeColors chrome{};
    UiEnabledDisabledColors fill{};
    UiEnabledDisabledColors text{};
    UiDragHandleThemeColors handle{};
};

struct UiTextInputThemeColors
{
    UiChromeThemeColors chrome{};
    UiEnabledDisabledColors text{};
    UiEnabledDisabledColors placeholder{};
    elysia::core::Color caret{};
};

struct UiScrollBarThemeColors
{
    elysia::core::Color track_idle_color{};
    elysia::core::Color track_focused_color{};
    elysia::core::Color track_dragging_color{};
    elysia::core::Color track_disabled_color{};
    elysia::core::Color thumb_idle_color{};
    elysia::core::Color thumb_focused_color{};
    elysia::core::Color thumb_dragging_color{};
    elysia::core::Color thumb_disabled_color{};
};

struct UiScrollContainerThemeColors
{
    UiScrollBarThemeColors scrollbar{};
    elysia::core::Color background_color{};
    elysia::core::Color border_color{};
};

// Stores one built-in theme as color-only data. Structure, spacing, draw flags, and
// other behavior stay in control defaults or explicit manual style overrides.
struct UiTheme
{
    static constexpr std::size_t panel_role_count = 4;
    static constexpr std::size_t label_role_count = 4;
    static constexpr std::size_t button_role_count = 3;
    static constexpr std::size_t bar_role_count = 2;

    std::array<UiPanelThemeColors,panel_role_count> panel_styles{};
    std::array<UiLabelThemeColors,label_role_count> label_styles{};
    std::array<UiButtonThemeColors,button_role_count> button_styles{};
    std::array<UiBarThemeColors,bar_role_count> bar_styles{};

    UiNumberThemeColors number_style{};
    UiWindowThemeColors window_style{};
    UiChromeContainerThemeColors chrome_container_style{};
    UiDialogThemeColors dialog_style{};
    UiCheckboxThemeColors checkbox_style{};
    UiRadioButtonThemeColors radio_button_style{};
    UiDragHandleThemeColors drag_handle_style{};
    UiSliderThemeColors slider_style{};
    UiTextInputThemeColors text_input_style{};
    UiScrollContainerThemeColors scroll_container_style{};

    [[nodiscard]] const UiPanelThemeColors& panel(UiPanelVisualRole role = UiPanelVisualRole::Default) const noexcept;
    [[nodiscard]] const UiLabelThemeColors& label(UiLabelVisualRole role = UiLabelVisualRole::Default) const noexcept;
    [[nodiscard]] const UiButtonThemeColors& button(UiButtonVisualRole role = UiButtonVisualRole::Default) const noexcept;
    [[nodiscard]] const UiBarThemeColors& bar(UiBarVisualRole role = UiBarVisualRole::Default) const noexcept;
};

[[nodiscard]] inline UiChromeStyle apply_theme_colors(
    UiChromeStyle style,
    const UiChromeThemeColors& colors
) noexcept
{
    style.background = colors.background;
    style.border = colors.border;
    return style;
}

[[nodiscard]] inline UiLabelStyle apply_theme_colors(
    UiLabelStyle style,
    const UiLabelThemeColors& colors
) noexcept
{
    style.text = colors.text;
    style.background = colors.background;
    return style;
}

[[nodiscard]] inline UiTextBlockStyle apply_theme_colors(
    UiTextBlockStyle style,
    const UiLabelThemeColors& colors
) noexcept
{
    style.text = colors.text;
    style.background = colors.background;
    return style;
}

[[nodiscard]] inline UiNumberStyle apply_theme_colors(
    UiNumberStyle style,
    const UiNumberThemeColors& colors
) noexcept
{
    style.text = colors.text;
    style.background = colors.background;
    return style;
}

[[nodiscard]] inline UiBarStyle apply_theme_colors(
    UiBarStyle style,
    const UiBarThemeColors& colors
) noexcept
{
    style.background = colors.background;
    style.fill = colors.fill;
    style.border = colors.border;
    return style;
}

[[nodiscard]] inline UiPanelStyle apply_theme_colors(
    UiPanelStyle style,
    const UiPanelThemeColors& colors
) noexcept
{
    style.background = colors.background;
    style.border = colors.border;
    return style;
}

[[nodiscard]] inline UiWindowStyle apply_theme_colors(
    UiWindowStyle style,
    const UiWindowThemeColors& colors
) noexcept
{
    style.background = colors.background;
    style.border = colors.border;
    return style;
}

[[nodiscard]] inline UiChromeContainerStyle apply_theme_colors(
    UiChromeContainerStyle style,
    const UiChromeContainerThemeColors& colors
) noexcept
{
    style.background = colors.background;
    style.border = colors.border;
    style.header_background = colors.header_background;
    return style;
}

[[nodiscard]] inline UiButtonStyle apply_theme_colors(
    UiButtonStyle style,
    const UiButtonThemeColors& colors
) noexcept
{
    style.chrome = apply_theme_colors(style.chrome,colors.chrome);
    style.text = colors.text;
    return style;
}

[[nodiscard]] inline UiCheckboxStyle apply_theme_colors(
    UiCheckboxStyle style,
    const UiCheckboxThemeColors& colors
) noexcept
{
    style.chrome = apply_theme_colors(style.chrome,colors.chrome);
    style.mark = colors.mark;
    return style;
}

[[nodiscard]] inline UiRadioButtonStyle apply_theme_colors(
    UiRadioButtonStyle style,
    const UiRadioButtonThemeColors& colors
) noexcept
{
    style.chrome = apply_theme_colors(style.chrome,colors.chrome);
    style.mark = colors.mark;
    return style;
}

[[nodiscard]] inline UiDragHandleStyle apply_theme_colors(
    UiDragHandleStyle style,
    const UiDragHandleThemeColors& colors
) noexcept
{
    style.chrome = apply_theme_colors(style.chrome,colors.chrome);
    return style;
}

[[nodiscard]] inline UiSliderStyle apply_theme_colors(
    UiSliderStyle style,
    const UiSliderThemeColors& colors
) noexcept
{
    style.chrome = apply_theme_colors(style.chrome,colors.chrome);
    style.fill = colors.fill;
    style.text = colors.text;
    style.handle = apply_theme_colors(style.handle,colors.handle);
    return style;
}

[[nodiscard]] inline UiTextInputStyle apply_theme_colors(
    UiTextInputStyle style,
    const UiTextInputThemeColors& colors
) noexcept
{
    style.chrome = apply_theme_colors(style.chrome,colors.chrome);
    style.text = colors.text;
    style.placeholder = colors.placeholder;
    style.caret = colors.caret;
    return style;
}

[[nodiscard]] inline UiScrollBarStyle apply_theme_colors(
    UiScrollBarStyle style,
    const UiScrollBarThemeColors& colors
) noexcept
{
    style.track_idle_color = colors.track_idle_color;
    style.track_focused_color = colors.track_focused_color;
    style.track_dragging_color = colors.track_dragging_color;
    style.track_disabled_color = colors.track_disabled_color;
    style.thumb_idle_color = colors.thumb_idle_color;
    style.thumb_focused_color = colors.thumb_focused_color;
    style.thumb_dragging_color = colors.thumb_dragging_color;
    style.thumb_disabled_color = colors.thumb_disabled_color;
    return style;
}

[[nodiscard]] inline UiScrollContainerStyle apply_theme_colors(
    UiScrollContainerStyle style,
    const UiScrollContainerThemeColors& colors
) noexcept
{
    style.scrollbar = apply_theme_colors(style.scrollbar,colors.scrollbar);
    style.background_color = colors.background_color;
    style.border_color = colors.border_color;
    return style;
}

[[nodiscard]] UiTheme make_builtin_theme(UiBuiltinTheme theme) noexcept;
[[nodiscard]] const UiTheme& builtin_theme(UiBuiltinTheme theme) noexcept;
}
