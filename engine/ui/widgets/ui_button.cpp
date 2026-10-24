#include "engine/core/render/sdl_texture_size.h"
#include "ui_button.h"

#include "../style/ui_style_defaults.h"

#include "../../audio/audio_service.h"
#include "../../core/render/colors.h"
#include "../../core/render/render_command.h"
#include "../../localization/localization_service.h"
#include "../../localization/localized_text_style.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <type_traits>
#include <utility>

namespace elysia::ui
{
using elysia::typography::UiTypographyRole;

UiButton::UiButton(const elysia::core::Rect& rect,int order) noexcept
    : UiControl(rect,order)
{
    reset();
}

UiButton::UiButton(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiButton(elysia::core::Rect(position.x,position.y,size.x,size.y),order) {}

UiButton::UiButton(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiButton(elysia::core::Rect::from_center(center,size),order) {}

UiButton::UiButton(const elysia::core::Rect& rect,const UiButtonConfig& config,int order) noexcept
    : UiButton(rect,order)
{
    set_button_config(config);
}

UiButton::UiButton(const elysia::core::Vector2& position,const elysia::core::Vector2& size,const UiButtonConfig& config,int order) noexcept
    : UiButton(elysia::core::Rect(position.x,position.y,size.x,size.y),config,order) {}

UiButton::UiButton(
    const elysia::core::Vector2& center,const elysia::core::Vector2& size,
    UiFromCenterTag,const UiButtonConfig& config,int order
) noexcept : UiButton(elysia::core::Rect::from_center(center,size),config,order) {}

void UiButton::reset() noexcept
{
    UiControl::reset();

    _text_content = UiTextContent{};
    _sounds = UiButtonSounds{};
    _state_textures = UiButtonTextures{};
    _on_click = nullptr;
    _visual_mode = UiButtonVisualMode::None;
    _style_state.reset(UiStyleDefaults::button());
    _visual_role = UiButtonVisualRole::Default;
    _typography_role = UiTypographyRole::Button;
    _font_source_override.reset();
    _padding = 10;
    _is_pushed = false;
}

void UiButton::set_enabled(bool enabled)
{
    UiControl::set_enabled(enabled);
    if (!enabled)
        clear_pushed_state();
}

void UiButton::set_focused(bool focused)
{
    const bool was_focused = is_focused();
    UiControl::set_focused(focused);
    if (!is_focused())
        clear_pushed_state();
    if (!was_focused && is_focused())
        play_sound_if_set(_sounds.focus);
}

bool UiButton::on_ui_input_event(const UiInputEvent& event)
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
        play_sound_if_set(_sounds.press);
        return true;
    }

    if (event.type == UiInputEventType::PointerReleased)
    {
        if (!is_primary_pointer_event(event))
            return false;

        const bool was_pushed = _is_pushed;
        const bool is_inside = can_receive_pointer() && contains_pointer(event.mouse_x,event.mouse_y);
        const ClickCallback on_click = _on_click;
        set_focused(is_inside);
        clear_pushed_state();

        if (was_pushed && is_inside)
        {
            play_sound_if_set(_sounds.click);
            if (on_click)
                on_click();
            return true;
        }

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
        play_sound_if_set(_sounds.press);
        return true;
    }

    if (event.type == UiInputEventType::ActionReleased)
    {
        const bool should_click = _is_pushed;
        const ClickCallback on_click = _on_click;
        clear_pushed_state();

        if (should_click)
        {
            play_sound_if_set(_sounds.click);
            if (on_click)
                on_click();
        }

        return should_click;
    }

    return false;
}

void UiButton::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;

    const elysia::core::Rect& button_rect = screen_rect();
    if (button_rect.is_empty())
        return;

    if (_visual_mode == UiButtonVisualMode::Textured)
    {
        SDL_Texture* state_texture = current_state_texture();
        if (!state_texture)
            return;

        elysia::core::UiRenderCommand command = elysia::core::make_ui_texture_command(state_texture,button_rect);
        apply_opacity(command);
        out_commands.push_back(command);
        if (style().chrome.draw_border)
        {
            out_commands.push_back(elysia::core::make_ui_draw_rect_command(
                button_rect,
                apply_opacity(current_border_color()),
                style().chrome.corner_radius,
                style().chrome.border_width));
        }
        return;
    }

    const UiButtonStyle& style = _style_state.effective_style();
    if (style.chrome.draw_background)
        out_commands.push_back(elysia::core::make_ui_fill_rect_command(button_rect,apply_opacity(current_background_color()),style.chrome.corner_radius));
    if (style.chrome.draw_border)
        out_commands.push_back(elysia::core::make_ui_draw_rect_command(
            button_rect,
            apply_opacity(current_border_color()),
            style.chrome.corner_radius,
            style.chrome.border_width));

    if (_visual_mode == UiButtonVisualMode::Icon)
    {
        SDL_Texture* icon_texture = current_state_texture();
        if (!icon_texture)
            return;

        const elysia::core::Rect icon_rect = text_render_rect(icon_texture);
        if (icon_rect.is_empty())
            return;

        elysia::core::UiRenderCommand command = elysia::core::make_ui_texture_command(icon_texture,icon_rect);
        apply_opacity(command);
        out_commands.push_back(command);
        return;
    }

