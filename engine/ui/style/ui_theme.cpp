#include "ui_theme.h"

#include "ui_palette.h"

#include <utility>

namespace elysia::ui
{
namespace
{
template<class Enum>
[[nodiscard]] constexpr std::size_t to_index(Enum value) noexcept
{
    return static_cast<std::size_t>(value);
}

[[nodiscard]] UiEnabledDisabledColors make_text_colors(
    elysia::core::Color enabled,
    elysia::core::Color disabled = UiPalette::text_disabled
) noexcept
{
    return UiEnabledDisabledColors{ enabled,disabled };
}

[[nodiscard]] UiInteractiveColors make_surface_colors(
    elysia::core::Color idle,
    elysia::core::Color focused,
    elysia::core::Color active,
    elysia::core::Color disabled
) noexcept
{
    return UiInteractiveColors{ idle,focused,active,disabled };
}

[[nodiscard]] UiChromeThemeColors make_chrome(
    const UiInteractiveColors& background,
    const UiInteractiveColors& border
) noexcept
{
    return UiChromeThemeColors{ background,border };
}

[[nodiscard]] UiTheme finalize_theme(
    UiTheme theme,elysia::core::Color page,elysia::core::Color section,
    elysia::core::Color button_idle) noexcept
{
    // Containers describe their purpose; each theme supplies its own surface family.
    theme.window_style.background = page;
    theme.panel_styles[to_index(UiPanelVisualRole::Screen)].background = page;
    theme.panel_styles[to_index(UiPanelVisualRole::Default)].background = section;
    theme.panel_styles[to_index(UiPanelVisualRole::List)].background = section;
    theme.chrome_container_style.background = section;
    theme.chrome_container_style.header_background = page;
    // Scrolling does not introduce a surface over its window, panel, or popup.
    theme.scroll_container_style.background_color = elysia::core::colors::transparent;
    // Button fills remain subtly separate from both page and section surfaces.
    theme.button_styles[to_index(UiButtonVisualRole::Default)].chrome.background.idle = button_idle;
    // Keep the initial action appearance familiar while reserving a distinct semantic slot.
    theme.dialog_style.action_button = theme.button(UiButtonVisualRole::Default);
    return theme;
}

[[nodiscard]] UiScrollBarThemeColors make_scrollbar_colors(
    elysia::core::Color track_idle,
    elysia::core::Color track_focused,
    elysia::core::Color track_dragging,
    elysia::core::Color track_disabled,
    elysia::core::Color thumb_idle,
    elysia::core::Color thumb_focused,
    elysia::core::Color thumb_dragging,
    elysia::core::Color thumb_disabled
) noexcept
{
    return UiScrollBarThemeColors{
        track_idle,
        track_focused,
        track_dragging,
        track_disabled,
        thumb_idle,
        thumb_focused,
        thumb_dragging,
        thumb_disabled
    };
}

[[nodiscard]] UiTheme make_blue_glass_moon_theme() noexcept
{
    UiTheme theme;

    const UiEnabledDisabledColors text = make_text_colors(UiPalette::text_primary);
    const UiEnabledDisabledColors secondary_text = make_text_colors(UiPalette::text_secondary);
    const UiEnabledDisabledColors placeholder_text = make_text_colors(UiPalette::text_placeholder);
    const UiInteractiveColors border = UiInteractiveColors{
        UiPalette::border_default,
        UiPalette::border_focus,
        UiPalette::accent,
        UiPalette::border_disabled
    };
    const UiInteractiveColors interactive_surface = make_surface_colors(
        UiPalette::surface_interactive_idle,
        UiPalette::surface_interactive_focused,
        UiPalette::surface_interactive_active,
        UiPalette::surface_disabled);
    const UiChromeThemeColors interactive_chrome = make_chrome(interactive_surface,border);

    theme.label_styles[to_index(UiLabelVisualRole::Default)] = UiLabelThemeColors{
        UiPalette::text_primary,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Title)] = UiLabelThemeColors{
        elysia::core::colors::glacial_white,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Subtitle)] = UiLabelThemeColors{
        UiPalette::text_secondary,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Muted)] = UiLabelThemeColors{
        UiPalette::text_muted,
        elysia::core::colors::transparent
    };

    theme.number_style = UiNumberThemeColors{
        UiPalette::text_primary,
        elysia::core::colors::transparent
    };

    theme.bar_styles[to_index(UiBarVisualRole::Default)] = UiBarThemeColors{
        UiPalette::surface_elevated,
        elysia::core::colors::powder_blue,
        elysia::core::colors::steel_blue
    };
    theme.bar_styles[to_index(UiBarVisualRole::Progress)] = UiBarThemeColors{
        UiPalette::surface_elevated,
        elysia::core::colors::royal_blue,
        elysia::core::colors::alice_blue
    };

    theme.panel_styles[to_index(UiPanelVisualRole::Default)] = UiPanelThemeColors{
        UiPalette::surface_elevated,
        UiPalette::border_default
    };
    theme.panel_styles[to_index(UiPanelVisualRole::Screen)] = UiPanelThemeColors{
        UiPalette::surface_base,
        UiPalette::border_focus
    };
    theme.panel_styles[to_index(UiPanelVisualRole::Dialog)] = UiPanelThemeColors{
        UiPalette::surface_elevated,
        UiPalette::border_focus
    };
    theme.panel_styles[to_index(UiPanelVisualRole::List)] = UiPanelThemeColors{
        UiPalette::surface_elevated,
        UiPalette::border_default
    };

    theme.window_style = UiWindowThemeColors{
        UiPalette::surface_base,
        UiPalette::border_focus
    };

    theme.chrome_container_style = UiChromeContainerThemeColors{
        UiPalette::surface_elevated,
        UiPalette::border_default,
        UiPalette::surface_base
    };

    theme.button_styles[to_index(UiButtonVisualRole::Default)] = UiButtonThemeColors{
        interactive_chrome,
        text
    };
    theme.button_styles[to_index(UiButtonVisualRole::Primary)] = UiButtonThemeColors{
        make_chrome(
            make_surface_colors(
                elysia::core::colors::royal_blue,
                elysia::core::colors::blue_500,
                elysia::core::colors::deep_cobalt_blue,
                UiPalette::surface_disabled),
            UiInteractiveColors{
                elysia::core::colors::alice_blue,
                elysia::core::colors::frosted_white,
                UiPalette::accent,
                UiPalette::border_disabled }),
        text
    };
    theme.button_styles[to_index(UiButtonVisualRole::Danger)] = UiButtonThemeColors{
        make_chrome(
            make_surface_colors(
                elysia::core::colors::red_700,
                elysia::core::colors::red_500,
                elysia::core::colors::red_300,
                UiPalette::surface_disabled),
            UiInteractiveColors{
                elysia::core::colors::alice_blue,
                elysia::core::colors::white,
                elysia::core::colors::deep_cobalt_blue,
                UiPalette::border_disabled }),
        text
    };

    theme.checkbox_style = UiCheckboxThemeColors{
        interactive_chrome,
        UiEnabledDisabledColors{ UiPalette::accent_fill,UiPalette::text_disabled }
    };

    theme.radio_button_style = UiRadioButtonThemeColors{
        interactive_chrome,
        UiEnabledDisabledColors{ UiPalette::accent_fill,UiPalette::text_disabled }
    };

    theme.drag_handle_style = UiDragHandleThemeColors{
        interactive_chrome
    };

    theme.slider_style = UiSliderThemeColors{
        make_chrome(interactive_surface,border),
        UiEnabledDisabledColors{ UiPalette::accent_fill,UiPalette::text_disabled },
        text,
        theme.drag_handle_style
    };

    theme.text_input_style = UiTextInputThemeColors{
        interactive_chrome,
        secondary_text,
        placeholder_text,
        UiPalette::caret
    };

    theme.scroll_container_style = UiScrollContainerThemeColors{
        make_scrollbar_colors(
            UiPalette::scrollbar_track_idle,
            UiPalette::scrollbar_track_focused,
            UiPalette::scrollbar_track_active,
            UiPalette::scrollbar_track_disabled,
            UiPalette::scrollbar_thumb_idle,
            UiPalette::scrollbar_thumb_focused,
            UiPalette::scrollbar_thumb_active,
            UiPalette::scrollbar_thumb_disabled),
        UiPalette::surface_elevated,
        UiPalette::border_default
    };

    return finalize_theme(std::move(theme),UiPalette::surface_base,UiPalette::surface_elevated,
        elysia::core::colors::blue_glass_button_idle);
}

