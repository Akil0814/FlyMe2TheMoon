#include "ui_labeled_checkbox.h"

#include "../style/ui_style_defaults.h"

#include <algorithm>

namespace elysia::ui
{
using elysia::typography::UiTypographyRole;

void UiLabeledCheckbox::set_base_styles(
    const UiCheckboxStyle& checkbox,
    const UiLabelStyle& label,
    const UiEnabledDisabledColors& text_colors) noexcept
{
    _checkbox.set_base_style(checkbox);
    _theme_label_style = label;
    _label.set_base_style(_theme_label_style);
    _theme_text_colors = text_colors;
}

UiLabeledCheckbox::UiLabeledCheckbox(const elysia::core::Rect& rect,int order) noexcept
    : UiControl(rect,order),_checkbox({},order),_label({},order) { reset(); }
UiLabeledCheckbox::UiLabeledCheckbox(const elysia::core::Rect& rect,const UiLabeledCheckboxConfig& config,int order) noexcept
    : UiLabeledCheckbox(rect,order) { set_labeled_checkbox_config(config); }

void UiLabeledCheckbox::reset() noexcept
{
    UiControl::reset(); _checkbox.reset(); _label.reset();
    _text_content = {}; _typography_role = UiTypographyRole::CheckboxLabel;
    _label_placement = UiLabeledCheckboxLabelPlacement::Right;
    _text_placement = UiLabeledCheckboxTextPlacement::NearBox;
    _theme_label_style = UiStyleDefaults::label();
    _theme_text_colors = UiStyleDefaults::labeled_checkbox_text();
    _text_colors_override.reset();
    _label_spacing = 8.0f; _indicator_padding = 4; _label_padding = 0;
}
void UiLabeledCheckbox::set_enabled(bool enabled) { UiControl::set_enabled(enabled); _checkbox.set_enabled(enabled); }
void UiLabeledCheckbox::set_focused(bool focused) { UiControl::set_focused(focused); _checkbox.set_focused(focused); }
bool UiLabeledCheckbox::on_ui_input_event(const UiInputEvent& event)
{
    sync_children();
    if (event.type == UiInputEventType::MouseMoved)
    {
        set_focused(is_active() && is_visible() && is_enabled() && presentation_screen_rect().contains({ static_cast<float>(event.mouse_x),static_cast<float>(event.mouse_y) }));
        return false;
    }
    return _checkbox.on_ui_input_event(event_for_indicator(event));
}
void UiLabeledCheckbox::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out) const
{
    if (!is_visible()) return; sync_children(); _checkbox.submit_ui_render_commands(out); _label.submit_ui_render_commands(out);
}
void UiLabeledCheckbox::set_labeled_checkbox_config(const UiLabeledCheckboxConfig& config)
{
    _checkbox.set_checkbox_config(config.checkbox); _text_content = config.text_content;
    _label_placement = config.label_placement; _label_spacing = std::max(0.0f,config.label_spacing);
    _text_placement = config.text_placement;
    _text_colors_override = config.text_colors;
}
void UiLabeledCheckbox::set_state(UiCheckboxState state) noexcept { _checkbox.set_state(state); }
UiCheckboxState UiLabeledCheckbox::state() const noexcept { return _checkbox.state(); }
void UiLabeledCheckbox::set_checked(bool checked) noexcept { _checkbox.set_checked(checked); }
bool UiLabeledCheckbox::is_checked() const noexcept { return _checkbox.is_checked(); }
void UiLabeledCheckbox::toggle() { _checkbox.toggle(); }
void UiLabeledCheckbox::set_on_toggled(UiCheckboxToggledCallback callback) { _checkbox.set_on_toggled(std::move(callback)); }
void UiLabeledCheckbox::set_text_content(UiTextContent content) { _text_content = std::move(content); }
const UiTextContent& UiLabeledCheckbox::text_content() const noexcept { return _text_content; }
elysia::core::Color UiLabeledCheckbox::resolved_text_color() const noexcept
{
    const UiEnabledDisabledColors& colors = _text_colors_override ? *_text_colors_override : _theme_text_colors;
    return is_enabled() ? colors.enabled : colors.disabled;
}
void UiLabeledCheckbox::set_label_placement(UiLabeledCheckboxLabelPlacement placement) noexcept { _label_placement = placement; }
void UiLabeledCheckbox::set_text_placement(UiLabeledCheckboxTextPlacement placement) noexcept { _text_placement = placement; }
void UiLabeledCheckbox::set_label_spacing(float spacing) noexcept { _label_spacing = std::max(0.0f,spacing); }
void UiLabeledCheckbox::set_typography_role(UiTypographyRole role) noexcept { _typography_role = role; }
void UiLabeledCheckbox::set_label_padding(int padding) noexcept { _label_padding = std::max(0,padding); }
void UiLabeledCheckbox::set_padding(int padding) noexcept { _indicator_padding = std::max(0,padding); _checkbox.set_padding(_indicator_padding); }
void UiLabeledCheckbox::sync_children() const
{
    _checkbox.set_screen_rect(indicator_rect()); _checkbox.set_visible(is_visible()); _checkbox.set_active(is_active());
    _checkbox.set_enabled(is_enabled()); _checkbox.set_focused(is_focused()); _checkbox.set_opacity(opacity());
    _label.set_screen_rect(label_rect()); _label.set_visible(is_visible()); _label.set_active(is_active()); _label.set_opacity(opacity());
    _label.set_text_content(_text_content); _label.set_typography_role(_typography_role); _label.set_padding(_label_padding);
    auto style = _theme_label_style;
    style.text = resolved_text_color();
    _label.set_base_style(style);
    _label.set_vertical_align(TextVerticalAlign::Center);
    if (_label_placement == UiLabeledCheckboxLabelPlacement::Left)
    {
        _label.set_horizontal_align(
            _text_placement == UiLabeledCheckboxTextPlacement::NearBox
                ? TextHorizontalAlign::Right
                : TextHorizontalAlign::Left);
    }
    else
    {
        _label.set_horizontal_align(
            _text_placement == UiLabeledCheckboxTextPlacement::NearBox
                ? TextHorizontalAlign::Left
                : TextHorizontalAlign::Right);
    }
}
elysia::core::Rect UiLabeledCheckbox::indicator_rect() const noexcept
{
    const auto& r = screen_rect(); const float side = std::min(r.width(),r.height());
    return { _label_placement == UiLabeledCheckboxLabelPlacement::Left ? r.right() - side : r.x(),r.y(),side,r.height() };
}
elysia::core::Rect UiLabeledCheckbox::label_rect() const noexcept
{
    const auto& r = screen_rect();
    const auto box = indicator_rect();
    const float spacing = _text_content.empty() ? 0.0f : _label_spacing;
    if (_label_placement == UiLabeledCheckboxLabelPlacement::Left)
    {
        const float right = box.x() - spacing;
        return { r.x(),r.y(),std::max(0.0f,right - r.x()),r.height() };
    }
    const float left = box.right() + spacing;
    return { left,r.y(),std::max(0.0f,r.right() - left),r.height() };
}
UiInputEvent UiLabeledCheckbox::event_for_indicator(const UiInputEvent& event) const noexcept
{
    UiInputEvent routed = event;
    if ((event.type == UiInputEventType::PointerPressed || event.type == UiInputEventType::PointerReleased)
        && presentation_screen_rect().contains({ static_cast<float>(event.mouse_x),static_cast<float>(event.mouse_y) }))
    { const auto c = indicator_rect().center(); routed.mouse_x = static_cast<int>(c.x); routed.mouse_y = static_cast<int>(c.y); }
    return routed;
}
}