    if (_text_content.empty())
        return;

    elysia::localization::LocalizationService* localization_service = ELYSIA_LOCALIZATION;

    const UiResolvedTextStyle typography = resolve_ui_typography(_typography_role);
    elysia::localization::LocalizedTextStyle text_style;
    text_style.typography_role = _typography_role;
    text_style.font_source_override = _font_source_override;
    text_style.color = current_text_color();
    text_style.wrap_width = typography.wrap_allowed ? std::max(0,static_cast<int>(content_rect().width())) : 0;

    SDL_Texture* text_texture = nullptr;
    if (_text_content.kind == UiTextContentKind::TextKey)
        text_texture = localization_service->get_text_texture(_text_content.value,text_style);
    else if (_text_content.kind == UiTextContentKind::RawText)
        text_texture = localization_service->get_raw_text_texture(_text_content.value,text_style);
    if (!text_texture)
        return;

    const elysia::core::Rect text_rect = text_render_rect(text_texture);
    if (text_rect.is_empty())
        return;

    elysia::core::UiRenderCommand command = elysia::core::make_ui_texture_command(text_texture,text_rect);
    apply_opacity(command);
    out_commands.push_back(command);
}

void UiButton::set_button_config(const UiButtonConfig& config)
{
    apply_button_config(config);
}

