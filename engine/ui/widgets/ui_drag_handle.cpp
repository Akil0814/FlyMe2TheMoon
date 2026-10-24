#include "ui_drag_handle.h"

#include "../style/ui_style_defaults.h"
#include "../../core/render/render_command.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace elysia::ui
{
namespace
{
constexpr float RectEpsilon = 0.0001f;

[[nodiscard]] float clamp_non_negative(float value) noexcept
{
    return std::max(0.0f,value);
}

[[nodiscard]] bool nearly_equal(float a,float b) noexcept
{
    return std::fabs(a - b) <= RectEpsilon;
}

[[nodiscard]] bool same_rect(const elysia::core::Rect& lhs,const elysia::core::Rect& rhs) noexcept
{
    return nearly_equal(lhs.x(),rhs.x())
        && nearly_equal(lhs.y(),rhs.y())
        && nearly_equal(lhs.width(),rhs.width())
        && nearly_equal(lhs.height(),rhs.height());
}
}

UiDragHandle::UiDragHandle(const elysia::core::Rect& rect,int order) noexcept
    : UiControl(rect,order)
{
    reset();
    set_size(rect.size());
}

UiDragHandle::UiDragHandle(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiControl(position,size,order)
{
    reset();
    set_size(size);
}

UiDragHandle::UiDragHandle(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiControl(center,size,from_center,order)
{
    reset();
    set_size(size);
}

UiDragHandle::UiDragHandle(const elysia::core::Rect& rect,const UiDragHandleConfig& config,int order) noexcept
    : UiDragHandle(rect,order)
{
    set_drag_handle_config(config);
}

UiDragHandle::UiDragHandle(const elysia::core::Vector2& position,const elysia::core::Vector2& size,const UiDragHandleConfig& config,int order) noexcept
    : UiDragHandle(elysia::core::Rect(position.x,position.y,size.x,size.y),config,order) {}

UiDragHandle::UiDragHandle(
    const elysia::core::Vector2& center,const elysia::core::Vector2& size,
    UiFromCenterTag,const UiDragHandleConfig& config,int order
) noexcept : UiDragHandle(elysia::core::Rect::from_center(center,size),config,order) {}

void UiDragHandle::reset() noexcept
{
    UiControl::reset();
    _config = UiDragHandleConfig{};
    _style_state.reset(UiStyleDefaults::drag_handle());
    _on_dragged = nullptr;
    _on_drag_ended = nullptr;
    _grab_offset = elysia::core::Vector2{};
    _is_dragging = false;
    set_size(_style_state.effective_style().size);
}

void UiDragHandle::set_enabled(bool enabled)
{
    UiControl::set_enabled(enabled);
    if (!enabled)
        cancel_drag();
}

void UiDragHandle::set_focused(bool focused)
{
    UiControl::set_focused(focused);
}

bool UiDragHandle::on_ui_input_event(const UiInputEvent& event)
{
    if (event.type == UiInputEventType::MouseMoved)
    {
        if (!can_receive_pointer())
        {
            set_focused(false);
            cancel_drag();
            return false;
        }

        if (_is_dragging)
        {
            set_focused(true);
            (void)drag_to_pointer(event.mouse_x,event.mouse_y);
            return true;
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

        begin_drag_session(elysia::core::Vector2(static_cast<float>(event.mouse_x),static_cast<float>(event.mouse_y)));
        return true;
    }

    if (event.type == UiInputEventType::PointerReleased)
    {
        if (!is_primary_pointer_event(event))
            return false;

        const bool was_dragging = _is_dragging;
        const elysia::core::Vector2 center = screen_rect().center();
        const bool is_inside = can_receive_pointer() && contains_pointer(event.mouse_x,event.mouse_y);
        _is_dragging = false;
        set_focused(is_inside);
        const UiDragHandleDragEndedCallback callback = was_dragging ? _on_drag_ended : nullptr;
        if (callback)
            callback(center);
        return was_dragging;
    }

    return false;
}

void UiDragHandle::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;

    const elysia::core::Rect& rect = screen_rect();
    if (rect.is_empty())
        return;

    SDL_Texture* texture = current_state_texture();
    if (texture)
    {
        elysia::core::UiRenderCommand command = elysia::core::make_ui_texture_command(texture,rect);
        apply_opacity(command);
        out_commands.push_back(command);
    }
    else if (style().chrome.draw_background)
    {
        out_commands.push_back(elysia::core::make_ui_fill_rect_command(rect,apply_opacity(current_background_color()),style().chrome.corner_radius));
    }

    if (style().chrome.draw_border)
        out_commands.push_back(elysia::core::make_ui_draw_rect_command(
            rect,
            apply_opacity(current_border_color()),
            style().chrome.corner_radius,
            style().chrome.border_width));
}

void UiDragHandle::set_drag_handle_config(const UiDragHandleConfig& config)
{
    apply_drag_handle_config(config);
}

const UiDragHandleConfig& UiDragHandle::drag_handle_config() const noexcept
{
    return _config;
}

void UiDragHandle::set_base_style(const UiDragHandleStyle& style) noexcept
{
    _style_state.set_base_style(style);
}

void UiDragHandle::set_style_overrides(const UiDragHandleStyleOverrides& overrides)
{
    _style_state.set_style_overrides(overrides);
    const auto& resolved = _style_state.effective_style();
    set_size(elysia::core::Vector2(clamp_non_negative(resolved.size.x),clamp_non_negative(resolved.size.y)));
}

const UiDragHandleStyle& UiDragHandle::style() const noexcept
{
    return _style_state.effective_style();
}

const UiDragHandleStyleOverrides& UiDragHandle::style_overrides() const noexcept { return _style_state.style_overrides(); }
bool UiDragHandle::has_style_overrides() const noexcept
{
    return _style_state.has_style_overrides();
}

void UiDragHandle::clear_style_overrides() noexcept
{
    _style_state.clear_style_overrides();
    set_size(_style_state.effective_style().size);
}

void UiDragHandle::set_drag_axis(UiDragAxis axis) noexcept
{
    _config.axis = axis;
}

UiDragAxis UiDragHandle::drag_axis() const noexcept
{
    return _config.axis;
}

void UiDragHandle::set_drag_bounds(const elysia::core::Rect& bounds) noexcept
{
    _config.drag_bounds = bounds;
}

void UiDragHandle::clear_drag_bounds() noexcept
{
    _config.drag_bounds.reset();
}

const std::optional<elysia::core::Rect>& UiDragHandle::drag_bounds() const noexcept
{
    return _config.drag_bounds;
}

void UiDragHandle::set_on_dragged(UiDragHandleDraggedCallback on_dragged)
{
    _on_dragged = std::move(on_dragged);
}

void UiDragHandle::set_on_drag_ended(UiDragHandleDragEndedCallback on_drag_ended)
{
    _on_drag_ended = std::move(on_drag_ended);
}

void UiDragHandle::begin_drag_from_pointer(const elysia::core::Vector2& pointer) noexcept
{
    if (!can_receive_pointer() || screen_rect().is_empty())
        return;
    begin_drag_session(pointer);
}

void UiDragHandle::cancel_drag() noexcept
{
    _is_dragging = false;
}

bool UiDragHandle::is_dragging() const noexcept
{
    return _is_dragging;
}

void UiDragHandle::apply_drag_handle_config(const UiDragHandleConfig& config)
{
    _config = config;
    _style_state.set_style_overrides(config.style_overrides);
    const auto& resolved = _style_state.effective_style();
    set_size(elysia::core::Vector2(clamp_non_negative(resolved.size.x),clamp_non_negative(resolved.size.y)));
}

bool UiDragHandle::can_receive_pointer() const noexcept
{
    return is_enabled() && is_active() && is_visible();
}

bool UiDragHandle::contains_pointer(int mouse_x,int mouse_y) const noexcept
{
    return presentation_screen_rect().contains(elysia::core::Vector2(static_cast<float>(mouse_x),static_cast<float>(mouse_y)));
}

bool UiDragHandle::is_primary_pointer_event(const UiInputEvent& event) const noexcept
{
    return event.device == elysia::input::InputDevice::Mouse && event.control == elysia::input::RawInputControl::MouseLeft;
}

bool UiDragHandle::drag_to_pointer(int mouse_x,int mouse_y)
{
    const elysia::core::Vector2 layout_pointer = presentation_to_layout_point(
        elysia::core::Vector2(static_cast<float>(mouse_x),static_cast<float>(mouse_y)));
    elysia::core::Rect next_rect = screen_rect();
    if (_config.axis == UiDragAxis::Horizontal || _config.axis == UiDragAxis::Free)
        next_rect.set_x(layout_pointer.x - _grab_offset.x);
    if (_config.axis == UiDragAxis::Vertical || _config.axis == UiDragAxis::Free)
        next_rect.set_y(layout_pointer.y - _grab_offset.y);
    next_rect = clamped_rect(next_rect);
    if (same_rect(next_rect,screen_rect()))
        return false;

    set_screen_rect(next_rect);
    const UiDragHandleDraggedCallback callback = _on_dragged;
    const elysia::core::Vector2 center = next_rect.center();
    if (callback)
        callback(center);
    return true;
}

void UiDragHandle::begin_drag_session(const elysia::core::Vector2& pointer) noexcept
{
    const elysia::core::Rect& rect = screen_rect();
    const elysia::core::Vector2 layout_pointer = presentation_to_layout_point(pointer);
    set_focused(true);
    _is_dragging = true;
    _grab_offset = elysia::core::Vector2(layout_pointer.x - rect.x(),layout_pointer.y - rect.y());
}

elysia::core::Rect UiDragHandle::clamped_rect(const elysia::core::Rect& rect) const noexcept
{
    if (!_config.drag_bounds)
        return rect;

    elysia::core::Rect clamped = rect;
    const elysia::core::Rect& bounds = *_config.drag_bounds;

    if (_config.axis == UiDragAxis::Horizontal || _config.axis == UiDragAxis::Free)
    {
        const float min_x = bounds.x();
        const float max_x = std::max(min_x,bounds.right() - clamped.width());
        clamped.set_x(std::clamp(clamped.x(),min_x,max_x));
    }

    if (_config.axis == UiDragAxis::Vertical || _config.axis == UiDragAxis::Free)
    {
        const float min_y = bounds.y();
        const float max_y = std::max(min_y,bounds.bottom() - clamped.height());
        clamped.set_y(std::clamp(clamped.y(),min_y,max_y));
    }

    return clamped;
}

SDL_Texture* UiDragHandle::current_state_texture() const noexcept
{
    if (!style().textures)
        return nullptr;

    const UiDragHandleTextures& textures = *style().textures;
    if (!is_enabled())
        return textures.disabled ? textures.disabled : textures.idle;
    if (_is_dragging)
        return textures.dragging ? textures.dragging : (textures.focused ? textures.focused : textures.idle);
    if (is_focused())
        return textures.focused ? textures.focused : textures.idle;
    return textures.idle;
}

elysia::core::Color UiDragHandle::current_background_color() const noexcept
{
    return resolve_interactive_color(style().chrome.background,is_enabled(),is_focused(),_is_dragging);
}

elysia::core::Color UiDragHandle::current_border_color() const noexcept
{
    return resolve_interactive_color(style().chrome.border,is_enabled(),is_focused(),_is_dragging);
}

}
