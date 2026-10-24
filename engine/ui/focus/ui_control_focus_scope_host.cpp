#include "ui_control_focus_scope_host.h"

#include <algorithm>

namespace elysia::ui
{
UiControlFocusScopeHost::UiControlFocusScopeHost(const elysia::core::Rect& rect,int order) noexcept
    : UiChildHost(rect,order) {}

UiControlFocusScopeHost::UiControlFocusScopeHost(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiChildHost(position,size,order) {}

UiControlFocusScopeHost::UiControlFocusScopeHost(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiChildHost(center,size,from_center,order) {}

void UiControlFocusScopeHost::reset() noexcept
{
    UiChildHost::reset();
    _focus_entries.clear();
    _focused_target = nullptr;
    _last_focused_target = nullptr;
    _scope_focused = false;
    _gamepad_scroll_focus_suppressed = false;
    _focus_input_device = elysia::input::InputDevice::Unknown;
}

void UiControlFocusScopeHost::set_focused_target(UiControl* control)
{
    cleanup_destroyed_children();
    update_layout_if_dirty();
    refresh_focus_registry();
    (void)set_focused_target_internal(control);
    ensure_valid_focus();
    apply_focus_state();
}

UiControl* UiControlFocusScopeHost::focused_target() const noexcept
{
    return _focused_target;
}

bool UiControlFocusScopeHost::focus_first_available()
{
    cleanup_destroyed_children();
    update_layout_if_dirty();
    refresh_focus_registry();

    if (_gamepad_scroll_focus_suppressed)
        return false;

    for (const FocusEntry& entry : _focus_entries)
    {
        if (is_control_usable(entry.control))
        {
            (void)set_focused_target_internal(entry.control);
            apply_focus_state();
            return true;
        }
    }

    _focused_target = nullptr;
    apply_focus_state();
    return false;
}

bool UiControlFocusScopeHost::has_focusable_target() const noexcept
{
    auto* self = const_cast<UiControlFocusScopeHost*>(this);
    self->cleanup_destroyed_children();
    self->update_layout_if_dirty();
    self->refresh_focus_registry();
    return std::any_of(_focus_entries.begin(),_focus_entries.end(),[](const FocusEntry& entry)
    {
        return is_control_usable(entry.control);
    });
}

bool UiControlFocusScopeHost::can_navigate(UiAction action) const noexcept
{
    if (!is_navigation_action(action))
        return false;

    auto* self = const_cast<UiControlFocusScopeHost*>(this);
    self->cleanup_destroyed_children();
    self->update_layout_if_dirty();
    self->refresh_focus_registry();
    self->ensure_valid_focus();
    self->apply_focus_state();

    if (!is_control_usable(_focused_target))
        return false;

    return find_neighbor(*_focused_target,action) != nullptr;
}

bool UiControlFocusScopeHost::clear_focus_for_gamepad_scroll()
{
    cleanup_destroyed_children();
    update_layout_if_dirty();
    refresh_focus_registry();
    ensure_valid_focus();

    if (!is_control_usable(_focused_target))
    {
        apply_focus_state();
        return false;
    }

    _gamepad_scroll_focus_suppressed = true;
    _focused_target = nullptr;
    apply_focus_state();
    return true;
}

bool UiControlFocusScopeHost::restore_focus_after_gamepad_scroll()
{
    if (!_gamepad_scroll_focus_suppressed)
        return false;

    _gamepad_scroll_focus_suppressed = false;
    cleanup_destroyed_children();
    update_layout_if_dirty();
    refresh_focus_registry();
    ensure_valid_focus();
    apply_focus_state();
    return _focused_target != nullptr;
}

UiElement& UiControlFocusScopeHost::focus_scope_element() noexcept
{
    return *this;
}

const UiElement& UiControlFocusScopeHost::focus_scope_element() const noexcept
{
    return *this;
}

void UiControlFocusScopeHost::set_scope_focused(bool focused) noexcept
{
    _scope_focused = focused;
    apply_focus_state();
}

bool UiControlFocusScopeHost::is_scope_focused() const noexcept
{
    return _scope_focused;
}

bool UiControlFocusScopeHost::contains_focus_point(int mouse_x,int mouse_y) const noexcept
{
    if (!has_focusable_target() || is_destroyed() || !is_active() || !is_visible())
        return false;
    return presentation_screen_rect().contains(elysia::core::Vector2(static_cast<float>(mouse_x),static_cast<float>(mouse_y)));
}

void UiControlFocusScopeHost::update(double delta)
{
    synchronize_focus_state();
    update_child_objects(delta);
    synchronize_focus_state();
}

void UiControlFocusScopeHost::on_ui_input_frame(const UiInputFrame& input)
{
    synchronize_focus_state();
    dispatch_frame_to_children(input);
    synchronize_focus_state();
}

bool UiControlFocusScopeHost::on_ui_input_event(const UiInputEvent& event)
{
    update_focus_input_device(event.device);
    synchronize_focus_state();

    bool handled = false;

    if (event.type == UiInputEventType::MouseMoved)
    {
        (void)set_focused_target_internal(find_registered_target_at(event.mouse_x,event.mouse_y));
        handled = dispatch_input_to_children(event);
    }
    else if (event.type == UiInputEventType::MouseWheel)
    {
        if (event.device == elysia::input::InputDevice::Mouse)
            (void)set_focused_target_internal(find_registered_target_at(event.mouse_x,event.mouse_y));
        handled = dispatch_input_to_children(event);
    }
    else if (event.type == UiInputEventType::PointerPressed)
    {
        if (event.device == elysia::input::InputDevice::Mouse && event.control == elysia::input::RawInputControl::MouseLeft)
        {
            (void)set_focused_target_internal(find_registered_target_at(event.mouse_x,event.mouse_y));
        }
        handled = dispatch_input_to_children(event);
    }
    else if ((event.action == UiAction::Confirm || event.action == UiAction::Cancel)
        && (event.type == UiInputEventType::ActionPressed || event.type == UiInputEventType::ActionReleased))
    {
        if (is_control_usable(_focused_target))
        {
            if (auto* receiver = dynamic_cast<UiInputEventReceiver*>(_focused_target))
                handled = receiver->on_ui_input_event(event);
        }
    }
    else if (event.type == UiInputEventType::ActionPressed && is_navigation_action(event.action))
    {
        if (is_control_usable(_focused_target))
        {
            if (auto* receiver = dynamic_cast<UiInputEventReceiver*>(_focused_target))
                handled = receiver->on_ui_input_event(event);
        }
        if (!handled && _focused_target)
        {
            if (UiControl* neighbor = find_neighbor(*_focused_target,event.action))
            {
                _focused_target = neighbor;
                handled = true;
            }
        }
    }
    else
    {
        handled = dispatch_input_to_children(event);
    }

    synchronize_focus_state();
    return handled;
}

void UiControlFocusScopeHost::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;
    auto* self = const_cast<UiControlFocusScopeHost*>(this);
    self->synchronize_focus_state();
    UiChildHost::submit_ui_render_commands(out_commands);
}

void UiControlFocusScopeHost::synchronize_focus_state()
{
    cleanup_destroyed_children();
    update_layout_if_dirty();
    refresh_focus_registry();
    ensure_valid_focus();
    apply_focus_state();
}

void UiControlFocusScopeHost::refresh_focus_registry()
{
    rebuild_focus_registry();
}

void UiControlFocusScopeHost::set_focus_entries(std::vector<FocusEntry> entries)
{
    _focus_entries = std::move(entries);
}

const std::vector<UiControlFocusScopeHost::FocusEntry>& UiControlFocusScopeHost::focus_entries() const noexcept
{
    return _focus_entries;
}

std::vector<UiControlFocusScopeHost::FocusEntry>& UiControlFocusScopeHost::focus_entries() noexcept
{
    return _focus_entries;
}

std::vector<UiControl*> UiControlFocusScopeHost::direct_focusable_children() const
{
    std::vector<UiControl*> controls;
    controls.reserve(child_count());
    for (const ChildEntry& entry : children())
    {
        if (!entry.element)
            continue;
        if (UiControl* control = dynamic_cast<UiControl*>(entry.element.get()))
            controls.push_back(control);
    }
    return controls;
}

void UiControlFocusScopeHost::ensure_valid_focus()
{
    if (_gamepad_scroll_focus_suppressed)
    {
        _focused_target = nullptr;
        return;
    }

    if (_focused_target && is_registered_focus_target(_focused_target) && is_control_usable(_focused_target))
        return;

    _focused_target = nullptr;
    if (uses_pointer_focus_policy(_focus_input_device))
        return;

    if (restore_preferred_focus_target())
        return;

    (void)focus_first_available();
}

void UiControlFocusScopeHost::apply_focus_state() const
{
    for (const FocusEntry& entry : _focus_entries)
    {
        if (!entry.control)
            continue;
        entry.control->set_focused(_scope_focused && entry.control == _focused_target && is_control_usable(entry.control));
    }
}

UiControl* UiControlFocusScopeHost::find_registered_target_at(int mouse_x,int mouse_y) const
{
    const elysia::core::Vector2 point(static_cast<float>(mouse_x),static_cast<float>(mouse_y));
    for (std::size_t index = _focus_entries.size(); index > 0; --index)
    {
        UiControl* control = _focus_entries[index - 1].control;
        if (!is_control_usable(control))
            continue;
        if (control->presentation_screen_rect().contains(point))
            return control;
    }
    return nullptr;
}

bool UiControlFocusScopeHost::is_registered_focus_target(const UiControl* control) const noexcept
{
    if (!control)
        return false;
    return std::any_of(_focus_entries.begin(),_focus_entries.end(),[&control](const FocusEntry& entry)
    {
        return entry.control == control;
    });
}

bool UiControlFocusScopeHost::set_focused_target_internal(UiControl* control) noexcept
{
    if (!control)
    {
        _focused_target = nullptr;
        return true;
    }
    if (!is_registered_focus_target(control) || !is_control_usable(control))
        return false;
    _focused_target = control;
    _last_focused_target = control;
    return true;
}

UiControl* UiControlFocusScopeHost::find_neighbor(const UiControl& control,UiAction action) const noexcept
{
    auto found = std::find_if(_focus_entries.begin(),_focus_entries.end(),[&control](const FocusEntry& entry)
    {
        return entry.control == &control;
    });
    if (found == _focus_entries.end())
        return nullptr;

    UiControl* candidate = nullptr;
    switch (action)
    {
    case UiAction::NavigateLeft:
        candidate = found->neighbors.left.value_or(nullptr);
        break;
    case UiAction::NavigateRight:
        candidate = found->neighbors.right.value_or(nullptr);
        break;
    case UiAction::NavigateUp:
        candidate = found->neighbors.up.value_or(nullptr);
        break;
    case UiAction::NavigateDown:
        candidate = found->neighbors.down.value_or(nullptr);
        break;
    default:
        return nullptr;
    }

    return candidate && is_registered_focus_target(candidate) && is_control_usable(candidate) ? candidate : nullptr;
}

void UiControlFocusScopeHost::update_focus_input_device(elysia::input::InputDevice device) noexcept
{
    if (device != elysia::input::InputDevice::Unknown)
        _focus_input_device = device;
}

bool UiControlFocusScopeHost::restore_preferred_focus_target()
{
    if (_last_focused_target && is_registered_focus_target(_last_focused_target) && is_control_usable(_last_focused_target))
        return set_focused_target_internal(_last_focused_target);
    return false;
}

bool UiControlFocusScopeHost::uses_pointer_focus_policy(elysia::input::InputDevice device) noexcept
{
    return device == elysia::input::InputDevice::Mouse;
}
}