[[nodiscard]] UiTheme make_elysia_light_theme() noexcept
{
    UiTheme theme;

    const UiEnabledDisabledColors text = make_text_colors(
        elysia::core::colors::elysia_plum,
        elysia::core::colors::gray_500);
    const UiEnabledDisabledColors placeholder_text = make_text_colors(
        elysia::core::colors::elysia_muted_mauve,
        elysia::core::colors::gray_500);
    const UiEnabledDisabledColors secondary_text = make_text_colors(
        elysia::core::colors::elysia_plum,
        elysia::core::colors::gray_500);
    const UiInteractiveColors border = UiInteractiveColors{
        elysia::core::colors::elysia_plum,
        elysia::core::colors::elysia_velvet_rose,
        elysia::core::colors::elysia_twilight_rose,
        elysia::core::colors::gray_500
    };
    const UiInteractiveColors interactive_surface = make_surface_colors(
        elysia::core::colors::elysia_chiffon_pink,
        elysia::core::colors::elysia_petal_pink,
        elysia::core::colors::elysia_opal_lilac,
        elysia::core::colors::gray_300);
    const UiChromeThemeColors interactive_chrome = make_chrome(interactive_surface,border);
    const UiChromeThemeColors default_button_chrome = make_chrome(
        make_surface_colors(
            elysia::core::colors::elysia_light_button_idle,
            elysia::core::colors::elysia_light_button_focus,
            elysia::core::colors::elysia_light_button_active,
            elysia::core::colors::gray_300),
        UiInteractiveColors{
            elysia::core::colors::elysia_plum,
            elysia::core::colors::elysia_deep_sea,
            elysia::core::colors::elysia_twilight_rose,
            elysia::core::colors::gray_500
        });

    theme.label_styles[to_index(UiLabelVisualRole::Default)] = UiLabelThemeColors{
        elysia::core::colors::elysia_plum,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Title)] = UiLabelThemeColors{
        elysia::core::colors::elysia_dusk_rose,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Subtitle)] = UiLabelThemeColors{
        elysia::core::colors::elysia_twilight_rose,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Muted)] = UiLabelThemeColors{
        elysia::core::colors::elysia_muted_mauve,
        elysia::core::colors::transparent
    };

    theme.number_style = UiNumberThemeColors{
        elysia::core::colors::elysia_plum,
        elysia::core::colors::transparent
    };

    theme.bar_styles[to_index(UiBarVisualRole::Default)] = UiBarThemeColors{
        elysia::core::colors::elysia_pearl_white,
        elysia::core::colors::elysia_velvet_rose,
        elysia::core::colors::elysia_plum
    };
    theme.bar_styles[to_index(UiBarVisualRole::Progress)] = UiBarThemeColors{
        elysia::core::colors::elysia_pearl_white,
        elysia::core::colors::elysia_twilight_rose,
        elysia::core::colors::elysia_velvet_rose
    };

    theme.panel_styles[to_index(UiPanelVisualRole::Default)] = UiPanelThemeColors{
        elysia::core::colors::elysia_pearl_white,
        elysia::core::colors::elysia_velvet_rose
    };
    theme.panel_styles[to_index(UiPanelVisualRole::Screen)] = UiPanelThemeColors{
        elysia::core::colors::elysia_chiffon_pink,
        elysia::core::colors::elysia_plum
    };
    theme.panel_styles[to_index(UiPanelVisualRole::Dialog)] = UiPanelThemeColors{
        elysia::core::colors::elysia_silk_white,
        elysia::core::colors::elysia_velvet_rose
    };
    theme.panel_styles[to_index(UiPanelVisualRole::List)] = UiPanelThemeColors{
        elysia::core::colors::elysia_chiffon_pink,
        elysia::core::colors::elysia_plum
    };

    theme.window_style = UiWindowThemeColors{
        elysia::core::colors::elysia_pearl_white,
        elysia::core::colors::elysia_velvet_rose
    };

    theme.chrome_container_style = UiChromeContainerThemeColors{
        elysia::core::colors::elysia_pearl_white,
        elysia::core::colors::elysia_velvet_rose,
        elysia::core::colors::elysia_opal_lilac
    };

    theme.button_styles[to_index(UiButtonVisualRole::Default)] = UiButtonThemeColors{
        default_button_chrome,
        text
    };
    theme.button_styles[to_index(UiButtonVisualRole::Primary)] = UiButtonThemeColors{
        make_chrome(
            make_surface_colors(
                elysia::core::colors::elysia_hair_rose,
                elysia::core::colors::elysia_rose_pink,
                elysia::core::colors::elysia_glow_pink,
                elysia::core::colors::gray_300),
            UiInteractiveColors{
                elysia::core::colors::elysia_twilight_rose,
                elysia::core::colors::elysia_plum,
                elysia::core::colors::elysia_deep_sea,
                elysia::core::colors::gray_500 }),
        make_text_colors(elysia::core::colors::elysia_deep_sea,elysia::core::colors::gray_100)
    };
    theme.button_styles[to_index(UiButtonVisualRole::Danger)] = UiButtonThemeColors{
        make_chrome(
            make_surface_colors(
                elysia::core::colors::elysia_rose_copper,
                elysia::core::colors::elysia_rose_copper_focus,
                elysia::core::colors::elysia_rose_copper_active,
                elysia::core::colors::gray_300),
            UiInteractiveColors{
                elysia::core::colors::elysia_rose_gold,
                elysia::core::colors::elysia_rose_gold_focus,
                elysia::core::colors::elysia_rose_gold_active,
                elysia::core::colors::gray_500 }),
        make_text_colors(elysia::core::colors::elysia_silk_white,elysia::core::colors::gray_100)
    };

    theme.checkbox_style = UiCheckboxThemeColors{
        interactive_chrome,
        UiEnabledDisabledColors{ elysia::core::colors::elysia_twilight_rose,elysia::core::colors::gray_500 }
    };

    theme.radio_button_style = UiRadioButtonThemeColors{
        interactive_chrome,
        UiEnabledDisabledColors{ elysia::core::colors::elysia_twilight_rose,elysia::core::colors::gray_500 }
    };

    theme.drag_handle_style = UiDragHandleThemeColors{
        interactive_chrome
    };

    theme.slider_style = UiSliderThemeColors{
        make_chrome(interactive_surface,border),
        UiEnabledDisabledColors{ elysia::core::colors::elysia_twilight_rose,elysia::core::colors::gray_500 },
        text,
        theme.drag_handle_style
    };

    theme.text_input_style = UiTextInputThemeColors{
        interactive_chrome,
        secondary_text,
        placeholder_text,
        elysia::core::colors::elysia_twilight_rose
    };

    theme.scroll_container_style = UiScrollContainerThemeColors{
        make_scrollbar_colors(
            elysia::core::colors::elysia_pearl_white,
            elysia::core::colors::elysia_chiffon_pink,
            elysia::core::colors::elysia_opal_lilac,
            elysia::core::colors::gray_300,
            elysia::core::colors::elysia_velvet_rose,
            elysia::core::colors::elysia_twilight_rose,
            elysia::core::colors::elysia_plum,
            elysia::core::colors::gray_500),
        elysia::core::colors::elysia_pearl_white,
        elysia::core::colors::elysia_velvet_rose
    };

    theme.button_styles[to_index(UiButtonVisualRole::Default)].text.enabled =
        elysia::core::colors::elysia_twilight_rose;
    return finalize_theme(std::move(theme),
        elysia::core::colors::elysia_chiffon_pink,elysia::core::colors::elysia_pearl_white,
        elysia::core::colors::elysia_light_button_idle);
}

