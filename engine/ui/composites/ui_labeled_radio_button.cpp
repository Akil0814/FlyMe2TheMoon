#include "ui_labeled_radio_button.h"

#include "../style/ui_style_defaults.h"

#include <algorithm>

namespace elysia::ui
{
using elysia::typography::UiTypographyRole;

void UiLabeledRadioButton::set_base_styles(
    const UiRadioButtonStyle& radio,
    const UiLabelStyle& label,
    const UiEnabledDisabledColors& text_colors) noexcept
{
    _radio.set_base_style(radio);
    _theme_label_style = label;
    _label.set_base_style(_theme_label_style);
    _theme_text_colors = text_colors;
}

UiLabeledRadioButton::UiLabeledRadioButton(const elysia::core::Rect& rect,int order) noexcept
    : UiControl(rect,order),_radio({},order),_label({},order) { reset(); }
UiLabeledRadioButton::UiLabeledRadioButton(const elysia::core::Rect& rect,const UiLabeledRadioButtonConfig& config,int order) noexcept
    : UiLabeledRadioButton(rect,order)
{
    _radio.set_radio_button_config(config.radio); _text_content = config.text_content;
    _label_placement = config.label_placement; _text_placement = config.text_placement;
    _label_spacing = std::max(0.0f,config.label_spacing);
}
void UiLabeledRadioButton::reset() noexcept
{
    UiControl::reset(); _radio.reset(); _label.reset();
    _text_content = {}; _typography_role = UiTypographyRole::RadioLabel;
    _theme_label_style = UiStyleDefaults::label();
    _theme_text_colors = UiStyleDefaults::labeled_checkbox_text();
    _label_placement = UiLabeledRadioLabelPlacement::Right; _text_placement = UiLabeledRadioTextPlacement::NearIndicator; _label_spacing = 8.0f;
}
void UiLabeledRadioButton::set_enabled(bool enabled) { UiControl::set_enabled(enabled); _radio.set_enabled(enabled); }
void UiLabeledRadioButton::set_focused(bool focused) { UiControl::set_focused(focused); _radio.set_focused(focused); }
bool UiLabeledRadioButton::on_ui_input_event(const UiInputEvent& event)
{
    sync_children();
    if (event.type == UiInputEventType::MouseMoved)
    {
        set_focused(is_active() && is_visible() && is_enabled() && presentation_screen_rect().contains({ static_cast<float>(event.mouse_x),static_cast<float>(event.mouse_y) }));
        return false;
    }
    return _radio.on_ui_input_event(routed_event(event));
}
void UiLabeledRadioButton::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out) const
{ if (!is_visible()) return; sync_children(); _radio.submit_ui_render_commands(out); _label.submit_ui_render_commands(out); }
void UiLabeledRadioButton::set_selected(bool selected) noexcept { _radio.set_selected(selected); }
bool UiLabeledRadioButton::is_selected() const noexcept { return _radio.is_selected(); }
void UiLabeledRadioButton::set_on_selection_changed(UiRadioButtonSelectedCallback callback) { _radio.set_on_selection_changed(std::move(callback)); }
void UiLabeledRadioButton::set_text_content(UiTextContent content) { _text_content = std::move(content); }
elysia::core::Color UiLabeledRadioButton::resolved_text_color() const noexcept
{
    return is_enabled() ? _theme_text_colors.enabled : _theme_text_colors.disabled;
}
void UiLabeledRadioButton::set_label_placement(UiLabeledRadioLabelPlacement placement) noexcept { _label_placement = placement; }
void UiLabeledRadioButton::set_text_placement(UiLabeledRadioTextPlacement placement) noexcept { _text_placement = placement; }
void UiLabeledRadioButton::set_label_spacing(float spacing) noexcept { _label_spacing = std::max(0.0f,spacing); }
void UiLabeledRadioButton::set_typography_role(UiTypographyRole role) noexcept { _typography_role = role; }
void UiLabeledRadioButton::sync_children() const
{
    _radio.set_screen_rect(indicator_rect()); _radio.set_visible(is_visible()); _radio.set_active(is_active()); _radio.set_enabled(is_enabled()); _radio.set_focused(is_focused()); _radio.set_opacity(opacity());
    _label.set_screen_rect(label_rect()); _label.set_visible(is_visible()); _label.set_active(is_active()); _label.set_opacity(opacity()); _label.set_text_content(_text_content); _label.set_typography_role(_typography_role);
    auto style = _theme_label_style;
    style.text = resolved_text_color();
    _label.set_base_style(style);
    _label.set_vertical_align(TextVerticalAlign::Center);
    if (_label_placement == UiLabeledRadioLabelPlacement::Left)
    {
        _label.set_horizontal_align(
            _text_placement == UiLabeledRadioTextPlacement::NearIndicator
                ? TextHorizontalAlign::Right
                : TextHorizontalAlign::Left);
    }
    else
    {
        _label.set_horizontal_align(
            _text_placement == UiLabeledRadioTextPlacement::NearIndicator
                ? TextHorizontalAlign::Left
                : TextHorizontalAlign::Right);
    }
}
elysia::core::Rect UiLabeledRadioButton::indicator_rect() const noexcept
{
    const auto& r = screen_rect(); const float side = std::min(r.width(),r.height());
    return { _label_placement == UiLabeledRadioLabelPlacement::Left ? r.right() - side : r.x(),r.y(),side,r.height() };
}
elysia::core::Rect UiLabeledRadioButton::label_rect() const noexcept
{
    const auto& r = screen_rect();
    const auto indicator = indicator_rect();
    const float spacing = _text_content.empty() ? 0.0f : _label_spacing;
    if (_label_placement == UiLabeledRadioLabelPlacement::Left)
    {
        const float right = indicator.x() - spacing;
        return { r.x(),r.y(),std::max(0.0f,right-r.x()),r.height() };
    }
    const float left = indicator.right() + spacing;
    return { left,r.y(),std::max(0.0f,r.right()-left),r.height() };
}
UiInputEvent UiLabeledRadioButton::routed_event(const UiInputEvent& event) const noexcept
{
    UiInputEvent routed = event;
    if ((event.type == UiInputEventType::PointerPressed || event.type == UiInputEventType::PointerReleased)
        && presentation_screen_rect().contains({ static_cast<float>(event.mouse_x),static_cast<float>(event.mouse_y) }))
    { const auto c = indicator_rect().center(); routed.mouse_x = static_cast<int>(c.x); routed.mouse_y = static_cast<int>(c.y); }
    return routed;
}
}
