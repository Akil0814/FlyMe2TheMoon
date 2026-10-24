#include "ui_theme_style_resolver.h"

#include "../core/ui_element.h"
#include "../window/ui_window.h"
#include "../containers/ui_chrome_container.h"
#include "../containers/ui_panel.h"
#include "../containers/ui_scroll_container.h"
#include "../composites/ui_dialog.h"
#include "../composites/ui_dropdown.h"
#include "../composites/ui_option_selector.h"
#include "../composites/ui_labeled_checkbox.h"
#include "../composites/ui_labeled_radio_button.h"
#include "../widgets/label/ui_label.h"
#include "../widgets/number/ui_number.h"
#include "../widgets/text/ui_text_block.h"
#include "../widgets/ui_bar.h"
#include "../widgets/ui_button.h"
#include "../widgets/ui_action_button.h"
#include "../widgets/ui_checkbox.h"
#include "../widgets/ui_drag_handle.h"
#include "../widgets/ui_radio_button.h"
#include "../widgets/ui_slider.h"
#include "../widgets/ui_text_input.h"

namespace elysia::ui
{
UiThemeStyleResolution UiThemeStyleResolver::apply(UiElement& element,const UiTheme& theme) const
{
    const auto found = _adapters.find(std::type_index(typeid(element)));
    if (found == _adapters.end() || !found->second)
        return {};
    return found->second->apply(element,theme);
}

void register_builtin_ui_theme_adapters(UiThemeStyleResolver& r)
{
    (void)r.register_adapter<UiWindow>([](UiWindow& e,const UiTheme& t) { e.set_base_style(apply_theme_colors(UiWindowStyle{},t.window_style)); });
    (void)r.register_adapter<UiChromeContainer>([](UiChromeContainer& e,const UiTheme& t) { e.set_base_style(apply_theme_colors(UiChromeContainerStyle{},t.chrome_container_style)); });
    (void)r.register_adapter<UiPanel>([](UiPanel& e,const UiTheme& t) { e.set_base_style(apply_theme_colors(UiPanelStyle{},t.panel(e.visual_role()))); });
    (void)r.register_adapter<UiScrollContainer>([](UiScrollContainer& e,const UiTheme& t) { e.set_base_style(apply_theme_colors(UiScrollContainerStyle{},t.scroll_container_style)); });
    (void)r.register_adapter<UiLabel>([](UiLabel& e,const UiTheme& t) { e.set_base_style(apply_theme_colors(UiLabelStyle{},t.label(e.visual_role()))); });
    (void)r.register_adapter<UiTextBlock>([](UiTextBlock& e,const UiTheme& t) {
        const UiLabelVisualRole role = e.visual_role() == UiTextBlockVisualRole::Muted ? UiLabelVisualRole::Muted : UiLabelVisualRole::Default;
        e.set_base_style(apply_theme_colors(UiTextBlockStyle{},t.label(role)));
    });
    (void)r.register_adapter<UiNumber>([](UiNumber& e,const UiTheme& t) { e.set_base_style(apply_theme_colors(UiNumberStyle{},t.number_style)); });
    (void)r.register_adapter<UiBar>([](UiBar& e,const UiTheme& t) { e.set_base_style(apply_theme_colors(UiBarStyle{},t.bar(e.visual_role()))); });
    (void)r.register_adapter<UiButton>([](UiButton& e,const UiTheme& t) { e.set_base_style(apply_theme_colors(UiButtonStyle{},t.button(e.visual_role()))); });
    (void)r.register_adapter<UiActionButton>([](UiActionButton& e,const UiTheme& t) {
        const auto button = apply_theme_colors(UiButtonStyle{},t.button());
        UiActionButtonStyle style;
        style.chrome = button.chrome;
        style.text = button.text;
        style.selected_border = button.chrome.border.active;
        e.set_base_style(style);
    });
    (void)r.register_adapter<UiCheckbox>([](UiCheckbox& e,const UiTheme& t) { e.set_base_style(apply_theme_colors(UiCheckboxStyle{},t.checkbox_style)); });
    (void)r.register_adapter<UiRadioButton>([](UiRadioButton& e,const UiTheme& t) { e.set_base_style(apply_theme_colors(UiRadioButtonStyle{},t.radio_button_style)); });
    (void)r.register_adapter<UiDragHandle>([](UiDragHandle& e,const UiTheme& t) { e.set_base_style(apply_theme_colors(UiDragHandleStyle{},t.drag_handle_style)); });
    (void)r.register_adapter<UiSlider>([](UiSlider& e,const UiTheme& t) { e.set_base_style(apply_theme_colors(UiSliderStyle{},t.slider_style)); },UiThemeStyleTraversal::Stop);
    (void)r.register_adapter<UiTextInput>([](UiTextInput& e,const UiTheme& t) { e.set_base_style(apply_theme_colors(UiTextInputStyle{},t.text_input_style)); });
    (void)r.register_adapter<UiDropdown>([](UiDropdown& e,const UiTheme& t) {
        UiScrollContainerStyle scroll = apply_theme_colors(UiScrollContainerStyle{},t.scroll_container_style);
        scroll.draw_background = false; scroll.draw_border = false;
        e.set_base_style(UiDropdownBaseStyle{
            UiDropdownStyle{},
            apply_theme_colors(UiButtonStyle{},t.button(UiButtonVisualRole::Default)),
            apply_theme_colors(UiPanelStyle{},t.panel(UiPanelVisualRole::List)),
            scroll,
            apply_theme_colors(UiButtonStyle{},t.button(UiButtonVisualRole::Default))
        });
    },UiThemeStyleTraversal::Stop);
    (void)r.register_adapter<UiOptionSelector>([](UiOptionSelector& e,const UiTheme& t) {
        UiOptionSelectorStyle style;
        style.arrow = apply_theme_colors(UiButtonStyle{},t.button(UiButtonVisualRole::Default));
        style.chrome = style.arrow.chrome;
        style.text = {t.label(UiLabelVisualRole::Default).text,style.arrow.text.disabled};
        e.set_base_style(style);
    },UiThemeStyleTraversal::Stop);
    (void)r.register_adapter<UiLabeledCheckbox>([](UiLabeledCheckbox& e,const UiTheme& t) {
        const UiLabelStyle label = apply_theme_colors(UiLabelStyle{},t.label(UiLabelVisualRole::Default));
        e.set_base_styles(
            apply_theme_colors(UiCheckboxStyle{},t.checkbox_style),
            label,
            UiEnabledDisabledColors{ label.text,t.button(UiButtonVisualRole::Default).text.disabled });
    },UiThemeStyleTraversal::Stop);
    (void)r.register_adapter<UiLabeledRadioButton>([](UiLabeledRadioButton& e,const UiTheme& t) {
        const UiLabelStyle label = apply_theme_colors(UiLabelStyle{},t.label(UiLabelVisualRole::Default));
        e.set_base_styles(
            apply_theme_colors(UiRadioButtonStyle{},t.radio_button_style),
            label,
            UiEnabledDisabledColors{ label.text,t.button(UiButtonVisualRole::Default).text.disabled });
    },UiThemeStyleTraversal::Stop);
}
}
