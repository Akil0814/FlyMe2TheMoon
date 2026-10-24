#include "ui_checkbox.h"

#include "../../audio/audio_service.h"
#include "../../core/render/render_command.h"
#include "../style/ui_style_defaults.h"


#include <algorithm>
#include <cmath>
#include <utility>

namespace elysia::ui
{
namespace
{
[[nodiscard]] bool has_visual_state_textures(const UiCheckboxVisualStateTextures& textures) noexcept
{
    return textures.idle
        && textures.focused
        && textures.pushed
        && textures.disabled;
}

}

UiCheckbox::UiCheckbox(const elysia::core::Rect& rect,int order) noexcept
    : UiControl(rect,order)
{
    reset();
}

UiCheckbox::UiCheckbox(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiCheckbox(elysia::core::Rect(position.x,position.y,size.x,size.y),order) {}

UiCheckbox::UiCheckbox(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiCheckbox(elysia::core::Rect::from_center(center,size),order) {}

UiCheckbox::UiCheckbox(const elysia::core::Rect& rect,const UiCheckboxConfig& config,int order) noexcept
    : UiCheckbox(rect,order)
{
    set_checkbox_config(config);
}

UiCheckbox::UiCheckbox(const elysia::core::Vector2& position,const elysia::core::Vector2& size,const UiCheckboxConfig& config,int order) noexcept
    : UiCheckbox(elysia::core::Rect(position.x,position.y,size.x,size.y),config,order) {}

UiCheckbox::UiCheckbox(
    const elysia::core::Vector2& center,const elysia::core::Vector2& size,
    UiFromCenterTag,const UiCheckboxConfig& config,int order
) noexcept : UiCheckbox(elysia::core::Rect::from_center(center,size),config,order) {}

void UiCheckbox::reset() noexcept
{
    UiControl::reset();

    _textures.reset();
    _sounds.reset();
    _on_toggled = nullptr;
    _state = UiCheckboxState::Unchecked;
    _style_state.reset(UiStyleDefaults::checkbox());
    _padding = 4;
    _is_pushed = false;
}

void UiCheckbox::set_enabled(bool enabled)
{
    UiControl::set_enabled(enabled);
    if (!enabled)
        clear_pushed_state();
}

void UiCheckbox::set_focused(bool focused)
{
    const bool was_focused = is_focused();
    UiControl::set_focused(focused);
    if (!is_focused())
        clear_pushed_state();
    if (!was_focused && is_focused() && _sounds)
        play_sound_if_set(_sounds->focus);
}

bool UiCheckbox::on_ui_input_event(const UiInputEvent& event)
{
    if (event.type == UiInputEventType::MouseMoved)
    {
        if (!can_receive_pointer())
        {
            set_focused(false);
            clear_pushed_state();
            return false;
        }

        set_focused(contains_pointer(event.mouse_x,event.mouse_y));
        return false;
    }

    if (event.type == UiInputEventType::PointerPressed)
    {
        if (!is_primary_pointer_event(event) || !can_receive_pointer())
            return false;
        if (!contains_pointer(event.mouse_x,event.mouse_y))
            return false;

        set_focused(true);
        _is_pushed = true;
        if (_sounds)
            play_sound_if_set(_sounds->press);
        return true;
    }

    if (event.type == UiInputEventType::PointerReleased)
    {
        if (!is_primary_pointer_event(event))
            return false;

        const bool was_pushed = _is_pushed;
        const bool is_inside = can_receive_pointer() && contains_pointer(event.mouse_x,event.mouse_y);
        set_focused(is_inside);
        clear_pushed_state();

        if (was_pushed && is_inside)
            return toggle_internal(true,true);

        return was_pushed;
    }

    if (event.action != UiAction::Confirm)
        return false;

    if (!can_interact())
    {
        clear_pushed_state();
        return false;
    }

    if (event.type == UiInputEventType::ActionPressed)
    {
        _is_pushed = true;
        if (_sounds)
            play_sound_if_set(_sounds->press);
        return true;
    }

    if (event.type == UiInputEventType::ActionReleased)
    {
        const bool should_toggle = _is_pushed;
        clear_pushed_state();
        if (should_toggle)
            return toggle_internal(true,true);
        return false;
    }

    return false;
}

void UiCheckbox::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;

    const elysia::core::Rect rect = checkbox_rect();
    if (rect.is_empty())
        return;

    if (uses_texture_rendering())
    {
        SDL_Texture* texture = current_state_texture();
        if (!texture)
            return;

        elysia::core::UiRenderCommand command = elysia::core::make_ui_texture_command(texture,rect);
        apply_opacity(command);
        out_commands.push_back(command);
        return;
    }

    const elysia::core::Color mark_color = apply_opacity(current_checkmark_color());
    const UiCheckboxStyle& style = _style_state.effective_style();
    if (style.mark_style == UiCheckboxMarkStyle::RadioDot)
    {
        const elysia::core::Vector2 center = rect.center();
        const float radius = std::min(rect.width(),rect.height()) * 0.5f;

        if (style.chrome.draw_background)
            out_commands.push_back(elysia::core::make_ui_fill_circle_command(
                center,
                radius,
                apply_opacity(current_background_color())));
        if (style.chrome.draw_border)
            out_commands.push_back(elysia::core::make_ui_draw_circle_command(
                center,
                radius,
                apply_opacity(current_border_color()),
                style.chrome.border_width));

        const float inset = std::max(2.0f,std::min(rect.width(),rect.height()) * 0.14f);
        const float inner_radius = std::max(0.0f,radius - inset);
        if (style.chrome.draw_background && inner_radius > 0.0f)
        {
            const elysia::core::Color background_color = apply_opacity(current_background_color());
            out_commands.push_back(elysia::core::make_ui_fill_circle_command(center,inner_radius,background_color));
            if (style.chrome.draw_border)
                out_commands.push_back(elysia::core::make_ui_draw_circle_command(center,inner_radius,background_color));
        }

        if (_state == UiCheckboxState::Checked)
        {
            const float dot_inset = std::max(4.0f,std::min(rect.width(),rect.height()) * 0.32f);
            const float dot_radius = std::max(0.0f,radius - dot_inset);
            if (dot_radius > 0.0f)
            {
                out_commands.push_back(elysia::core::make_ui_fill_circle_command(center,dot_radius,mark_color));
                out_commands.push_back(elysia::core::make_ui_draw_circle_command(center,dot_radius,mark_color));
            }
        }
        return;
    }

    if (style.chrome.draw_background)
        out_commands.push_back(elysia::core::make_ui_fill_rect_command(rect,apply_opacity(current_background_color()),style.chrome.corner_radius));
    if (style.chrome.draw_border)
        out_commands.push_back(elysia::core::make_ui_draw_rect_command(
            rect,
            apply_opacity(current_border_color()),
            style.chrome.corner_radius,
            style.chrome.border_width));

    if (_state == UiCheckboxState::Checked)
    {
        if (style.mark_style == UiCheckboxMarkStyle::FilledBox)
        {
            const float inset = std::max(2.0f,std::min(rect.width(),rect.height()) * 0.22f);
            const float fill_side = std::max(0.0f,std::round(std::min(rect.width(),rect.height()) - inset * 2.0f));
            const elysia::core::Vector2 fill_size(fill_side,fill_side);
            const elysia::core::Rect fill_rect = elysia::core::Rect::from_center(
                elysia::core::Vector2(std::round(rect.center().x),std::round(rect.center().y)),
                fill_size);
            if (!fill_rect.is_empty())
                out_commands.push_back(elysia::core::make_ui_fill_rect_command(fill_rect,mark_color,style.chrome.corner_radius));
        }
        else
        {
            const elysia::core::Vector2 start(rect.x() + rect.width() * 0.22f,rect.y() + rect.height() * 0.54f);
            const elysia::core::Vector2 mid(rect.x() + rect.width() * 0.43f,rect.y() + rect.height() * 0.76f);
            const elysia::core::Vector2 end(rect.x() + rect.width() * 0.80f,rect.y() + rect.height() * 0.28f);
            out_commands.push_back(elysia::core::make_ui_draw_line_command(start,mid,mark_color));
            out_commands.push_back(elysia::core::make_ui_draw_line_command(mid,end,mark_color));
        }
    }
    else if (_state == UiCheckboxState::Indeterminate)
    {
        const elysia::core::Vector2 start(rect.x() + rect.width() * 0.22f,rect.center().y);
        const elysia::core::Vector2 end(rect.x() + rect.width() * 0.78f,rect.center().y);
        out_commands.push_back(elysia::core::make_ui_draw_line_command(start,end,mark_color));
    }
}

void UiCheckbox::set_checkbox_config(const UiCheckboxConfig& config)
{
    apply_checkbox_config(config);
}

void UiCheckbox::set_state(UiCheckboxState state) noexcept
{
    (void)set_state_internal(state,false);
}

UiCheckboxState UiCheckbox::state() const noexcept
{
    return _state;
}

void UiCheckbox::set_checked(bool checked) noexcept
{
    (void)set_state_internal(checked ? UiCheckboxState::Checked : UiCheckboxState::Unchecked,false);
}

bool UiCheckbox::is_checked() const noexcept
{
    return _state == UiCheckboxState::Checked;
}

bool UiCheckbox::is_indeterminate() const noexcept
{
    return _state == UiCheckboxState::Indeterminate;
}

void UiCheckbox::toggle()
{
    (void)toggle_internal(true,false);
}

void UiCheckbox::set_base_style(const UiCheckboxStyle& style) noexcept
{
    _style_state.set_base_style(style);
}

void UiCheckbox::set_style_overrides(const UiCheckboxStyleOverrides& overrides) noexcept
{
    _style_state.set_style_overrides(overrides);
}

const UiCheckboxStyle& UiCheckbox::style() const noexcept
{
    return _style_state.effective_style();
}

const UiCheckboxStyleOverrides& UiCheckbox::style_overrides() const noexcept { return _style_state.style_overrides(); }
bool UiCheckbox::has_style_overrides() const noexcept
{
    return _style_state.has_style_overrides();
}

void UiCheckbox::clear_style_overrides() noexcept
{
    _style_state.clear_style_overrides();
}

void UiCheckbox::set_mark_style(UiCheckboxMarkStyle mark_style) noexcept
{
    UiCheckboxStyleOverrides overrides = style_overrides();
    overrides.mark_style = mark_style;
    set_style_overrides(overrides);
}

UiCheckboxMarkStyle UiCheckbox::mark_style() const noexcept
{
    return style().mark_style;
}

void UiCheckbox::set_state_textures(const UiCheckboxTextures& textures)
{
    _textures = textures;
}

void UiCheckbox::clear_state_textures() noexcept
{
    _textures.reset();
}

const std::optional<UiCheckboxTextures>& UiCheckbox::state_textures() const noexcept
{
    return _textures;
}

bool UiCheckbox::has_complete_state_textures() const noexcept
{
    return _textures
        && has_visual_state_textures(_textures->unchecked)
        && has_visual_state_textures(_textures->checked)
        && has_visual_state_textures(_textures->indeterminate);
}

void UiCheckbox::set_sounds(const UiCheckboxSounds& sounds)
{
    _sounds = sounds;
}

void UiCheckbox::clear_sounds() noexcept
{
    _sounds.reset();
}

const std::optional<UiCheckboxSounds>& UiCheckbox::sounds() const noexcept
{
    return _sounds;
}

void UiCheckbox::set_on_toggled(UiCheckboxToggledCallback on_toggled)
{
    _on_toggled = std::move(on_toggled);
}

void UiCheckbox::set_padding(int padding) noexcept
{
    _padding = std::max(0,padding);
}

int UiCheckbox::padding() const noexcept
{
    return _padding;
}

void UiCheckbox::apply_checkbox_config(const UiCheckboxConfig& config)
{
    if (config.textures)
        set_state_textures(*config.textures);
    else
        clear_state_textures();

    if (config.sounds)
        set_sounds(*config.sounds);
    else
        clear_sounds();

    if (config.style_overrides)
        set_style_overrides(*config.style_overrides);
    else
        clear_style_overrides();
}

bool UiCheckbox::set_state_internal(UiCheckboxState state,bool notify)
{
    if (_state == state)
        return false;

    _state = state;
    const UiCheckboxToggledCallback callback = notify ? _on_toggled : nullptr;
    const UiCheckboxState callback_state = _state;
    if (callback)
        callback(callback_state);
    return true;
}

bool UiCheckbox::toggle_internal(bool notify,bool play_toggle_sound)
{
    const UiCheckboxState next_state = toggled_state(_state);
    if (_state == next_state)
        return false;
    _state = next_state;
    if (play_toggle_sound && _sounds)
        play_sound_if_set(_sounds->toggle);
    const UiCheckboxToggledCallback callback = notify ? _on_toggled : nullptr;
    const UiCheckboxState callback_state = _state;
    if (callback)
        callback(callback_state);
    return true;
}

bool UiCheckbox::can_interact() const noexcept
{
    return is_enabled() && is_focused() && is_active() && is_visible();
}

bool UiCheckbox::can_receive_pointer() const noexcept
{
    return is_enabled() && is_active() && is_visible();
}

bool UiCheckbox::contains_pointer(int mouse_x,int mouse_y) const noexcept
{
    return presentation_screen_rect().contains(elysia::core::Vector2(static_cast<float>(mouse_x),static_cast<float>(mouse_y)));
}

bool UiCheckbox::is_primary_pointer_event(const UiInputEvent& event) const noexcept
{
    return event.device == elysia::input::InputDevice::Mouse
        && event.control == elysia::input::RawInputControl::MouseLeft;
}

void UiCheckbox::clear_pushed_state() noexcept
{
    _is_pushed = false;
}

void UiCheckbox::play_sound_if_set(const std::string& sound_key) const
{
    if (sound_key.empty())
        return;
    elysia::audio::AudioService::instance()->request_sound(sound_key,{
        .group = elysia::audio::SoundGroup::Ui
    });
}

elysia::core::Rect UiCheckbox::content_rect() const noexcept
{
    const elysia::core::Rect& control_rect = screen_rect();
    const float width = std::max(0.0f,control_rect.width());
    const float height = std::max(0.0f,control_rect.height());
    const float padding = static_cast<float>(_padding);
    const float pad_x = std::min(padding,width * 0.5f);
    const float pad_y = std::min(padding,height * 0.5f);

    elysia::core::Rect content = control_rect;
    content.set_x(control_rect.x() + pad_x);
    content.set_y(control_rect.y() + pad_y);
    content.set_width(width - pad_x * 2.0f);
    content.set_height(height - pad_y * 2.0f);
    return content;
}

elysia::core::Rect UiCheckbox::checkbox_rect() const noexcept
{
    const elysia::core::Rect content = content_rect();
    if (content.is_empty())
        return elysia::core::Rect::zero();

    const float side = std::min(content.width(),content.height());
    if (side <= 0.0f)
        return elysia::core::Rect::zero();

    return elysia::core::Rect::from_center(content.center(),elysia::core::Vector2(side,side));
}

const UiCheckboxVisualStateTextures* UiCheckbox::current_state_textures() const noexcept
{
    if (!_textures)
        return nullptr;

    switch (_state)
    {
    case UiCheckboxState::Checked:
        return &_textures->checked;
    case UiCheckboxState::Indeterminate:
        return &_textures->indeterminate;
    case UiCheckboxState::Unchecked:
    default:
        return &_textures->unchecked;
    }
}

SDL_Texture* UiCheckbox::current_state_texture() const noexcept
{
    const UiCheckboxVisualStateTextures* textures = current_state_textures();
    if (!textures)
        return nullptr;
    if (!is_enabled())
        return textures->disabled;
    if (_is_pushed)
        return textures->pushed;
    if (is_focused())
        return textures->focused;
    return textures->idle;
}

bool UiCheckbox::uses_texture_rendering() const noexcept
{
    return has_complete_state_textures();
}

elysia::core::Color UiCheckbox::current_background_color() const noexcept
{
    return resolve_interactive_color(style().chrome.background,is_enabled(),is_focused(),_is_pushed);
}

elysia::core::Color UiCheckbox::current_border_color() const noexcept
{
    return resolve_interactive_color(style().chrome.border,is_enabled(),is_focused(),_is_pushed);
}

elysia::core::Color UiCheckbox::current_checkmark_color() const noexcept
{
    return resolve_enabled_disabled_color(style().mark,is_enabled());
}

UiCheckboxState UiCheckbox::toggled_state(UiCheckboxState state) noexcept
{
    switch (state)
    {
    case UiCheckboxState::Checked:
        return UiCheckboxState::Unchecked;
    case UiCheckboxState::Indeterminate:
        return UiCheckboxState::Checked;
    case UiCheckboxState::Unchecked:
    default:
        return UiCheckboxState::Checked;
    }
}

}

