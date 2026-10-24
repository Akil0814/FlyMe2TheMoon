#include "ui_radio_button.h"

#include "../style/ui_style_defaults.h"
#include "../../audio/audio_service.h"
#include "../../core/render/render_command.h"

#include <algorithm>

namespace elysia::ui
{
UiRadioButton::UiRadioButton(const elysia::core::Rect& rect,int order) noexcept : UiControl(rect,order) { reset(); }
UiRadioButton::UiRadioButton(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiRadioButton(elysia::core::Rect(position,size),order) {}
UiRadioButton::UiRadioButton(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiRadioButton(elysia::core::Rect::from_center(center,size),order) {}
UiRadioButton::UiRadioButton(const elysia::core::Rect& rect,const UiRadioButtonConfig& config,int order) noexcept
    : UiRadioButton(rect,order) { set_radio_button_config(config); }

void UiRadioButton::reset() noexcept
{
    UiControl::reset();
    _on_selected = {};
    _sounds.reset();
    _style_state.reset(UiStyleDefaults::radio_button());
    _padding = 4;
    _selected = false;
    _pushed = false;
}

void UiRadioButton::set_enabled(bool enabled) { UiControl::set_enabled(enabled); if (!enabled) _pushed = false; }
void UiRadioButton::set_focused(bool focused)
{
    const bool changed = focused != is_focused();
    UiControl::set_focused(focused);
    if (!focused) _pushed = false;
    if (changed && focused && _sounds) play_sound_if_set(_sounds->focus);
}

bool UiRadioButton::on_ui_input_event(const UiInputEvent& event)
{
    if (event.type == UiInputEventType::MouseMoved)
    {
        set_focused(is_active() && is_visible() && is_enabled() && contains_pointer(event.mouse_x,event.mouse_y));
        return false;
    }
    if (event.type == UiInputEventType::PointerPressed)
    {
        if (!is_primary_pointer_event(event) || !can_interact() || !contains_pointer(event.mouse_x,event.mouse_y)) return false;
        set_focused(true); _pushed = true; if (_sounds) play_sound_if_set(_sounds->press); return true;
    }
    if (event.type == UiInputEventType::PointerReleased)
    {
        if (!is_primary_pointer_event(event)) return false;
        const bool activate = _pushed && can_interact() && contains_pointer(event.mouse_x,event.mouse_y);
        const bool handled = _pushed; _pushed = false;
        if (activate) return select_internal(true);
        return handled;
    }
    if (event.action != UiAction::Confirm || !can_interact()) return false;
    if (event.type == UiInputEventType::ActionPressed) { _pushed = true; if (_sounds) play_sound_if_set(_sounds->press); return true; }
    if (event.type == UiInputEventType::ActionReleased) { const bool activate = _pushed; _pushed = false; return activate && select_internal(true); }
    return false;
}

void UiRadioButton::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out) const
{
    if (!is_visible()) return;
    const auto& outer = screen_rect();
    const float inset = static_cast<float>(_padding);
    const elysia::core::Rect rect(
        outer.x() + inset,outer.y() + inset,
        std::max(0.0f,outer.width() - inset * 2.0f),
        std::max(0.0f,outer.height() - inset * 2.0f));
    if (rect.is_empty()) return;
    const float radius = std::min(rect.width(),rect.height()) * 0.5f;
    if (style().chrome.draw_background) out.push_back(elysia::core::make_ui_fill_circle_command(rect.center(),radius,apply_opacity(background_color())));
    if (style().chrome.draw_border)
    {
        out.push_back(elysia::core::make_ui_draw_circle_command(
            rect.center(),
            radius,
            apply_opacity(border_color()),
            style().chrome.border_width));
    }
    if (_selected)
    {
        const float dot = std::max(0.0f,radius - std::max(4.0f,std::min(rect.width(),rect.height()) * 0.32f));
        if (dot > 0.0f) out.push_back(elysia::core::make_ui_fill_circle_command(rect.center(),dot,apply_opacity(mark_color())));
    }
}

void UiRadioButton::set_radio_button_config(const UiRadioButtonConfig& config)
{
    if (config.sounds) set_sounds(*config.sounds); else clear_sounds();
    if (config.style_overrides) set_style_overrides(*config.style_overrides); else clear_style_overrides();
    set_padding(config.padding);
}
void UiRadioButton::set_selected(bool selected) noexcept { _selected = selected; }
bool UiRadioButton::is_selected() const noexcept { return _selected; }
void UiRadioButton::select() { (void)select_internal(true); }
void UiRadioButton::set_on_selection_changed(UiRadioButtonSelectedCallback callback) { _on_selected = std::move(callback); }
void UiRadioButton::set_sounds(const UiRadioButtonSounds& sounds) { _sounds = sounds; }
void UiRadioButton::clear_sounds() noexcept { _sounds.reset(); }
void UiRadioButton::set_base_style(const UiRadioButtonStyle& style) noexcept
{
    _style_state.set_base_style(style);
}

void UiRadioButton::set_style_overrides(const UiRadioButtonStyleOverrides& overrides) noexcept { _style_state.set_style_overrides(overrides); }
const UiRadioButtonStyle& UiRadioButton::style() const noexcept { return _style_state.effective_style(); }
const UiRadioButtonStyleOverrides& UiRadioButton::style_overrides() const noexcept { return _style_state.style_overrides(); }
bool UiRadioButton::has_style_overrides() const noexcept { return _style_state.has_style_overrides(); }
void UiRadioButton::clear_style_overrides() noexcept { _style_state.clear_style_overrides(); }
void UiRadioButton::set_padding(int padding) noexcept { _padding = std::max(0,padding); }
int UiRadioButton::padding() const noexcept { return _padding; }
bool UiRadioButton::select_internal(bool notify)
{
    if (_selected) return true;
    _selected = true;
    if (_sounds) play_sound_if_set(_sounds->select);
    const UiRadioButtonSelectedCallback callback = notify ? _on_selected : nullptr;
    if (callback) callback();
    return true;
}
bool UiRadioButton::contains_pointer(int x,int y) const noexcept { return presentation_screen_rect().contains({ static_cast<float>(x),static_cast<float>(y) }); }
bool UiRadioButton::can_interact() const noexcept { return is_active() && is_visible() && is_enabled(); }
bool UiRadioButton::is_primary_pointer_event(const UiInputEvent& event) const noexcept { return event.device == elysia::input::InputDevice::Mouse && event.control == elysia::input::RawInputControl::MouseLeft; }
void UiRadioButton::play_sound_if_set(const std::string& key) const { if (!key.empty()) elysia::audio::AudioService::instance()->request_sound(key,{ .group = elysia::audio::SoundGroup::Ui }); }
elysia::core::Color UiRadioButton::background_color() const noexcept { return resolve_interactive_color(style().chrome.background,is_enabled(),is_focused(),_pushed); }
elysia::core::Color UiRadioButton::border_color() const noexcept
{
    return resolve_interactive_color(style().chrome.border,is_enabled(),is_focused(),_pushed);
}
elysia::core::Color UiRadioButton::mark_color() const noexcept { return resolve_enabled_disabled_color(style().mark,is_enabled()); }
}