void UiButton::set_text_content(UiTextContent text_content)
{
    clear_content();
    _text_content = std::move(text_content);
    _visual_mode = _text_content.empty() ? UiButtonVisualMode::None : UiButtonVisualMode::Text;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

const UiTextContent& UiButton::text_content() const noexcept
{
    return _text_content;
}

void UiButton::set_typography_role(UiTypographyRole role) noexcept
{
    _typography_role = role;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

UiTypographyRole UiButton::typography_role() const noexcept
{
    return _typography_role;
}

void UiButton::set_font_source_override(
    elysia::typography::FontSource source) noexcept
{
    if (_font_source_override == source)
        return;
    _font_source_override = source;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiButton::clear_font_source_override() noexcept
{
    if (!_font_source_override)
        return;
    _font_source_override.reset();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

std::optional<elysia::typography::FontSource>
UiButton::font_source_override() const noexcept
{
    return _font_source_override;
}

void UiButton::set_state_textures(const UiButtonTextures& textures)
{
    clear_content();
    _state_textures = textures;
    _visual_mode = has_state_textures() ? UiButtonVisualMode::Textured : UiButtonVisualMode::None;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiButton::clear_state_textures()
{
    _state_textures = UiButtonTextures{};
    if (_visual_mode == UiButtonVisualMode::Textured || _visual_mode == UiButtonVisualMode::Icon)
        _visual_mode = UiButtonVisualMode::None;
}

bool UiButton::has_state_textures() const noexcept
{
    return _state_textures.idle
        || _state_textures.focused
        || _state_textures.pushed
        || _state_textures.disabled;
}

UiButton::UiButtonVisualMode UiButton::visual_mode() const noexcept
{
    return _visual_mode;
}

void UiButton::set_sounds(const UiButtonSounds& sounds)
{
    _sounds = sounds;
}

void UiButton::clear_sounds()
{
    _sounds = UiButtonSounds{};
}

const UiButtonSounds& UiButton::sounds() const noexcept
{
    return _sounds;
}

void UiButton::set_on_click(ClickCallback on_click)
{
    _on_click = std::move(on_click);
}

void UiButton::prepend_on_click(ClickCallback on_click)
{
    if (!on_click)
        return;
    ClickCallback existing = std::move(_on_click);
    _on_click = [before = std::move(on_click),after = std::move(existing)]()
    {
        before();
        if (after)
            after();
    };
}

void UiButton::set_base_style(const UiButtonStyle& style) noexcept
{
    _style_state.set_base_style(style);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiButton::set_style_overrides(const UiButtonStyleOverrides& overrides)
{
    _style_state.set_style_overrides(overrides);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

const UiButtonStyle& UiButton::style() const noexcept
{
    return _style_state.effective_style();
}

const UiButtonStyleOverrides& UiButton::style_overrides() const noexcept { return _style_state.style_overrides(); }
bool UiButton::has_style_overrides() const noexcept
{
    return _style_state.has_style_overrides();
}

void UiButton::clear_style_overrides() noexcept
{
    _style_state.clear_style_overrides();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiButton::set_visual_role(UiButtonVisualRole role) noexcept
{
    if (_visual_role == role)
        return;
    _visual_role = role;
    notify_base_style_invalidated();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

UiButtonVisualRole UiButton::visual_role() const noexcept
{
    return _visual_role;
}

void UiButton::set_padding(int padding)
{
    _padding = std::max(0,padding);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

int UiButton::padding() const noexcept
{
    return _padding;
}

void UiButton::apply_button_config(const UiButtonConfig& config)
{
    if (config.sounds)
        set_sounds(*config.sounds);
    else
        clear_sounds();

    if (config.style_overrides)
        set_style_overrides(*config.style_overrides);
    else
        clear_style_overrides();
    apply_button_content(config.content);
}

void UiButton::apply_button_content(const UiButtonContent& content)
{
    std::visit([this](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T,std::monostate>)
            clear_content();
        else if constexpr (std::is_same_v<T,UiTextContent>)
            set_text_content(value);
        else if constexpr (std::is_same_v<T,UiButtonIconContent>)
            set_icon_texture(value.texture);
        else if constexpr (std::is_same_v<T,UiButtonTextureSetContent>)
            set_state_textures(value.textures);
    },content);
}

void UiButton::set_icon_texture(SDL_Texture* texture) noexcept
{
    clear_content();
    if (!texture)
    {
        notify_layout_parent_of_intrinsic_layout_invalidation();
        return;
    }

    _state_textures = UiButtonTextures{ texture,texture,texture,texture };
    _visual_mode = UiButtonVisualMode::Icon;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiButton::clear_content() noexcept
{
    _text_content = UiTextContent{};
    _state_textures = UiButtonTextures{};
    _visual_mode = UiButtonVisualMode::None;
}

bool UiButton::can_interact() const noexcept
{
    return is_enabled() && is_focused() && is_active() && is_visible();
}

bool UiButton::can_receive_pointer() const noexcept
{
    return is_enabled() && is_active() && is_visible();
}

elysia::core::Color UiButton::current_background_color() const noexcept
{
    return resolve_interactive_color(style().chrome.background,is_enabled(),is_focused(),_is_pushed);
}

elysia::core::Color UiButton::current_border_color() const noexcept
{
    return resolve_interactive_color(style().chrome.border,is_enabled(),is_focused(),_is_pushed);
}

elysia::core::Color UiButton::current_text_color() const noexcept
{
    return resolve_enabled_disabled_color(style().text,is_enabled());
}

SDL_Texture* UiButton::current_state_texture() const noexcept
{
    if (!is_enabled())
        return _state_textures.disabled ? _state_textures.disabled : _state_textures.idle;
    if (_is_pushed)
        return _state_textures.pushed ? _state_textures.pushed : _state_textures.idle;
    if (is_focused())
        return _state_textures.focused ? _state_textures.focused : _state_textures.idle;
    return _state_textures.idle;
}

elysia::core::Rect UiButton::content_rect() const noexcept
{
    const elysia::core::Rect& button_rect = screen_rect();
    const float width = std::max(0.0f,button_rect.width());
    const float height = std::max(0.0f,button_rect.height());
    const float padding = static_cast<float>(_padding);
    const float pad_x = std::min(padding,width * 0.5f);
    const float pad_y = std::min(padding,height * 0.5f);

    elysia::core::Rect content = button_rect;
    content.set_x(button_rect.x() + pad_x);
    content.set_y(button_rect.y() + pad_y);
    content.set_width(width - pad_x * 2.0f);
    content.set_height(height - pad_y * 2.0f);
    return content;
}

elysia::core::Rect UiButton::text_render_rect(SDL_Texture* text_texture) const noexcept
{
    if (!text_texture)
        return elysia::core::Rect::zero();

    int texture_width = 0;
    int texture_height = 0;
    if (!elysia::core::texture_pixel_size(text_texture,&texture_width,&texture_height))
        return elysia::core::Rect::zero();
    if (texture_width <= 0 || texture_height <= 0)
        return elysia::core::Rect::zero();

    const elysia::core::Rect available_rect = content_rect();
    if (available_rect.is_empty())
        return elysia::core::Rect::zero();

    const float available_width = available_rect.width();
    const float available_height = available_rect.height();
    const float width_scale = available_width / static_cast<float>(texture_width);
    const float height_scale = available_height / static_cast<float>(texture_height);
    const float scale = std::min(1.0f,std::min(width_scale,height_scale));
    const elysia::core::Vector2 render_size(
        static_cast<float>(texture_width) * scale,
        static_cast<float>(texture_height) * scale
    );
    return elysia::core::Rect::from_center(available_rect.center(),render_size);
}

bool UiButton::contains_pointer(int mouse_x,int mouse_y) const noexcept
{
    return presentation_screen_rect().contains(elysia::core::Vector2(static_cast<float>(mouse_x),static_cast<float>(mouse_y)));
}

bool UiButton::is_primary_pointer_event(const UiInputEvent& event) const noexcept
{
    return event.device == elysia::input::InputDevice::Mouse
        && event.control == elysia::input::RawInputControl::MouseLeft;
}

void UiButton::clear_pushed_state() noexcept
{
    _is_pushed = false;
}

void UiButton::play_sound_if_set(std::string_view sound_key) const
{
    if (sound_key.empty())
        return;
    elysia::audio::AudioService::instance()->request_sound(sound_key,{
        .group = elysia::audio::SoundGroup::Ui
    });
}

}