[[nodiscard]] UiTheme make_elysia_dark_theme() noexcept
{
    UiTheme theme;

    const UiEnabledDisabledColors text = make_text_colors(
        elysia::core::colors::elysia_moonlit_lavender,
        elysia::core::colors::elysia_silver_mist);
    const UiEnabledDisabledColors secondary_text = make_text_colors(
        elysia::core::colors::elysia_starlight_lilac,
        elysia::core::colors::elysia_silver_mist);
    const UiEnabledDisabledColors placeholder_text = make_text_colors(
        elysia::core::colors::elysia_mist_blue,
        elysia::core::colors::elysia_silver_mist);
    const UiInteractiveColors border = UiInteractiveColors{
        elysia::core::colors::elysia_starlight_lilac,
        elysia::core::colors::elysia_moonlit_lavender,
        elysia::core::colors::elysia_crystal_orchid,
        elysia::core::colors::elysia_silver_mist
    };
    const UiInteractiveColors interactive_surface = make_surface_colors(
        elysia::core::colors::elysia_twilight_mist,
        elysia::core::colors::elysia_phantom_sea,
        elysia::core::colors::elysia_deep_sea,
        elysia::core::colors::elysia_deep_sea);
    const UiChromeThemeColors interactive_chrome = make_chrome(interactive_surface,border);
    const UiChromeThemeColors default_button_chrome = make_chrome(
        make_surface_colors(
            elysia::core::colors::elysia_dark_button_idle,
            elysia::core::colors::elysia_dark_button_focus,
            elysia::core::colors::elysia_dark_button_active,
            elysia::core::colors::elysia_deep_sea),
        UiInteractiveColors{
            elysia::core::colors::elysia_starlight_lilac,
            elysia::core::colors::elysia_crystal_orchid,
            elysia::core::colors::elysia_moonlit_lavender,
            elysia::core::colors::elysia_silver_mist
        });

    theme.label_styles[to_index(UiLabelVisualRole::Default)] = UiLabelThemeColors{
        elysia::core::colors::elysia_moonlit_lavender,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Title)] = UiLabelThemeColors{
        elysia::core::colors::elysia_crystal_orchid,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Subtitle)] = UiLabelThemeColors{
        elysia::core::colors::elysia_starlight_lilac,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Muted)] = UiLabelThemeColors{
        elysia::core::colors::elysia_mist_blue,
        elysia::core::colors::transparent
    };

    theme.number_style = UiNumberThemeColors{
        elysia::core::colors::elysia_moonlit_lavender,
        elysia::core::colors::transparent
    };

    theme.bar_styles[to_index(UiBarVisualRole::Default)] = UiBarThemeColors{
        elysia::core::colors::elysia_twilight_mist,
        elysia::core::colors::elysia_glow_pink,
        elysia::core::colors::elysia_starlight_lilac
    };
    theme.bar_styles[to_index(UiBarVisualRole::Progress)] = UiBarThemeColors{
        elysia::core::colors::elysia_twilight_mist,
        elysia::core::colors::elysia_glow_pink,
        elysia::core::colors::elysia_dream_rose
    };

    theme.panel_styles[to_index(UiPanelVisualRole::Default)] = UiPanelThemeColors{
        elysia::core::colors::elysia_twilight_mist,
        elysia::core::colors::elysia_starlight_lilac
    };
    theme.panel_styles[to_index(UiPanelVisualRole::Screen)] = UiPanelThemeColors{
        elysia::core::colors::elysia_starsea_navy,
        elysia::core::colors::elysia_aurora_haze
    };
    theme.panel_styles[to_index(UiPanelVisualRole::Dialog)] = UiPanelThemeColors{
        elysia::core::colors::elysia_twilight_mist,
        elysia::core::colors::elysia_crystal_orchid
    };
    theme.panel_styles[to_index(UiPanelVisualRole::List)] = UiPanelThemeColors{
        elysia::core::colors::elysia_twilight_mist,
        elysia::core::colors::elysia_dream_rose
    };

    theme.window_style = UiWindowThemeColors{
        elysia::core::colors::elysia_starsea_navy,
        elysia::core::colors::elysia_starlight_lilac
    };

    theme.chrome_container_style = UiChromeContainerThemeColors{
        elysia::core::colors::elysia_twilight_mist,
        elysia::core::colors::elysia_starlight_lilac,
        elysia::core::colors::elysia_phantom_sea
    };

    theme.button_styles[to_index(UiButtonVisualRole::Default)] = UiButtonThemeColors{
        default_button_chrome,
        text
    };
    theme.button_styles[to_index(UiButtonVisualRole::Primary)] = UiButtonThemeColors{
        make_chrome(
            make_surface_colors(
                elysia::core::colors::elysia_lilac,
                elysia::core::colors::elysia_hair_rose,
                elysia::core::colors::elysia_glow_pink,
                elysia::core::colors::elysia_phantom_sea),
            UiInteractiveColors{
                elysia::core::colors::elysia_twilight_mist,
                elysia::core::colors::elysia_twilight_rose,
                elysia::core::colors::elysia_phantom_sea,
                elysia::core::colors::elysia_silver_mist
            }),
        make_text_colors(
            elysia::core::colors::elysia_deep_sea,
            elysia::core::colors::elysia_silver_mist)
    };
    theme.button_styles[to_index(UiButtonVisualRole::Danger)] = UiButtonThemeColors{
        make_chrome(
            make_surface_colors(
                elysia::core::colors::elysia_danger_crimson,
                elysia::core::colors::elysia_danger_crimson_focus,
                elysia::core::colors::elysia_danger_crimson_active,
                elysia::core::colors::elysia_deep_sea),
            UiInteractiveColors{
                elysia::core::colors::elysia_hair_rose,
                elysia::core::colors::elysia_glow_pink,
                elysia::core::colors::elysia_moonlit_lavender,
                elysia::core::colors::elysia_silver_mist
            }),
        text
    };

    theme.checkbox_style = UiCheckboxThemeColors{
        interactive_chrome,
        UiEnabledDisabledColors{
            elysia::core::colors::elysia_crystal_orchid,
            elysia::core::colors::elysia_silver_mist
        }
    };

    theme.radio_button_style = UiRadioButtonThemeColors{
        interactive_chrome,
        UiEnabledDisabledColors{
            elysia::core::colors::elysia_crystal_orchid,
            elysia::core::colors::elysia_silver_mist
        }
    };

    theme.drag_handle_style = UiDragHandleThemeColors{
        interactive_chrome
    };

    theme.slider_style = UiSliderThemeColors{
        make_chrome(interactive_surface,border),
        UiEnabledDisabledColors{
            elysia::core::colors::elysia_crystal_orchid,
            elysia::core::colors::elysia_silver_mist
        },
        text,
        theme.drag_handle_style
    };

    theme.text_input_style = UiTextInputThemeColors{
        interactive_chrome,
        secondary_text,
        placeholder_text,
        elysia::core::colors::elysia_crystal_orchid
    };

    theme.scroll_container_style = UiScrollContainerThemeColors{
        make_scrollbar_colors(
            elysia::core::colors::elysia_starsea_navy,
            elysia::core::colors::elysia_twilight_mist,
            elysia::core::colors::elysia_aurora_haze,
            elysia::core::colors::elysia_phantom_sea,
            elysia::core::colors::elysia_dream_rose,
            elysia::core::colors::elysia_crystal_orchid,
            elysia::core::colors::elysia_moonlit_lavender,
            elysia::core::colors::elysia_silver_mist),
        elysia::core::colors::elysia_twilight_mist,
        elysia::core::colors::elysia_starlight_lilac
    };

    return finalize_theme(std::move(theme),
        elysia::core::colors::elysia_starsea_navy,elysia::core::colors::elysia_twilight_mist,
        elysia::core::colors::elysia_dark_button_idle);
}

