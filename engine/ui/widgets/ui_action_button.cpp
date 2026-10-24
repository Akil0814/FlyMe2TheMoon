#include "ui_action_button.h"

#include "../../core/render/sdl_texture_size.h"
#include "../../localization/localization_service.h"
#include "../../localization/localized_text_style.h"
#include "../style/ui_style_defaults.h"

#include <algorithm>
#include <cmath>

namespace elysia::ui
{
UiActionButton::UiActionButton(const elysia::core::Rect& rect,int order) : UiElement(rect,order) { reset(); }
UiActionButton::UiActionButton(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order)
    : UiActionButton(elysia::core::Rect(position,size),order) {}
UiActionButton::UiActionButton(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order)
    : UiActionButton(elysia::core::Rect::from_center(center,size),order) {}

void UiActionButton::reset() noexcept
{
    UiElement::reset();
    _content = std::monostate{};
    _key_hint = {};
    _badge_text = {};
    _font_source_override.reset();
    _on_interaction = {};
    _overlay_ratio = 0.0f;
    _enabled = true;
    _selected = _external_pressed = _pointer_pressed = _hovered = false;
    const auto button = UiStyleDefaults::button();
    UiActionButtonStyle initial;
    initial.chrome = button.chrome;
    initial.text = button.text;
    initial.selected_border = button.chrome.border.active;
    _style_state.reset(initial);
}

bool UiActionButton::can_interact() const noexcept
{
    return _enabled && is_visible() && is_active() && !is_destroyed();
}

void UiActionButton::set_enabled(bool enabled)
{
    _enabled = enabled;
    if (!enabled)
        cancel_input_interaction(); // Do not access this after the callback.
}

void UiActionButton::set_overlay_ratio(float ratio) noexcept
{
    _overlay_ratio = std::isfinite(ratio) ? std::clamp(ratio,0.0f,1.0f) : 0.0f;
}

void UiActionButton::cancel_input_interaction()
{
    const bool notify = _pointer_pressed;
    _pointer_pressed = _hovered = _external_pressed = false;
    const auto callback = _on_interaction;
    if (notify && callback)
        callback({ UiActionButtonInteractionPhase::Canceled,false });
}

bool UiActionButton::on_ui_input_event(const UiInputEvent& event)
{
    const bool inside = presentation_screen_rect().contains(
        { static_cast<float>(event.mouse_x),static_cast<float>(event.mouse_y) });
    if (event.type == UiInputEventType::MouseMoved)
    {
        _hovered = can_interact() && inside;
        return false;
    }
    if (event.device != elysia::input::InputDevice::Mouse
        || event.control != elysia::input::RawInputControl::MouseLeft)
        return false;
    if (event.type == UiInputEventType::PointerPressed)
    {
        if (_pointer_pressed)
            return true;
        if (!can_interact() || !inside)
            return false;
        _pointer_pressed = _hovered = true;
        const auto callback = _on_interaction;
        if (callback)
            callback({ UiActionButtonInteractionPhase::Pressed,false });
        return true;
    }
    if (event.type == UiInputEventType::PointerReleased && _pointer_pressed)
    {
        const bool clicked = can_interact() && inside;
        _pointer_pressed = false;
        _hovered = clicked;
        const auto callback = _on_interaction;
        if (callback)
            callback({ UiActionButtonInteractionPhase::Released,clicked });
        return true;
    }
    return false;
}

elysia::core::Rect UiActionButton::content_rect() const noexcept
{
    const auto& rect = screen_rect();
    const float requested = std::isfinite(style().padding) ? std::max(0.0f,style().padding) : 0.0f;
    const float padding = std::min(requested,0.5f * std::max(0.0f,std::min(rect.width(),rect.height())));
    return { rect.x() + padding,rect.y() + padding,rect.width() - 2 * padding,rect.height() - 2 * padding };
}

void UiActionButton::append_text(std::vector<elysia::core::UiRenderCommand>& out,const UiTextContent& text,
    elysia::typography::UiTypographyRole role,TextAnchor anchor) const
{
    if (text.empty())
        return;
    elysia::localization::LocalizedTextStyle text_style;
    text_style.typography_role = role;
    text_style.font_source_override = _font_source_override;
    text_style.color = resolve_enabled_disabled_color(style().text,_enabled);
    auto* service = ELYSIA_LOCALIZATION;
    SDL_Texture* texture = text.kind == UiTextContentKind::TextKey
        ? service->get_text_texture(text.value,text_style)
        : service->get_raw_text_texture(text.value,text_style);
    int width = 0,height = 0;
    if (!texture || !elysia::core::texture_pixel_size(texture,&width,&height) || width <= 0 || height <= 0)
        return;
    const auto area = content_rect();
    if (area.is_empty())
        return;
    const float scale = std::min({ 1.0f,area.width() / width,area.height() / height });
    const elysia::core::Vector2 size{ width * scale,height * scale };
    auto rect = elysia::core::Rect::from_center(area.center(),size);
    if (anchor == TextAnchor::BottomLeft)
        rect = { area.left(),area.bottom() - size.y,size.x,size.y };
    else if (anchor == TextAnchor::TopRight)
        rect = { area.right() - size.x,area.top(),size.x,size.y };
    auto command = elysia::core::make_ui_texture_command(texture,rect);
    apply_opacity(command);
    out.push_back(command);
}

void UiActionButton::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out) const
{
    using namespace elysia::core;
    if (!is_visible() || is_destroyed() || screen_rect().is_empty())
        return;
    const auto& s = style();
    const auto& rect = screen_rect();
    if (s.chrome.draw_background)
        out.push_back(make_ui_fill_rect_command(rect,apply_opacity(resolve_interactive_color(
            s.chrome.background,_enabled,_hovered,is_pressed())),s.chrome.corner_radius));
    if (const auto* text = std::get_if<UiTextContent>(&_content))
        append_text(out,*text,elysia::typography::UiTypographyRole::Button,TextAnchor::Center);
    else if (const auto* icon = std::get_if<UiActionButtonIconContent>(&_content); icon && icon->texture)
    {
        int width = 0,height = 0;
        if (texture_pixel_size(icon->texture,&width,&height))
        {
            const float source_width = icon->source_rect ? icon->source_rect->width() : static_cast<float>(width);
            const float source_height = icon->source_rect ? icon->source_rect->height() : static_cast<float>(height);
            const auto area = content_rect();
            if (source_width > 0 && source_height > 0 && !area.is_empty())
            {
                const float scale = std::min(area.width() / source_width,area.height() / source_height);
                auto command = make_ui_texture_command(icon->texture,Rect::from_center(area.center(),
                    { source_width * scale,source_height * scale }));
                if (icon->source_rect)
                {
                    command.use_src_rect = true;
                    command.src_rect = *icon->source_rect;
                }
                apply_opacity(command);
                out.push_back(command);
            }
        }
    }
    if (_overlay_ratio > 0)
        out.push_back(make_ui_fill_rect_command(
            { rect.x(),rect.bottom() - rect.height() * _overlay_ratio,rect.width(),rect.height() * _overlay_ratio },
            apply_opacity(s.overlay)));
    if (s.chrome.draw_border)
        out.push_back(make_ui_draw_rect_command(rect,apply_opacity(resolve_interactive_color(
            s.chrome.border,_enabled,_hovered,is_pressed())),s.chrome.corner_radius,s.chrome.border_width));
    if (_selected)
        out.push_back(make_ui_draw_rect_command(rect,apply_opacity(s.selected_border),
            s.chrome.corner_radius,s.selected_border_width));
    append_text(out,_key_hint,elysia::typography::UiTypographyRole::Caption,TextAnchor::BottomLeft);
    append_text(out,_badge_text,elysia::typography::UiTypographyRole::Caption,TextAnchor::TopRight);
}
}
