#pragma once

#include "ui_theme.h"
#include "ui_theme_defaults.h"
#include "ui_visual_styles.h"

namespace elysia::ui
{
// Supplies deterministic construction-time styles before an element is attached to a UiThemeManager.
struct UiStyleDefaults
{
    // Unmanaged elements retain this construction-time snapshot until reset.
    [[nodiscard]] static const UiTheme& theme() noexcept
    {
        return UiThemeDefaults::theme();
    }

    [[nodiscard]] static UiEnabledDisabledColors text() noexcept
    {
        return theme().button(UiButtonVisualRole::Default).text;
    }

    [[nodiscard]] static UiEnabledDisabledColors muted_text() noexcept
    {
        return UiEnabledDisabledColors{
            theme().label(UiLabelVisualRole::Muted).text,
            theme().button(UiButtonVisualRole::Default).text.disabled
        };
    }

    [[nodiscard]] static UiEnabledDisabledColors secondary_text() noexcept
    {
        return theme().text_input_style.text;
    }

    [[nodiscard]] static UiEnabledDisabledColors placeholder_text() noexcept
    {
        return theme().text_input_style.placeholder;
    }

    [[nodiscard]] static UiInteractiveColors interactive_surface() noexcept
    {
        return theme().button(UiButtonVisualRole::Default).chrome.background;
    }

    [[nodiscard]] static UiChromeStyle interactive_chrome() noexcept
    {
        return apply_theme_colors(UiChromeStyle{},theme().button(UiButtonVisualRole::Default).chrome);
    }

    [[nodiscard]] static UiLabelStyle label() noexcept
    {
        return apply_theme_colors(UiLabelStyle{},theme().label(UiLabelVisualRole::Default));
    }

    [[nodiscard]] static UiTextBlockStyle text_block() noexcept
    {
        return apply_theme_colors(UiTextBlockStyle{},theme().label(UiLabelVisualRole::Default));
    }

    [[nodiscard]] static UiDropdownStyle dropdown() noexcept
    {
        return UiDropdownStyle{};
    }

    [[nodiscard]] static UiNumberStyle number() noexcept
    {
        return apply_theme_colors(UiNumberStyle{},theme().number_style);
    }

    [[nodiscard]] static UiBarStyle bar() noexcept
    {
        return apply_theme_colors(UiBarStyle{},theme().bar(UiBarVisualRole::Default));
    }

    [[nodiscard]] static UiPanelStyle panel() noexcept
    {
        return apply_theme_colors(UiPanelStyle{},theme().panel(UiPanelVisualRole::Default));
    }

    [[nodiscard]] static UiChromeContainerStyle chrome_container() noexcept
    {
        return apply_theme_colors(UiChromeContainerStyle{},theme().chrome_container_style);
    }

    [[nodiscard]] static UiWindowStyle window() noexcept
    {
        return apply_theme_colors(UiWindowStyle{},theme().window_style);
    }

    [[nodiscard]] static UiButtonStyle button() noexcept
    {
        return apply_theme_colors(UiButtonStyle{},theme().button(UiButtonVisualRole::Default));
    }

    [[nodiscard]] static UiCheckboxStyle checkbox() noexcept
    {
        return apply_theme_colors(UiCheckboxStyle{},theme().checkbox_style);
    }

    [[nodiscard]] static UiRadioButtonStyle radio_button() noexcept
    {
        return apply_theme_colors(UiRadioButtonStyle{},theme().radio_button_style);
    }

    [[nodiscard]] static UiDragHandleStyle drag_handle() noexcept
    {
        return apply_theme_colors(UiDragHandleStyle{},theme().drag_handle_style);
    }

    [[nodiscard]] static UiSliderStyle slider() noexcept
    {
        return apply_theme_colors(UiSliderStyle{},theme().slider_style);
    }

    [[nodiscard]] static UiTextInputStyle text_input() noexcept
    {
        return apply_theme_colors(UiTextInputStyle{},theme().text_input_style);
    }

    [[nodiscard]] static UiScrollBarStyle scrollbar() noexcept
    {
        return apply_theme_colors(UiScrollBarStyle{},theme().scroll_container_style.scrollbar);
    }

    [[nodiscard]] static UiScrollContainerStyle scroll_container() noexcept
    {
        return apply_theme_colors(UiScrollContainerStyle{},theme().scroll_container_style);
    }

    [[nodiscard]] static UiEnabledDisabledColors labeled_checkbox_text() noexcept
    {
        return UiEnabledDisabledColors{
            theme().label(UiLabelVisualRole::Default).text,
            theme().button(UiButtonVisualRole::Default).text.disabled
        };
    }
};
}