[[nodiscard]] UiTheme make_evangelion_unit00_theme() noexcept
{
    UiTheme theme;

    const UiEnabledDisabledColors text = make_text_colors(
        elysia::core::colors::eva_unit00_deep_graphite,
        elysia::core::colors::gray_500);
    const UiEnabledDisabledColors secondary_text = make_text_colors(
        elysia::core::colors::eva_unit00_muted_ink,
        elysia::core::colors::gray_500);
    const UiEnabledDisabledColors placeholder_text = make_text_colors(
        elysia::core::colors::eva_unit00_muted_ink,
        elysia::core::colors::gray_300);
    const UiInteractiveColors border = UiInteractiveColors{
        elysia::core::colors::eva_unit00_soft_graphite,
        elysia::core::colors::eva_unit00_deep_graphite,
        elysia::core::colors::gray_900,
        elysia::core::colors::gray_500
    };
    const UiInteractiveColors interactive_surface = make_surface_colors(
        elysia::core::colors::eva_unit00_rei_white,
        elysia::core::colors::eva_unit00_focus_blue,
        elysia::core::colors::eva_unit00_pale_blue,
        elysia::core::colors::gray_300);
    const UiChromeThemeColors interactive_chrome = make_chrome(interactive_surface,border);

    theme.label_styles[to_index(UiLabelVisualRole::Default)] = UiLabelThemeColors{
        elysia::core::colors::eva_unit00_deep_graphite,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Title)] = UiLabelThemeColors{
        elysia::core::colors::eva_unit00_signal_orange,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Subtitle)] = UiLabelThemeColors{
        elysia::core::colors::eva_unit00_muted_ink,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Muted)] = UiLabelThemeColors{
        elysia::core::colors::eva_unit00_muted_ink,
        elysia::core::colors::transparent
    };

    theme.number_style = UiNumberThemeColors{
        elysia::core::colors::eva_unit00_deep_graphite,
        elysia::core::colors::transparent
    };

    theme.bar_styles[to_index(UiBarVisualRole::Default)] = UiBarThemeColors{
        elysia::core::colors::eva_unit00_rei_white,
        elysia::core::colors::eva_unit00_soft_graphite,
        elysia::core::colors::eva_unit00_deep_graphite
    };
    theme.bar_styles[to_index(UiBarVisualRole::Progress)] = UiBarThemeColors{
        elysia::core::colors::eva_unit00_rei_white,
        elysia::core::colors::eva_unit00_signal_orange,
        elysia::core::colors::eva_unit00_deep_graphite
    };

    theme.panel_styles[to_index(UiPanelVisualRole::Default)] = UiPanelThemeColors{
        elysia::core::colors::eva_unit00_rei_white,
        elysia::core::colors::eva_unit00_soft_graphite
    };
    theme.panel_styles[to_index(UiPanelVisualRole::Screen)] = UiPanelThemeColors{
        elysia::core::colors::eva_unit00_frost_blue,
        elysia::core::colors::eva_unit00_soft_graphite
    };
    theme.panel_styles[to_index(UiPanelVisualRole::Dialog)] = UiPanelThemeColors{
        elysia::core::colors::eva_unit00_rei_white,
        elysia::core::colors::eva_unit00_soft_graphite
    };
    theme.panel_styles[to_index(UiPanelVisualRole::List)] = UiPanelThemeColors{
        elysia::core::colors::eva_unit00_frost_blue,
        elysia::core::colors::eva_unit00_soft_graphite
    };

    theme.window_style = UiWindowThemeColors{
        elysia::core::colors::eva_unit00_rei_white,
        elysia::core::colors::eva_unit00_soft_graphite
    };

    theme.chrome_container_style = UiChromeContainerThemeColors{
        elysia::core::colors::eva_unit00_rei_white,
        elysia::core::colors::eva_unit00_soft_graphite,
        elysia::core::colors::eva_unit00_frost_blue
    };

    theme.button_styles[to_index(UiButtonVisualRole::Default)] = UiButtonThemeColors{
        interactive_chrome,
        text
    };
    theme.button_styles[to_index(UiButtonVisualRole::Primary)] = UiButtonThemeColors{
        make_chrome(
            make_surface_colors(
                elysia::core::colors::eva_unit00_primary_idle,
                elysia::core::colors::alice_blue,
                elysia::core::colors::eva_unit00_pale_blue,
                elysia::core::colors::gray_300),
            UiInteractiveColors{
                elysia::core::colors::eva_unit00_soft_graphite,
                elysia::core::colors::eva_unit00_deep_graphite,
                elysia::core::colors::gray_900,
                elysia::core::colors::gray_500 }),
        make_text_colors(elysia::core::colors::eva_unit00_deep_graphite,elysia::core::colors::gray_700)
    };
    theme.button_styles[to_index(UiButtonVisualRole::Danger)] = UiButtonThemeColors{
        make_chrome(
            make_surface_colors(
                elysia::core::colors::eva_unit00_alert_crimson,
                elysia::core::colors::eva_unit00_alert_crimson_focus,
                elysia::core::colors::eva_unit00_alert_crimson_active,
                elysia::core::colors::gray_300),
            UiInteractiveColors{
                elysia::core::colors::eva_unit00_pale_blue,
                elysia::core::colors::eva_unit00_rei_white,
                elysia::core::colors::eva_unit00_frost_blue,
                elysia::core::colors::gray_500 }),
        make_text_colors(elysia::core::colors::eva_unit00_rei_white,elysia::core::colors::gray_700)
    };

    theme.checkbox_style = UiCheckboxThemeColors{
        interactive_chrome,
        UiEnabledDisabledColors{ elysia::core::colors::eva_unit00_signal_orange,elysia::core::colors::gray_500 }
    };

    theme.radio_button_style = UiRadioButtonThemeColors{
        interactive_chrome,
        UiEnabledDisabledColors{ elysia::core::colors::eva_unit00_signal_orange,elysia::core::colors::gray_500 }
    };

    theme.drag_handle_style = UiDragHandleThemeColors{
        interactive_chrome
    };

    theme.slider_style = UiSliderThemeColors{
        make_chrome(interactive_surface,border),
        UiEnabledDisabledColors{ elysia::core::colors::eva_unit00_signal_orange,elysia::core::colors::gray_500 },
        text,
        theme.drag_handle_style
    };

    theme.text_input_style = UiTextInputThemeColors{
        interactive_chrome,
        secondary_text,
        placeholder_text,
        elysia::core::colors::eva_unit00_signal_orange
    };

    theme.scroll_container_style = UiScrollContainerThemeColors{
        make_scrollbar_colors(
            elysia::core::colors::eva_unit00_rei_white,
            elysia::core::colors::eva_unit00_frost_blue,
            elysia::core::colors::eva_unit00_pale_blue,
            elysia::core::colors::gray_300,
            elysia::core::colors::eva_unit00_soft_graphite,
            elysia::core::colors::eva_unit00_signal_orange,
            elysia::core::colors::eva_unit00_deep_graphite,
            elysia::core::colors::gray_500),
        elysia::core::colors::eva_unit00_rei_white,
        elysia::core::colors::eva_unit00_soft_graphite
    };

    theme.button_styles[to_index(UiButtonVisualRole::Default)].chrome.background.focused =
        elysia::core::colors::eva_unit00_focus_blue;
    return finalize_theme(std::move(theme),
        elysia::core::colors::eva_unit00_frost_blue,elysia::core::colors::eva_unit00_rei_white,
        elysia::core::colors::eva_unit00_button_idle);
}

[[nodiscard]] UiTheme make_evangelion_unit01_theme() noexcept
{
    UiTheme theme;

    const UiEnabledDisabledColors text = make_text_colors(
        elysia::core::colors::frosted_white,
        elysia::core::colors::gray_500);
    const UiEnabledDisabledColors secondary_text = make_text_colors(
        elysia::core::colors::eva_unit01_lime_glow,
        elysia::core::colors::gray_500);
    const UiEnabledDisabledColors placeholder_text = make_text_colors(
        elysia::core::colors::eva_unit01_mist_lime,
        elysia::core::colors::gray_500);
    const UiInteractiveColors border = UiInteractiveColors{
        elysia::core::colors::eva_unit01_toxic_green,
        elysia::core::colors::eva_unit01_lime_glow,
        elysia::core::colors::eva_unit01_orange_core,
        elysia::core::colors::gray_500
    };
    const UiInteractiveColors interactive_surface = make_surface_colors(
        elysia::core::colors::eva_unit01_deep_purple,
        elysia::core::colors::eva_unit01_royal_purple,
        elysia::core::colors::eva_unit01_void_purple,
        elysia::core::colors::gray_700);
    const UiChromeThemeColors interactive_chrome = make_chrome(interactive_surface,border);

    theme.label_styles[to_index(UiLabelVisualRole::Default)] = UiLabelThemeColors{
        elysia::core::colors::frosted_white,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Title)] = UiLabelThemeColors{
        elysia::core::colors::eva_unit01_toxic_green,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Subtitle)] = UiLabelThemeColors{
        elysia::core::colors::eva_unit01_lime_glow,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Muted)] = UiLabelThemeColors{
        elysia::core::colors::eva_unit01_mist_lime,
        elysia::core::colors::transparent
    };

    theme.number_style = UiNumberThemeColors{
        elysia::core::colors::frosted_white,
        elysia::core::colors::transparent
    };

    theme.bar_styles[to_index(UiBarVisualRole::Default)] = UiBarThemeColors{
        elysia::core::colors::eva_unit01_deep_purple,
        elysia::core::colors::eva_unit01_muted_lime,
        elysia::core::colors::eva_unit01_toxic_green
    };
    theme.bar_styles[to_index(UiBarVisualRole::Progress)] = UiBarThemeColors{
        elysia::core::colors::eva_unit01_deep_purple,
        elysia::core::colors::eva_unit01_toxic_green,
        elysia::core::colors::eva_unit01_lime_glow
    };

    theme.panel_styles[to_index(UiPanelVisualRole::Default)] = UiPanelThemeColors{
        elysia::core::colors::eva_unit01_deep_purple,
        elysia::core::colors::eva_unit01_toxic_green
    };
    theme.panel_styles[to_index(UiPanelVisualRole::Screen)] = UiPanelThemeColors{
        elysia::core::colors::eva_unit01_void_purple,
        elysia::core::colors::eva_unit01_toxic_green
    };
    theme.panel_styles[to_index(UiPanelVisualRole::Dialog)] = UiPanelThemeColors{
        elysia::core::colors::eva_unit01_deep_purple,
        elysia::core::colors::eva_unit01_orange_core
    };
    theme.panel_styles[to_index(UiPanelVisualRole::List)] = UiPanelThemeColors{
        elysia::core::colors::eva_unit01_royal_purple,
        elysia::core::colors::eva_unit01_toxic_green
    };

    theme.window_style = UiWindowThemeColors{
        elysia::core::colors::eva_unit01_void_purple,
        elysia::core::colors::eva_unit01_toxic_green
    };

    theme.chrome_container_style = UiChromeContainerThemeColors{
        elysia::core::colors::eva_unit01_deep_purple,
        elysia::core::colors::eva_unit01_toxic_green,
        elysia::core::colors::eva_unit01_royal_purple
    };

    theme.button_styles[to_index(UiButtonVisualRole::Default)] = UiButtonThemeColors{
        interactive_chrome,
        text
    };
    theme.button_styles[to_index(UiButtonVisualRole::Primary)] = UiButtonThemeColors{
        make_chrome(
            make_surface_colors(
                elysia::core::colors::eva_unit01_muted_lime,
                elysia::core::colors::eva_unit01_toxic_green,
                elysia::core::colors::eva_unit01_lime_glow,
                elysia::core::colors::gray_700),
            UiInteractiveColors{
                elysia::core::colors::eva_unit01_deep_purple,
                elysia::core::colors::eva_unit01_void_purple,
                elysia::core::colors::eva_unit01_royal_purple,
                elysia::core::colors::gray_500 }),
        make_text_colors(elysia::core::colors::eva_unit01_deep_purple,elysia::core::colors::gray_700)
    };
    theme.button_styles[to_index(UiButtonVisualRole::Danger)] = UiButtonThemeColors{
        make_chrome(
            make_surface_colors(
                elysia::core::colors::eva_unit01_burnt_orange,
                elysia::core::colors::eva_unit01_orange_core,
                elysia::core::colors::eva_unit01_alert_amber,
                elysia::core::colors::gray_700),
            UiInteractiveColors{
                elysia::core::colors::eva_unit01_void_purple,
                elysia::core::colors::eva_unit01_deep_purple,
                elysia::core::colors::eva_unit01_royal_purple,
                elysia::core::colors::gray_500 }),
        make_text_colors(elysia::core::colors::eva_unit01_deep_purple,elysia::core::colors::gray_700)
    };

    theme.checkbox_style = UiCheckboxThemeColors{
        interactive_chrome,
        UiEnabledDisabledColors{ elysia::core::colors::eva_unit01_toxic_green,elysia::core::colors::gray_500 }
    };

    theme.radio_button_style = UiRadioButtonThemeColors{
        interactive_chrome,
        UiEnabledDisabledColors{ elysia::core::colors::eva_unit01_toxic_green,elysia::core::colors::gray_500 }
    };

    theme.drag_handle_style = UiDragHandleThemeColors{
        interactive_chrome
    };

    theme.slider_style = UiSliderThemeColors{
        make_chrome(interactive_surface,border),
        UiEnabledDisabledColors{ elysia::core::colors::eva_unit01_toxic_green,elysia::core::colors::gray_500 },
        text,
        theme.drag_handle_style
    };

    theme.text_input_style = UiTextInputThemeColors{
        interactive_chrome,
        secondary_text,
        placeholder_text,
        elysia::core::colors::eva_unit01_toxic_green
    };

    theme.scroll_container_style = UiScrollContainerThemeColors{
        make_scrollbar_colors(
            elysia::core::colors::eva_unit01_deep_purple,
            elysia::core::colors::eva_unit01_royal_purple,
            elysia::core::colors::eva_unit01_void_purple,
            elysia::core::colors::gray_700,
            elysia::core::colors::eva_unit01_toxic_green,
            elysia::core::colors::eva_unit01_lime_glow,
            elysia::core::colors::eva_unit01_orange_core,
            elysia::core::colors::gray_500),
        elysia::core::colors::eva_unit01_deep_purple,
        elysia::core::colors::eva_unit01_toxic_green
    };

    return finalize_theme(std::move(theme),
        elysia::core::colors::eva_unit01_void_purple,elysia::core::colors::eva_unit01_deep_purple,
        elysia::core::colors::eva_unit01_button_idle);
}

[[nodiscard]] UiTheme make_evangelion_unit02_theme() noexcept
{
    UiTheme theme;

    const UiEnabledDisabledColors text = make_text_colors(
        elysia::core::colors::eva_unit02_bone_white,
        elysia::core::colors::gray_500);
    const UiEnabledDisabledColors secondary_text = make_text_colors(
        elysia::core::colors::eva_unit02_bone_white,
        elysia::core::colors::gray_500);
    const UiEnabledDisabledColors placeholder_text = make_text_colors(
        elysia::core::colors::eva_unit02_warm_mist,
        elysia::core::colors::gray_500);
    const UiInteractiveColors border = UiInteractiveColors{
        elysia::core::colors::eva_unit02_orange,
        elysia::core::colors::eva_unit02_bone_white,
        elysia::core::colors::eva_unit02_sun_yellow,
        elysia::core::colors::gray_500
    };
    const UiInteractiveColors interactive_surface = make_surface_colors(
        elysia::core::colors::eva_unit02_deep_maroon,
        elysia::core::colors::eva_unit02_crimson,
        elysia::core::colors::eva_unit02_pressed_maroon,
        elysia::core::colors::gray_700);
    const UiChromeThemeColors interactive_chrome = make_chrome(interactive_surface,border);

    theme.label_styles[to_index(UiLabelVisualRole::Default)] = UiLabelThemeColors{
        elysia::core::colors::eva_unit02_bone_white,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Title)] = UiLabelThemeColors{
        elysia::core::colors::eva_unit02_sun_yellow,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Subtitle)] = UiLabelThemeColors{
        elysia::core::colors::eva_unit02_glow_amber,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Muted)] = UiLabelThemeColors{
        elysia::core::colors::eva_unit02_warm_mist,
        elysia::core::colors::transparent
    };

    theme.number_style = UiNumberThemeColors{
        elysia::core::colors::eva_unit02_bone_white,
        elysia::core::colors::transparent
    };

    theme.bar_styles[to_index(UiBarVisualRole::Default)] = UiBarThemeColors{
        elysia::core::colors::eva_unit02_deep_maroon,
        elysia::core::colors::eva_unit02_orange,
        elysia::core::colors::eva_unit02_sun_yellow
    };
    theme.bar_styles[to_index(UiBarVisualRole::Progress)] = UiBarThemeColors{
        elysia::core::colors::eva_unit02_deep_maroon,
        elysia::core::colors::eva_unit02_sun_yellow,
        elysia::core::colors::eva_unit02_glow_amber
    };

    theme.panel_styles[to_index(UiPanelVisualRole::Default)] = UiPanelThemeColors{
        elysia::core::colors::eva_unit02_crimson,
        elysia::core::colors::eva_unit02_sun_yellow
    };
    theme.panel_styles[to_index(UiPanelVisualRole::Screen)] = UiPanelThemeColors{
        elysia::core::colors::eva_unit02_deep_maroon,
        elysia::core::colors::eva_unit02_orange
    };
    theme.panel_styles[to_index(UiPanelVisualRole::Dialog)] = UiPanelThemeColors{
        elysia::core::colors::eva_unit02_crimson,
        elysia::core::colors::eva_unit02_glow_amber
    };
    theme.panel_styles[to_index(UiPanelVisualRole::List)] = UiPanelThemeColors{
        elysia::core::colors::eva_unit02_crimson,
        elysia::core::colors::eva_unit02_sun_yellow
    };

    theme.window_style = UiWindowThemeColors{
        elysia::core::colors::eva_unit02_deep_maroon,
        elysia::core::colors::eva_unit02_orange
    };

    theme.chrome_container_style = UiChromeContainerThemeColors{
        elysia::core::colors::eva_unit02_crimson,
        elysia::core::colors::eva_unit02_sun_yellow,
        elysia::core::colors::eva_unit02_deep_maroon
    };

    theme.button_styles[to_index(UiButtonVisualRole::Default)] = UiButtonThemeColors{
        interactive_chrome,
        text
    };
    theme.button_styles[to_index(UiButtonVisualRole::Primary)] = UiButtonThemeColors{
        make_chrome(
            make_surface_colors(
                elysia::core::colors::eva_unit02_orange,
                elysia::core::colors::eva_unit02_sun_yellow,
                elysia::core::colors::eva_unit02_glow_amber,
                elysia::core::colors::gray_700),
            UiInteractiveColors{
                elysia::core::colors::eva_unit02_deep_maroon,
                elysia::core::colors::eva_unit02_crimson,
                elysia::core::colors::eva_unit02_pressed_maroon,
                elysia::core::colors::gray_500 }),
        make_text_colors(elysia::core::colors::eva_unit02_deep_maroon,elysia::core::colors::gray_700)
    };
    theme.button_styles[to_index(UiButtonVisualRole::Danger)] = UiButtonThemeColors{
        make_chrome(
            make_surface_colors(
                elysia::core::colors::eva_unit02_danger_black_red,
                elysia::core::colors::eva_unit02_danger_black_red_focus,
                elysia::core::colors::eva_unit02_danger_black_red_active,
                elysia::core::colors::gray_700),
            UiInteractiveColors{
                elysia::core::colors::eva_unit02_glow_amber,
                elysia::core::colors::eva_unit02_bone_white,
                elysia::core::colors::eva_unit02_sun_yellow,
                elysia::core::colors::gray_500 }),
        text
    };

    theme.checkbox_style = UiCheckboxThemeColors{
        interactive_chrome,
        UiEnabledDisabledColors{ elysia::core::colors::eva_unit02_sun_yellow,elysia::core::colors::gray_500 }
    };

    theme.radio_button_style = UiRadioButtonThemeColors{
        interactive_chrome,
        UiEnabledDisabledColors{ elysia::core::colors::eva_unit02_sun_yellow,elysia::core::colors::gray_500 }
    };

    theme.drag_handle_style = UiDragHandleThemeColors{
        interactive_chrome
    };

    theme.slider_style = UiSliderThemeColors{
        make_chrome(interactive_surface,border),
        UiEnabledDisabledColors{ elysia::core::colors::eva_unit02_sun_yellow,elysia::core::colors::gray_500 },
        text,
        theme.drag_handle_style
    };

    theme.text_input_style = UiTextInputThemeColors{
        interactive_chrome,
        secondary_text,
        placeholder_text,
        elysia::core::colors::eva_unit02_sun_yellow
    };

    theme.scroll_container_style = UiScrollContainerThemeColors{
        make_scrollbar_colors(
            elysia::core::colors::eva_unit02_deep_maroon,
            elysia::core::colors::eva_unit02_crimson,
            elysia::core::colors::eva_unit02_pressed_maroon,
            elysia::core::colors::gray_700,
            elysia::core::colors::eva_unit02_orange,
            elysia::core::colors::eva_unit02_sun_yellow,
            elysia::core::colors::eva_unit02_glow_amber,
            elysia::core::colors::gray_500),
        elysia::core::colors::eva_unit02_crimson,
        elysia::core::colors::eva_unit02_sun_yellow
    };

    return finalize_theme(std::move(theme),
        elysia::core::colors::eva_unit02_pressed_maroon,elysia::core::colors::eva_unit02_deep_maroon,
        elysia::core::colors::eva_unit02_button_idle);
}

[[nodiscard]] UiTheme make_quiet_slate_theme() noexcept
{
    UiTheme theme;

    const UiEnabledDisabledColors text = make_text_colors(
        elysia::core::colors::quiet_slate_white,
        elysia::core::colors::quiet_slate_gray);
    const UiEnabledDisabledColors secondary_text = make_text_colors(
        elysia::core::colors::quiet_slate_silver,
        elysia::core::colors::quiet_slate_gray);
    const UiEnabledDisabledColors placeholder_text = make_text_colors(
        elysia::core::colors::quiet_slate_silver,
        elysia::core::colors::quiet_slate_gray);
    const UiInteractiveColors border = UiInteractiveColors{
        elysia::core::colors::quiet_slate_silver,
        elysia::core::colors::quiet_slate_white,
        elysia::core::colors::quiet_slate_gray,
        elysia::core::colors::quiet_slate_gray
    };
    const UiInteractiveColors interactive_surface = make_surface_colors(
        elysia::core::colors::quiet_slate_charcoal,
        elysia::core::colors::quiet_slate_steel,
        elysia::core::colors::quiet_slate_ink,
        elysia::core::colors::quiet_slate_ink);
    const UiChromeThemeColors interactive_chrome = make_chrome(interactive_surface,border);

    theme.label_styles[to_index(UiLabelVisualRole::Default)] = UiLabelThemeColors{
        elysia::core::colors::quiet_slate_white,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Title)] = UiLabelThemeColors{
        elysia::core::colors::quiet_slate_white,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Subtitle)] = UiLabelThemeColors{
        elysia::core::colors::quiet_slate_silver,
        elysia::core::colors::transparent
    };
    theme.label_styles[to_index(UiLabelVisualRole::Muted)] = UiLabelThemeColors{
        elysia::core::colors::quiet_slate_silver,
        elysia::core::colors::transparent
    };

    theme.number_style = UiNumberThemeColors{
        elysia::core::colors::quiet_slate_white,
        elysia::core::colors::transparent
    };

    theme.bar_styles[to_index(UiBarVisualRole::Default)] = UiBarThemeColors{
        elysia::core::colors::quiet_slate_ink,
        elysia::core::colors::quiet_slate_silver,
        elysia::core::colors::quiet_slate_gray
    };
    theme.bar_styles[to_index(UiBarVisualRole::Progress)] = UiBarThemeColors{
        elysia::core::colors::quiet_slate_ink,
        elysia::core::colors::quiet_slate_white,
        elysia::core::colors::quiet_slate_silver
    };

    theme.panel_styles[to_index(UiPanelVisualRole::Default)] = UiPanelThemeColors{
        elysia::core::colors::quiet_slate_charcoal,
        elysia::core::colors::quiet_slate_silver
    };
    theme.panel_styles[to_index(UiPanelVisualRole::Screen)] = UiPanelThemeColors{
        elysia::core::colors::quiet_slate_ink,
        elysia::core::colors::quiet_slate_gray
    };
    theme.panel_styles[to_index(UiPanelVisualRole::Dialog)] = UiPanelThemeColors{
        elysia::core::colors::quiet_slate_charcoal,
        elysia::core::colors::quiet_slate_white
    };
    theme.panel_styles[to_index(UiPanelVisualRole::List)] = UiPanelThemeColors{
        elysia::core::colors::quiet_slate_charcoal,
        elysia::core::colors::quiet_slate_silver
    };

    theme.window_style = UiWindowThemeColors{
        elysia::core::colors::quiet_slate_ink,
        elysia::core::colors::quiet_slate_gray
    };

    theme.chrome_container_style = UiChromeContainerThemeColors{
        elysia::core::colors::quiet_slate_charcoal,
        elysia::core::colors::quiet_slate_silver,
        elysia::core::colors::quiet_slate_ink
    };

    theme.button_styles[to_index(UiButtonVisualRole::Default)] = UiButtonThemeColors{
        interactive_chrome,
        text
    };
    theme.button_styles[to_index(UiButtonVisualRole::Primary)] = UiButtonThemeColors{
        make_chrome(
            make_surface_colors(
                elysia::core::colors::quiet_slate_mist,
                elysia::core::colors::quiet_slate_white,
                elysia::core::colors::quiet_slate_silver,
                elysia::core::colors::quiet_slate_charcoal),
            UiInteractiveColors{
                elysia::core::colors::quiet_slate_gray,
                elysia::core::colors::quiet_slate_charcoal,
                elysia::core::colors::quiet_slate_ink,
                elysia::core::colors::quiet_slate_gray }),
        make_text_colors(
            elysia::core::colors::quiet_slate_ink,
            elysia::core::colors::quiet_slate_gray)
    };
    theme.button_styles[to_index(UiButtonVisualRole::Danger)] = UiButtonThemeColors{
        make_chrome(
            make_surface_colors(
                elysia::core::colors::quiet_slate_danger,
                elysia::core::colors::quiet_slate_danger_focus,
                elysia::core::colors::quiet_slate_danger_active,
                elysia::core::colors::quiet_slate_ink),
            UiInteractiveColors{
                elysia::core::colors::quiet_slate_silver,
                elysia::core::colors::quiet_slate_white,
                elysia::core::colors::quiet_slate_silver,
                elysia::core::colors::quiet_slate_gray }),
        text
    };

    theme.checkbox_style = UiCheckboxThemeColors{
        interactive_chrome,
        UiEnabledDisabledColors{
            elysia::core::colors::quiet_slate_white,
            elysia::core::colors::quiet_slate_gray }
    };

    theme.radio_button_style = UiRadioButtonThemeColors{
        interactive_chrome,
        UiEnabledDisabledColors{
            elysia::core::colors::quiet_slate_white,
            elysia::core::colors::quiet_slate_gray }
    };

    theme.drag_handle_style = UiDragHandleThemeColors{
        interactive_chrome
    };

    theme.slider_style = UiSliderThemeColors{
        make_chrome(interactive_surface,border),
        UiEnabledDisabledColors{
            elysia::core::colors::quiet_slate_white,
            elysia::core::colors::quiet_slate_gray },
        text,
        theme.drag_handle_style
    };

    theme.text_input_style = UiTextInputThemeColors{
        interactive_chrome,
        secondary_text,
        placeholder_text,
        elysia::core::colors::quiet_slate_white
    };

    theme.scroll_container_style = UiScrollContainerThemeColors{
        make_scrollbar_colors(
            elysia::core::colors::quiet_slate_ink,
            elysia::core::colors::quiet_slate_charcoal,
            elysia::core::colors::quiet_slate_steel,
            elysia::core::colors::quiet_slate_ink,
            elysia::core::colors::quiet_slate_silver,
            elysia::core::colors::quiet_slate_white,
            elysia::core::colors::quiet_slate_white,
            elysia::core::colors::quiet_slate_gray),
        elysia::core::colors::quiet_slate_ink,
        elysia::core::colors::quiet_slate_gray
    };

    theme.button_styles[to_index(UiButtonVisualRole::Default)].chrome.background.focused =
        elysia::core::colors::quiet_slate_button_focus;
    return finalize_theme(std::move(theme),
        elysia::core::colors::quiet_slate_ink,elysia::core::colors::quiet_slate_charcoal,
        elysia::core::colors::quiet_slate_button_idle);
}
}

const UiPanelThemeColors& UiTheme::panel(UiPanelVisualRole role) const noexcept
{
    return panel_styles[to_index(role)];
}

const UiLabelThemeColors& UiTheme::label(UiLabelVisualRole role) const noexcept
{
    return label_styles[to_index(role)];
}

const UiButtonThemeColors& UiTheme::button(UiButtonVisualRole role) const noexcept
{
    return button_styles[to_index(role)];
}

const UiBarThemeColors& UiTheme::bar(UiBarVisualRole role) const noexcept
{
    return bar_styles[to_index(role)];
}

UiTheme make_builtin_theme(UiBuiltinTheme theme) noexcept
{
    switch (theme)
    {
    case UiBuiltinTheme::EvangelionUnit00:
        return make_evangelion_unit00_theme();
    case UiBuiltinTheme::EvangelionUnit01:
        return make_evangelion_unit01_theme();
    case UiBuiltinTheme::EvangelionUnit02:
        return make_evangelion_unit02_theme();
    case UiBuiltinTheme::QuietSlate:
        return make_quiet_slate_theme();
    case UiBuiltinTheme::ElysiaLight:
        return make_elysia_light_theme();
    case UiBuiltinTheme::ElysiaDark:
        return make_elysia_dark_theme();
    case UiBuiltinTheme::BlueGlassMoon:
    default:
        return make_blue_glass_moon_theme();
    }
}

const UiTheme& builtin_theme(UiBuiltinTheme theme) noexcept
{
    static const UiTheme blue_glass_moon = make_blue_glass_moon_theme();
    static const UiTheme elysia_light = make_elysia_light_theme();
    static const UiTheme elysia_dark = make_elysia_dark_theme();
    static const UiTheme evangelion_unit00 = make_evangelion_unit00_theme();
    static const UiTheme evangelion_unit01 = make_evangelion_unit01_theme();
    static const UiTheme evangelion_unit02 = make_evangelion_unit02_theme();
    static const UiTheme quiet_slate = make_quiet_slate_theme();

    switch (theme)
    {
    case UiBuiltinTheme::EvangelionUnit00:
        return evangelion_unit00;
    case UiBuiltinTheme::EvangelionUnit01:
        return evangelion_unit01;
    case UiBuiltinTheme::EvangelionUnit02:
        return evangelion_unit02;
    case UiBuiltinTheme::QuietSlate:
        return quiet_slate;
    case UiBuiltinTheme::ElysiaLight:
        return elysia_light;
    case UiBuiltinTheme::ElysiaDark:
        return elysia_dark;
    case UiBuiltinTheme::BlueGlassMoon:
    default:
        return blue_glass_moon;
    }
}
}
