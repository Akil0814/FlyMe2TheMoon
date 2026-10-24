#include "ui_window.h"
#include "../composites/ui_tooltip.h"

#include "../style/ui_style_defaults.h"
#include "../focus/ui_focus_scope_utils.h"
#include "../focus/ui_control_focus_scope_host.h"
#include "../containers/ui_scroll_container.h"
#include "../layout/ui_anchor_layout.h"
#include "../layout/ui_layout_geometry.h"
#include "../../core/render/render_command.h"

#include <algorithm>

namespace elysia::ui
{
namespace
{
[[nodiscard]] bool contains_child_element(const UiElement& root,const UiElement* target) noexcept
{
    if (&root == target)
        return true;

    const auto* child_host = dynamic_cast<const UiChildHost*>(&root);
    if (!child_host)
        return false;

    for (std::size_t index = 0; index < child_host->child_count(); ++index)
    {
        const UiElement* child = child_host->child_at(index);
        if (child && contains_child_element(*child,target))
            return true;
    }
    return false;
}

[[nodiscard]] bool is_primary_mouse_press(const UiInputEvent& event) noexcept
{
    return event.type == UiInputEventType::PointerPressed
        && event.device == elysia::input::InputDevice::Mouse
        && event.control == elysia::input::RawInputControl::MouseLeft;
}

void collect_focused_scroll_containers(UiElement& element,std::vector<UiScrollContainer*>& out_containers)
{
    if (element.is_destroyed() || !element.is_active() || !element.is_visible())
        return;

    if (auto* child_host = dynamic_cast<UiChildHost*>(&element))
    {
        for (std::size_t index = 0; index < child_host->child_count(); ++index)
        {
            if (UiElement* child = child_host->child_at(index))
                collect_focused_scroll_containers(*child,out_containers);
        }
    }

    if (auto* scroll = dynamic_cast<UiScrollContainer*>(&element); scroll && scroll->is_scope_focused())
        out_containers.push_back(scroll);
}

void collect_scroll_containers(UiElement& element,std::vector<UiScrollContainer*>& out_containers)
{
    if (element.is_destroyed() || !element.is_active() || !element.is_visible())
        return;

    if (auto* child_host = dynamic_cast<UiChildHost*>(&element))
    {
        for (std::size_t index = 0; index < child_host->child_count(); ++index)
        {
            if (UiElement* child = child_host->child_at(index))
                collect_scroll_containers(*child,out_containers);
        }
    }

    if (auto* scroll = dynamic_cast<UiScrollContainer*>(&element))
        out_containers.push_back(scroll);
}

[[nodiscard]] bool is_pointer_scroll_target_event(const UiInputEvent& event) noexcept
{
    return event.device == elysia::input::InputDevice::Mouse
        && (event.type == UiInputEventType::PointerPressed || event.type == UiInputEventType::MouseWheel);
}

[[nodiscard]] bool is_passive_scroll_action(const UiInputEvent& event) noexcept
{
    if (event.type != UiInputEventType::ActionPressed)
        return false;
    return event.action == UiAction::PageUp || event.action == UiAction::PageDown
        || event.action == UiAction::Home || event.action == UiAction::End;
}
}

UiWindow::UiWindow(const elysia::core::Rect& rect,int order) noexcept : UiChildHost(rect,order)
{
    reset();
}
UiWindow::UiWindow(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept : UiChildHost(position,size,order)
{
    reset();
}
UiWindow::UiWindow(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag tag,int order) noexcept : UiChildHost(center,size,tag,order)
{
    reset();
}

UiWindow::~UiWindow()
{
    detach_window_registrations();
}

void UiWindow::reset() noexcept
{
    detach_window_registrations();
    UiChildHost::reset();
    _scope_entries.clear();
    _overlay_entries.clear();
    _transient_popups.clear();
    _active_transient_popup = nullptr;
    _focused_scope = nullptr;
    _last_focused_scope = nullptr;
    _gamepad_scroll_target = nullptr;
    _style_state.reset(UiStyleDefaults::window());
    _hover_focus_enabled = true;
    _transient_popup_pointer_active = false;
    _focus_input_device = elysia::input::InputDevice::Unknown;
    _on_cancel = {};
}

void UiWindow::set_base_style(const UiWindowStyle& style) noexcept
{
    _style_state.set_base_style(style);
}

void UiWindow::set_style_overrides(const UiWindowStyleOverrides& overrides) noexcept
{
    _style_state.set_style_overrides(overrides);
}

const UiWindowStyle& UiWindow::style() const noexcept
{
    return _style_state.effective_style();
}

const UiWindowStyleOverrides& UiWindow::style_overrides() const noexcept { return _style_state.style_overrides(); }
bool UiWindow::has_style_overrides() const noexcept
{
    return _style_state.has_style_overrides();
}

void UiWindow::clear_style_overrides() noexcept
{
    _style_state.clear_style_overrides();
}

void UiWindow::set_hover_focus_enabled(bool enabled) noexcept
{
    _hover_focus_enabled = enabled;
}

bool UiWindow::hover_focus_enabled() const noexcept
{
    return _hover_focus_enabled;
}

void UiWindow::set_on_cancel(UiWindowCancelCallback on_cancel)
{
    _on_cancel = std::move(on_cancel);
}

void UiWindow::register_focus_scope(UiFocusScope& scope,const UiFocusScopeNeighbors& neighbors)
{
    auto found = std::find_if(_scope_entries.begin(),_scope_entries.end(),[&scope](const ScopeEntry& entry)
    {
        return entry.scope == &scope;
    });
    if (found != _scope_entries.end())
        found->neighbors = neighbors;
    else
        _scope_entries.push_back(ScopeEntry{ &scope,neighbors });
    ensure_valid_scope_focus();
    apply_scope_focus();
}

void UiWindow::unregister_focus_scope(UiFocusScope& scope)
{
    _scope_entries.erase(std::remove_if(_scope_entries.begin(),_scope_entries.end(),[&scope](const ScopeEntry& entry)
    {
        return entry.scope == &scope;
    }),_scope_entries.end());
    if (_focused_scope == &scope)
        _focused_scope = nullptr;
    if (_last_focused_scope == &scope)
        _last_focused_scope = nullptr;
    ensure_valid_scope_focus();
    apply_scope_focus();
}

void UiWindow::set_scope_neighbors(UiFocusScope& scope,const UiFocusScopeNeighbors& neighbors)
{
    auto found = std::find_if(_scope_entries.begin(),_scope_entries.end(),[&scope](const ScopeEntry& entry)
    {
        return entry.scope == &scope;
    });
    if (found == _scope_entries.end())
    {
        register_focus_scope(scope,neighbors);
        return;
    }
    found->neighbors = neighbors;
}

void UiWindow::set_focused_scope(UiFocusScope* scope)
{
    (void)set_focused_scope_internal(scope);
    ensure_valid_scope_focus();
    apply_scope_focus();
}

UiFocusScope* UiWindow::focused_scope() const noexcept
{
    return _focused_scope;
}

const UiScrollContainer* UiWindow::gamepad_scroll_target() const noexcept
{
    if (!_gamepad_scroll_target || !is_live_child_element(_gamepad_scroll_target))
        return nullptr;
    return _gamepad_scroll_target->is_passive_scroll_target_usable() ? _gamepad_scroll_target : nullptr;
}

bool UiWindow::focus_first_available_scope()
{
    cleanup_destroyed_children();
    update_layout_if_dirty();
    prune_focus_scopes();
    for (const ScopeEntry& entry : _scope_entries)
    {
        if (is_scope_usable(entry.scope))
        {
            (void)set_focused_scope_internal(entry.scope);
            apply_scope_focus();
            return true;
        }
    }
    _focused_scope = nullptr;
    apply_scope_focus();
    return false;
}

bool UiWindow::register_overlay(UiElement& element,UiOverlayOptions options)
{
    if (element.is_destroyed() || element.layout_parent() != this)
        return false;
    if (OverlayEntry* entry = find_overlay(element))
    {
        const bool was_open = entry->options.open;
        entry->options = options;
        if (!options.open)
            entry->restore_focus_scope = nullptr;
        element.set_order(options.order);
        sync_overlay_visibility(*entry);
        mark_layout_dirty();
        if (options.open)
        {
            if (!was_open)
                remember_overlay_restore_focus(*entry);
            update_layout_if_dirty();
            (void)focus_overlay(*entry);
        }
    }
    else
    {
        _overlay_entries.push_back(OverlayEntry{
            &element,dynamic_cast<UiOverlayWindowClient*>(&element),options });
        OverlayEntry& added = _overlay_entries.back();
        element.set_order(options.order);
        sync_overlay_visibility(added);
        mark_layout_dirty();
        if (options.open)
        {
            remember_overlay_restore_focus(added);
            update_layout_if_dirty();
            (void)focus_overlay(added);
        }
    }

    prune_focus_scopes();
    ensure_valid_scope_focus();
    apply_scope_focus();
    return true;
}

void UiWindow::unregister_overlay(UiElement& element)
{
    UiOverlayWindowClient* client = nullptr;
    const bool registered = std::any_of(_overlay_entries.begin(),_overlay_entries.end(),[&](const OverlayEntry& entry)
    {
        if (entry.element == &element)
            client = entry.client;
        return entry.element == &element;
    });
    _overlay_entries.erase(std::remove_if(_overlay_entries.begin(),_overlay_entries.end(),[&element](const OverlayEntry& entry)
    {
        return entry.element == &element;
    }),_overlay_entries.end());
    if (registered)
    {
        if (client)
            client->on_window_detached(*this);
    }
}

void UiWindow::open_overlay(UiElement& element)
{
    set_overlay_open_state(element,true);
}

void UiWindow::close_overlay(UiElement& element)
{
    set_overlay_open_state(element,false);
}

void UiWindow::set_overlay_open_state(UiElement& element,bool open)
{
    OverlayEntry* entry = find_overlay(element);
    if (!entry)
        return;

    const bool was_open = entry->options.open;
    if (open && !was_open)
        remember_overlay_restore_focus(*entry);

    entry->options.open = open;
    sync_overlay_visibility(*entry);
    mark_layout_dirty();
    update_layout_if_dirty();

    if (open)
    {
        (void)focus_overlay(*entry);
    }
    else if (!restore_focus_after_overlay_close(*entry))
    {
        if (OverlayEntry* active = active_overlay())
            (void)focus_overlay(*active);
    }

    prune_focus_scopes();
    ensure_valid_scope_focus();
    apply_scope_focus();
}

bool UiWindow::is_overlay_open(const UiElement& element) const noexcept
{
    const OverlayEntry* entry = find_overlay(element);
    return entry && entry->options.open;
}

UiOverlayOptions* UiWindow::overlay_options(UiElement& element) noexcept
{
    OverlayEntry* entry = find_overlay(element);
    return entry ? &entry->options : nullptr;
}

void UiWindow::register_transient_popup(UiTransientPopup& popup)
{
    const auto found = std::find_if(_transient_popups.begin(),_transient_popups.end(),[&popup](const TransientPopupEntry& entry)
    {
        return entry.popup == &popup;
    });
    if (found == _transient_popups.end())
        _transient_popups.push_back(TransientPopupEntry{ &popup,&popup.popup_owner() });
}

void UiWindow::unregister_transient_popup(UiTransientPopup& popup)
{
    const bool registered = std::any_of(_transient_popups.begin(),_transient_popups.end(),[&popup](const TransientPopupEntry& entry)
    {
        return entry.popup == &popup;
    });
    _transient_popups.erase(std::remove_if(_transient_popups.begin(),_transient_popups.end(),[&popup](const TransientPopupEntry& entry)
    {
        return entry.popup == &popup;
    }),_transient_popups.end());
    if (_active_transient_popup == &popup)
    {
        _active_transient_popup = nullptr;
        _transient_popup_pointer_active = false;
    }
    if (registered)
        popup.on_window_detached(*this);
}

void UiWindow::activate_transient_popup(UiTransientPopup& popup)
{
    register_transient_popup(popup);
    if (_active_transient_popup && _active_transient_popup != &popup)
        _active_transient_popup->close();
    _active_transient_popup = &popup;
    _transient_popup_pointer_active = false;
}

void UiWindow::register_tooltip(UiTooltip& tooltip)
{
    if (std::find(_tooltips.begin(),_tooltips.end(),&tooltip) == _tooltips.end())
        _tooltips.push_back(&tooltip);
    if (UiElement* content = tooltip.content())
        attach_tooltip_content(*content);
}

void UiWindow::unregister_tooltip(UiTooltip& tooltip) noexcept
{
    if (UiElement* content = tooltip.content())
        detach_tooltip_content(*content);
    _tooltips.erase(std::remove(_tooltips.begin(),_tooltips.end(),&tooltip),_tooltips.end());
    if (tooltip._window == this)
        tooltip._window = nullptr;
}

void UiWindow::attach_tooltip_content(UiElement& content)
{
    attach_external_style_subtree(content);
}

void UiWindow::detach_tooltip_content(UiElement& content) noexcept
{
    detach_external_style_subtree(content);
}

elysia::core::Rect UiWindow::tooltip_trigger_visible_rect(const UiElement& trigger) const noexcept
{
    if (trigger.is_destroyed() || !trigger.is_visible() || !trigger.is_active())
        return elysia::core::Rect::zero();

    elysia::core::Rect visible_rect = trigger.presentation_screen_rect().intersection(
        content_rect().translated(accumulated_presentation_translation()));
    if (visible_rect.is_empty())
        return elysia::core::Rect::zero();

    const UiChildHost* ancestor = trigger.layout_parent();
    while (ancestor)
    {
        if (ancestor->is_destroyed() || !ancestor->is_visible() || !ancestor->is_active())
            return elysia::core::Rect::zero();

        if (ancestor->clips_children())
        {
            visible_rect = visible_rect.intersection(
                ancestor->content_rect().translated(ancestor->accumulated_presentation_translation()));
            if (visible_rect.is_empty())
                return elysia::core::Rect::zero();
        }

        if (ancestor == this)
            return visible_rect;
        ancestor = ancestor->layout_parent();
    }

    return elysia::core::Rect::zero();
}

bool UiWindow::tooltip_pointer_reaches_trigger(
    const UiElement& trigger,
    int mouse_x,
    int mouse_y
) const noexcept
{
    const elysia::core::Rect visible_rect = tooltip_trigger_visible_rect(trigger);
    const elysia::core::Vector2 pointer(
        static_cast<float>(mouse_x),
        static_cast<float>(mouse_y));
    if (visible_rect.is_empty() || !visible_rect.contains(pointer))
        return false;

    const UiTransientPopup* popup = active_transient_popup();
    if (popup && popup->is_open() && popup->contains_popup_point(mouse_x,mouse_y))
        return false;

    if (const OverlayEntry* modal = active_modal_overlay();
        modal && modal->element && !contains_child_element(*modal->element,&trigger))
    {
        return false;
    }

    if (const OverlayEntry* overlay = active_overlay();
        overlay && overlay->element && !overlay->options.modal
        && !contains_child_element(*overlay->element,&trigger)
        && contains_overlay_point(*overlay,mouse_x,mouse_y))
    {
        return false;
    }

    return true;
}

bool UiWindow::tooltip_focus_reaches_trigger(const UiElement& trigger) const noexcept
{
    // Mouse focus mirrors pointer hover. Requiring the pointer path prevents a
    // clipped or occluded control's local focus state from bypassing hit testing.
    if (_focus_input_device == elysia::input::InputDevice::Mouse)
        return false;

    const elysia::core::Rect visible_rect = tooltip_trigger_visible_rect(trigger);
    if (visible_rect.is_empty())
        return false;

    if (const UiTransientPopup* popup = active_transient_popup(); popup && popup->is_open())
        return false;

    if (const OverlayEntry* modal = active_modal_overlay();
        modal && modal->element && !contains_child_element(*modal->element,&trigger))
    {
        return false;
    }

    if (const OverlayEntry* overlay = active_overlay();
        overlay && overlay->element && !overlay->options.modal
        && !contains_child_element(*overlay->element,&trigger))
    {
        const elysia::core::Rect overlay_rect = overlay->element->presentation_screen_rect();
        if (overlay_rect.is_empty() || overlay_rect.intersects(visible_rect))
            return false;
    }

    return true;
}

elysia::core::Rect UiWindow::content_bounds() const noexcept
{
    return content_rect();
}

void UiWindow::update(double delta)
{
    cleanup_destroyed_children();
    prune_overlays();
    prune_transient_popups();
    prune_tooltips();
    prune_gamepad_scroll_target();
    sync_overlay_visibility_all();
    update_layout_if_dirty();
    prune_focus_scopes();
    ensure_valid_scope_focus();
    apply_scope_focus();
    update_child_objects(delta);
    cleanup_destroyed_children();
    prune_overlays();
    prune_transient_popups();
    prune_tooltips();
    prune_gamepad_scroll_target();
    sync_overlay_visibility_all();
    prune_focus_scopes();
    ensure_valid_scope_focus();
    apply_scope_focus();
}

void UiWindow::on_ui_input_frame(const UiInputFrame& input)
{
    cleanup_destroyed_children();
    prune_overlays();
    prune_transient_popups();
    prune_tooltips();
    prune_gamepad_scroll_target();
    sync_overlay_visibility_all();
    update_layout_if_dirty();
    prune_focus_scopes();
    ensure_valid_scope_focus();
    apply_scope_focus();

    if (OverlayEntry* overlay = active_modal_overlay())
    {
        sync_overlay_visibility(*overlay);
        (void)focus_overlay(*overlay);
        apply_scope_focus();
        if (UiInputFrameReceiver* receiver = dynamic_cast<UiInputFrameReceiver*>(overlay->element))
            receiver->on_ui_input_frame(input);
        cleanup_destroyed_children();
        prune_overlays();
        prune_focus_scopes();
        ensure_valid_scope_focus();
        apply_scope_focus();
        return;
    }

    dispatch_frame_to_children(input);
    cleanup_destroyed_children();
    prune_focus_scopes();
    ensure_valid_scope_focus();
    apply_scope_focus();
}

bool UiWindow::on_ui_input_event(const UiInputEvent& event)
{
    if (event.type == UiInputEventType::MouseMoved
        || event.type == UiInputEventType::PointerPressed
        || event.type == UiInputEventType::PointerReleased
        || event.type == UiInputEventType::MouseWheel)
    {
        for (UiTooltip* tooltip : _tooltips)
            if (tooltip)
                tooltip->observe_pointer(event.mouse_x,event.mouse_y);
    }
    update_focus_input_device(event.device);
    cleanup_destroyed_children();
    prune_overlays();
    prune_transient_popups();
    sync_overlay_visibility_all();
    update_layout_if_dirty();
    prune_focus_scopes();
    ensure_valid_scope_focus();
    apply_scope_focus();

    if (OverlayEntry* overlay = active_modal_overlay())
    {
        if (is_pointer_scroll_target_event(event))
            promote_scroll_target_at(*overlay->element,event.mouse_x,event.mouse_y);
        (void)focus_overlay(*overlay);
        apply_scope_focus();

        const bool blocks_background_input = overlay->options.modal;
        if (event.type == UiInputEventType::ActionPressed && event.action == UiAction::Cancel)
        {
            // A focused editor gets the first chance to end its local adjustment.
            UiElement* const element = overlay->element;
            const bool close_on_cancel = overlay->options.close_on_cancel;
            if (!dispatch_to_overlay(*overlay,event) && close_on_cancel && is_live_child_element(element))
                close_overlay(*element);
            cleanup_destroyed_children();
            prune_overlays();
            prune_focus_scopes();
            ensure_valid_scope_focus();
            apply_scope_focus();
            return true;
        }
        if (should_close_overlay_from_event(*overlay,event))
        {
            close_overlay(*overlay->element);
            return true;
        }

        if (dispatch_gamepad_scroll_to_focused_containers(*overlay->element,event))
            return true;

        if (dispatch_passive_scroll_input(*overlay->element,event))
            return true;

        const bool handled = dispatch_to_overlay(*overlay,event);
        cleanup_destroyed_children();
        prune_overlays();
        prune_focus_scopes();
        ensure_valid_scope_focus();
        apply_scope_focus();
        return handled || blocks_background_input;
    }

    if (UiTransientPopup* popup = active_transient_popup())
    {
        const bool pointer_event = event.type == UiInputEventType::MouseMoved
            || event.type == UiInputEventType::PointerPressed
            || event.type == UiInputEventType::PointerReleased
            || event.type == UiInputEventType::MouseWheel;
        const bool contains_pointer = pointer_event
            && popup->contains_popup_point(event.mouse_x,event.mouse_y);

        if (is_primary_mouse_press(event) && !contains_pointer)
        {
            popup->close();
            _active_transient_popup = nullptr;
            _transient_popup_pointer_active = false;
        }
        else if (!pointer_event || contains_pointer || _transient_popup_pointer_active)
        {
            const bool handled = dispatch_to_transient_popup(*popup,event);
            if (event.type == UiInputEventType::PointerPressed && contains_pointer)
                _transient_popup_pointer_active = true;
            if (event.type == UiInputEventType::PointerReleased)
                _transient_popup_pointer_active = false;

            if (handled || contains_pointer || _transient_popup_pointer_active)
                return true;
        }
    }

    if (OverlayEntry* overlay = active_overlay())
    {
        if (event.type == UiInputEventType::ActionPressed && event.action == UiAction::Cancel)
        {
            UiElement* const element = overlay->element;
            const bool close_on_cancel = overlay->options.close_on_cancel;
            if (!dispatch_to_overlay(*overlay,event) && close_on_cancel && is_live_child_element(element))
                close_overlay(*element);
            cleanup_destroyed_children();
            prune_overlays();
            prune_focus_scopes();
            ensure_valid_scope_focus();
            apply_scope_focus();
            return true;
        }
        if (is_pointer_scroll_target_event(event))
            promote_scroll_target_at(*overlay->element,event.mouse_x,event.mouse_y);
        if (dispatch_gamepad_scroll_to_focused_containers(*overlay->element,event))
            return true;

        if (dispatch_passive_scroll_input(*overlay->element,event))
            return true;

        if (should_close_overlay_from_event(*overlay,event))
        {
            close_overlay(*overlay->element);
            return true;
        }

        if (dispatch_to_overlay(*overlay,event))
        {
            cleanup_destroyed_children();
            prune_overlays();
            prune_focus_scopes();
            ensure_valid_scope_focus();
            apply_scope_focus();
            return true;
        }
    }

    bool handled = false;

    if (is_pointer_scroll_target_event(event))
        promote_scroll_target_at(*this,event.mouse_x,event.mouse_y);

    if (_focused_scope
        && dispatch_gamepad_scroll_to_focused_containers(_focused_scope->focus_scope_element(),event))
    {
        return true;
    }

    if (event.type == UiInputEventType::MouseWheel
        && event.device == elysia::input::InputDevice::Gamepad
        && dispatch_passive_scroll_input(*this,event))
    {
        return true;
    }

    if (event.type == UiInputEventType::ActionPressed && event.action == UiAction::Cancel)
    {
        if (dispatch_to_scope(_focused_scope,event))
        {
            cleanup_destroyed_children();
            prune_focus_scopes();
            ensure_valid_scope_focus();
            apply_scope_focus();
            return true;
        }
        const UiWindowCancelCallback callback = _on_cancel;
        if (callback)
            callback();
        return static_cast<bool>(callback);
    }
    else
    {
        UiFocusScope* pointer_scope = nullptr;
        if (event.type == UiInputEventType::MouseMoved
            || event.type == UiInputEventType::PointerPressed
            || event.type == UiInputEventType::PointerReleased
            || event.type == UiInputEventType::MouseWheel)
        {
            pointer_scope = find_registered_scope_at(event.mouse_x,event.mouse_y);
        }

        if ((event.type == UiInputEventType::MouseMoved || event.type == UiInputEventType::MouseWheel) && _hover_focus_enabled)
            (void)set_focused_scope_internal(pointer_scope);
        else if (event.type == UiInputEventType::PointerPressed
            && event.device == elysia::input::InputDevice::Mouse
            && event.control == elysia::input::RawInputControl::MouseLeft)
        {
            (void)set_focused_scope_internal(pointer_scope);
            // Activate the new scope before its controls process the press.
            // Otherwise its focus synchronization clears the button's pushed state.
            apply_scope_focus();
        }

        if (event.type == UiInputEventType::ActionPressed && is_navigation_action(event.action))
        {
            handled = dispatch_to_scope(_focused_scope,event);
            if (!handled && _focused_scope)
            {
                if (UiFocusScope* neighbor = find_neighbor(*_focused_scope,event.action))
                {
                    _focused_scope = neighbor;
                    if (!_focused_scope->focused_target())
                        (void)_focused_scope->focus_first_available();
                    handled = true;
                }
            }
        }
        else if ((event.action == UiAction::Confirm || event.action == UiAction::Cancel)
            && (event.type == UiInputEventType::ActionPressed || event.type == UiInputEventType::ActionReleased))
        {
            handled = dispatch_to_scope(_focused_scope,event);
        }
        else
        {
            UiFocusScope* event_scope = pointer_scope ? pointer_scope : _focused_scope;
            if (event_scope)
                handled = dispatch_to_scope(event_scope,event);
            if (!handled && is_passive_scroll_action(event))
                handled = dispatch_passive_scroll_input(*this,event);
            if (!handled)
                handled = dispatch_input_to_children(event);
        }
    }

    cleanup_destroyed_children();
    prune_overlays();
    prune_focus_scopes();
    ensure_valid_scope_focus();
    apply_scope_focus();
    return handled;
}

void UiWindow::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;
    auto* self = const_cast<UiWindow*>(this);
    self->cleanup_destroyed_children();
    self->prune_overlays();
    self->prune_transient_popups();
    self->prune_tooltips();
    self->prune_gamepad_scroll_target();
    self->sync_overlay_visibility_all();
    self->update_layout_if_dirty();
    self->prune_focus_scopes();
    self->ensure_valid_scope_focus();
    self->apply_scope_focus();

    const UiWindowStyle& style = _style_state.effective_style();
    if (style.draw_background)
        out_commands.push_back(elysia::core::make_ui_fill_rect_command(screen_rect(),apply_opacity(style.background),style.corner_radius));
    submit_child_render_commands(out_commands);
    if (!active_modal_overlay())
        submit_active_transient_popup_render_commands(out_commands);
    submit_tooltip_render_commands(out_commands);
    if (style.draw_border)
        out_commands.push_back(elysia::core::make_ui_draw_rect_command(
            screen_rect(),apply_opacity(style.border),style.corner_radius,style.border_width));
}

void UiWindow::rebuild_layout()
{
    layout::layout_anchored_children(children(),content_rect());
    apply_overlay_placements();
}

void UiWindow::prune_focus_scopes()
{
    // Temporary visibility/activity changes must not discard registrations or neighbors.
    std::vector<const UiFocusScope*> live_scopes;
    live_scopes.reserve(_scope_entries.size());
    collect_live_scopes(*this,live_scopes);
    _scope_entries.erase(std::remove_if(_scope_entries.begin(),_scope_entries.end(),[&live_scopes](const ScopeEntry& entry)
    {
        return !entry.scope || !contains_scope(live_scopes,entry.scope);
    }),_scope_entries.end());
    if (_focused_scope && !contains_scope(live_scopes,_focused_scope))
        _focused_scope = nullptr;
    if (_last_focused_scope && !contains_scope(live_scopes,_last_focused_scope))
        _last_focused_scope = nullptr;
}

void UiWindow::prune_overlays()
{
    _overlay_entries.erase(std::remove_if(_overlay_entries.begin(),_overlay_entries.end(),[this](const OverlayEntry& entry)
    {
        if (!entry.element || !is_live_child_element(entry.element))
        {
            if (entry.client)
                entry.client->on_window_detached(*this);
            return true;
        }
        if (!entry.element->is_destroyed())
            return false;
        if (entry.client)
            entry.client->on_window_detached(*this);
        return true;
    }),_overlay_entries.end());
}

void UiWindow::prune_transient_popups()
{
    _transient_popups.erase(std::remove_if(_transient_popups.begin(),_transient_popups.end(),[this](const TransientPopupEntry& entry)
    {
        if (!entry.popup)
            return true;
        if (!entry.owner || !is_live_child_element(entry.owner))
        {
            entry.popup->on_window_detached(*this);
            return true;
        }
        if (!entry.owner->is_destroyed())
            return false;
        entry.popup->on_window_detached(*this);
        return true;
    }),_transient_popups.end());

    if (_active_transient_popup
        && std::none_of(_transient_popups.begin(),_transient_popups.end(),[this](const TransientPopupEntry& entry)
        {
            return entry.popup == _active_transient_popup;
        }))
    {
        _active_transient_popup = nullptr;
        _transient_popup_pointer_active = false;
    }
}

void UiWindow::prune_tooltips()
{
    for (UiTooltip* tooltip : _tooltips)
    {
        if (tooltip && is_live_child_element(tooltip)
            && tooltip->trigger() && !is_live_child_element(tooltip->trigger()))
            tooltip->clear_trigger();
    }
    _tooltips.erase(std::remove_if(_tooltips.begin(),_tooltips.end(),[this](UiTooltip* tooltip)
    {
        if (!tooltip)
            return true;
        if (!is_live_child_element(tooltip))
        {
            if (UiElement* content = tooltip->content())
                detach_tooltip_content(*content);
            if (tooltip->_window == this)
                tooltip->_window = nullptr;
            return true;
        }
        return tooltip->is_destroyed();
    }),_tooltips.end());
}

void UiWindow::prune_gamepad_scroll_target() noexcept
{
    if (!_gamepad_scroll_target)
        return;
    if (!is_live_child_element(_gamepad_scroll_target)
        || !_gamepad_scroll_target->is_passive_scroll_target_usable())
    {
        _gamepad_scroll_target = nullptr;
    }
}

void UiWindow::detach_window_registrations() noexcept
{
    for (OverlayEntry& entry : _overlay_entries)
    {
        if (entry.client)
            entry.client->on_window_detached(*this);
    }
    _overlay_entries.clear();

    for (TransientPopupEntry& entry : _transient_popups)
    {
        if (entry.popup)
            entry.popup->on_window_detached(*this);
    }
    _transient_popups.clear();
    _active_transient_popup = nullptr;
    _transient_popup_pointer_active = false;
    _gamepad_scroll_target = nullptr;

    for (UiTooltip* tooltip : _tooltips)
    {
        if (!tooltip)
            continue;
        if (UiElement* content = tooltip->content())
            detach_tooltip_content(*content);
        if (tooltip->_window == this)
            tooltip->_window = nullptr;
    }
    _tooltips.clear();
}

void UiWindow::ensure_valid_scope_focus()
{
    if (_focused_scope && is_registered_scope(*_focused_scope) && is_scope_usable(_focused_scope))
        return;

    _focused_scope = nullptr;
    if (uses_pointer_focus_policy(_focus_input_device))
        return;

    if (restore_preferred_scope_focus())
        return;

    (void)focus_first_available_scope();
}

void UiWindow::apply_scope_focus()
{
    for (const ScopeEntry& entry : _scope_entries)
    {
        if (!entry.scope)
            continue;
        entry.scope->set_scope_focused(entry.scope == _focused_scope && is_scope_usable(entry.scope));
    }
}

UiFocusScope* UiWindow::find_registered_scope_at(int mouse_x,int mouse_y) const
{
    for (std::size_t index = _scope_entries.size(); index > 0; --index)
    {
        UiFocusScope* scope = _scope_entries[index - 1].scope;
        if (!is_scope_usable(scope))
            continue;
        if (scope->contains_focus_point(mouse_x,mouse_y))
            return scope;
    }
    return nullptr;
}

UiFocusScope* UiWindow::find_neighbor(const UiFocusScope& scope,UiAction action) const
{
    auto found = std::find_if(_scope_entries.begin(),_scope_entries.end(),[&scope](const ScopeEntry& entry)
    {
        return entry.scope == &scope;
    });
    if (found == _scope_entries.end())
        return nullptr;

    UiFocusScope* candidate = nullptr;
    switch (action)
    {
    case UiAction::NavigateLeft:
        candidate = found->neighbors.left;
        break;
    case UiAction::NavigateRight:
        candidate = found->neighbors.right;
        break;
    case UiAction::NavigateUp:
        candidate = found->neighbors.up;
        break;
    case UiAction::NavigateDown:
        candidate = found->neighbors.down;
        break;
    default:
        break;
    }
    return candidate && is_registered_scope(*candidate) && is_scope_usable(candidate) ? candidate : nullptr;
}

bool UiWindow::is_registered_scope(const UiFocusScope& scope) const noexcept
{
    return std::any_of(_scope_entries.begin(),_scope_entries.end(),[&scope](const ScopeEntry& entry)
    {
        return entry.scope == &scope;
    });
}

bool UiWindow::set_focused_scope_internal(UiFocusScope* scope) noexcept
{
    if (!scope)
    {
        _focused_scope = nullptr;
        return true;
    }
    if (!is_registered_scope(*scope) || !is_scope_usable(scope))
        return false;
    _focused_scope = scope;
    _last_focused_scope = scope;
    if (!_focused_scope->focused_target())
        (void)_focused_scope->focus_first_available();
    return true;
}

bool UiWindow::dispatch_to_scope(UiFocusScope* scope,const UiInputEvent& event) const
{
    if (!is_scope_usable(scope))
        return false;
    if (auto* receiver = dynamic_cast<UiInputEventReceiver*>(&scope->focus_scope_element()))
        return receiver->on_ui_input_event(event);
    return false;
}

bool UiWindow::is_scope_usable(const UiFocusScope* scope) noexcept
{
    if (!scope)
        return false;
    // A registered scope can remain owned while any ancestor makes it unavailable.
    for (const UiElement* element = &scope->focus_scope_element();
         element; element = element->layout_parent())
    {
        if (element->is_destroyed() || !element->is_active() || !element->is_visible())
            return false;
    }
    return scope->has_focusable_target();
}

void UiWindow::update_focus_input_device(elysia::input::InputDevice device) noexcept
{
    if (device != elysia::input::InputDevice::Unknown)
        _focus_input_device = device;
}

bool UiWindow::restore_preferred_scope_focus()
{
    if (_last_focused_scope && is_registered_scope(*_last_focused_scope) && is_scope_usable(_last_focused_scope))
        return set_focused_scope_internal(_last_focused_scope);
    return false;
}

UiWindow::OverlayEntry* UiWindow::active_overlay() noexcept
{
    OverlayEntry* top_overlay = nullptr;
    for (std::size_t index = _overlay_entries.size(); index > 0; --index)
    {
        OverlayEntry& entry = _overlay_entries[index - 1];
        if (!entry.element || !entry.options.open
            || entry.element->is_destroyed())
        {
            continue;
        }

        if (!top_overlay || entry.element->order() > top_overlay->element->order())
            top_overlay = &entry;
    }
    return top_overlay;
}

const UiWindow::OverlayEntry* UiWindow::active_overlay() const noexcept
{
    const OverlayEntry* top_overlay = nullptr;
    for (std::size_t index = _overlay_entries.size(); index > 0; --index)
    {
        const OverlayEntry& entry = _overlay_entries[index - 1];
        if (!entry.element || !entry.options.open
            || entry.element->is_destroyed())
        {
            continue;
        }

        if (!top_overlay || entry.element->order() > top_overlay->element->order())
            top_overlay = &entry;
    }
    return top_overlay;
}

UiWindow::OverlayEntry* UiWindow::active_modal_overlay() noexcept
{
    OverlayEntry* top_overlay = nullptr;
    for (std::size_t index = _overlay_entries.size(); index > 0; --index)
    {
        OverlayEntry& entry = _overlay_entries[index - 1];
        if (!entry.element || !entry.options.open || !entry.options.modal
            || entry.element->is_destroyed())
        {
            continue;
        }

        if (!top_overlay || entry.element->order() > top_overlay->element->order())
            top_overlay = &entry;
    }
    return top_overlay;
}

const UiWindow::OverlayEntry* UiWindow::active_modal_overlay() const noexcept
{
    const OverlayEntry* top_overlay = nullptr;
    for (std::size_t index = _overlay_entries.size(); index > 0; --index)
    {
        const OverlayEntry& entry = _overlay_entries[index - 1];
        if (!entry.element || !entry.options.open || !entry.options.modal
            || entry.element->is_destroyed())
        {
            continue;
        }

        if (!top_overlay || entry.element->order() > top_overlay->element->order())
            top_overlay = &entry;
    }
    return top_overlay;
}

UiWindow::OverlayEntry* UiWindow::find_overlay(UiElement& element) noexcept
{
    auto found = std::find_if(_overlay_entries.begin(),_overlay_entries.end(),[&element](const OverlayEntry& entry)
    {
        return entry.element == &element;
    });
    return found != _overlay_entries.end() ? &(*found) : nullptr;
}

const UiWindow::OverlayEntry* UiWindow::find_overlay(const UiElement& element) const noexcept
{
    auto found = std::find_if(_overlay_entries.begin(),_overlay_entries.end(),[&element](const OverlayEntry& entry)
    {
        return entry.element == &element;
    });
    return found != _overlay_entries.end() ? &(*found) : nullptr;
}

UiFocusScope* UiWindow::overlay_focus_scope(OverlayEntry& entry) noexcept
{
    return entry.element ? dynamic_cast<UiFocusScope*>(entry.element) : nullptr;
}

const UiFocusScope* UiWindow::overlay_focus_scope(const OverlayEntry& entry) const noexcept
{
    return entry.element ? dynamic_cast<const UiFocusScope*>(entry.element) : nullptr;
}

void UiWindow::remember_overlay_restore_focus(OverlayEntry& entry) noexcept
{
    UiFocusScope* overlay_scope = overlay_focus_scope(entry);
    if (_focused_scope && _focused_scope != overlay_scope && is_registered_scope(*_focused_scope) && is_scope_usable(_focused_scope))
        entry.restore_focus_scope = _focused_scope;
}

bool UiWindow::restore_focus_after_overlay_close(OverlayEntry& entry)
{
    UiFocusScope* restore_scope = entry.restore_focus_scope;
    entry.restore_focus_scope = nullptr;
    if (restore_scope && is_registered_scope(*restore_scope) && is_scope_usable(restore_scope))
        return set_focused_scope_internal(restore_scope);
    return false;
}

bool UiWindow::focus_overlay(OverlayEntry& entry)
{
    UiFocusScope* scope = overlay_focus_scope(entry);
    if (!scope)
        return false;

    const bool pointer_focus = uses_pointer_focus_policy(_focus_input_device);
    auto* host = dynamic_cast<UiControlFocusScopeHost*>(scope);

    if (_focused_scope == scope && is_scope_usable(scope))
    {
        if (!pointer_focus && !scope->focused_target())
            return scope->focus_first_available();
        return true;
    }

    register_focus_scope(*scope);
    if (!scope->has_focusable_target())
        return false;

    const bool focused = set_focused_scope_internal(scope);
    if (focused && pointer_focus && host)
        host->set_focused_target(nullptr);
    apply_scope_focus();
    if (focused && !pointer_focus && !scope->focused_target())
        return scope->focus_first_available();
    return focused;
}

bool UiWindow::dispatch_to_overlay(OverlayEntry& entry,const UiInputEvent& event)
{
    if (!entry.element || entry.element->is_destroyed() || !entry.options.open)
        return false;

    sync_overlay_visibility(entry);

    if (UiInputEventReceiver* receiver = dynamic_cast<UiInputEventReceiver*>(entry.element))
        return receiver->on_ui_input_event(event);

    return false;
}

void UiWindow::sync_overlay_visibility(OverlayEntry& entry) noexcept
{
    if (!entry.element)
        return;
    entry.element->set_visible(entry.options.open);
    entry.element->set_active(entry.options.open);
}

void UiWindow::sync_overlay_visibility_all() noexcept
{
    for (OverlayEntry& entry : _overlay_entries)
        sync_overlay_visibility(entry);
}

void UiWindow::apply_overlay_placements() noexcept
{
    for (OverlayEntry& entry : _overlay_entries)
        apply_overlay_placement(entry);
}

void UiWindow::apply_overlay_placement(OverlayEntry& entry) noexcept
{
    if (!entry.element || !entry.options.open)
        return;

    const elysia::core::Rect bounds = content_rect();
    const elysia::core::Vector2 fallback = layout::clamp_size(entry.options.fallback_size);
    const elysia::core::Vector2 current = layout::clamp_size(entry.element->size());
    const float width = current.x > elysia::core::Vector2::kEpsilon ? current.x : fallback.x;
    const float height = current.y > elysia::core::Vector2::kEpsilon ? current.y : fallback.y;

    switch (entry.options.placement)
    {
    case UiOverlayPlacement::LeftDrawer:
        entry.element->set_screen_rect(elysia::core::Rect(bounds.left(),bounds.top(),width,bounds.height()));
        break;
    case UiOverlayPlacement::RightDrawer:
        entry.element->set_screen_rect(elysia::core::Rect(bounds.right() - width,bounds.top(),width,bounds.height()));
        break;
    case UiOverlayPlacement::TopSheet:
        entry.element->set_screen_rect(elysia::core::Rect(bounds.left(),bounds.top(),bounds.width(),height));
        break;
    case UiOverlayPlacement::BottomSheet:
        entry.element->set_screen_rect(elysia::core::Rect(bounds.left(),bounds.bottom() - height,bounds.width(),height));
        break;
    case UiOverlayPlacement::Center:
    default:
        entry.element->set_screen_rect(elysia::core::Rect::from_center(bounds.center(),elysia::core::Vector2(width,height)));
        break;
    }
}

bool UiWindow::should_close_overlay_from_event(const OverlayEntry& entry,const UiInputEvent& event) const noexcept
{
    if (entry.options.close_on_cancel && event.type == UiInputEventType::ActionPressed && event.action == UiAction::Cancel)
        return true;
    if (entry.options.close_on_outside_click && is_primary_mouse_press(event))
        return !contains_overlay_point(entry,event.mouse_x,event.mouse_y);
    return false;
}

bool UiWindow::contains_overlay_point(const OverlayEntry& entry,int mouse_x,int mouse_y) const noexcept
{
    if (!entry.element)
        return false;
    if (entry.element->screen_rect().is_empty())
        return true;
    return entry.element->presentation_screen_rect().contains(
        elysia::core::Vector2(static_cast<float>(mouse_x),static_cast<float>(mouse_y)));
}

UiTransientPopup* UiWindow::active_transient_popup() noexcept
{
    return _active_transient_popup && _active_transient_popup->is_open()
        ? _active_transient_popup
        : nullptr;
}

const UiTransientPopup* UiWindow::active_transient_popup() const noexcept
{
    return _active_transient_popup && _active_transient_popup->is_open()
        ? _active_transient_popup
        : nullptr;
}

bool UiWindow::dispatch_to_transient_popup(UiTransientPopup& popup,const UiInputEvent& event)
{
    return popup.is_open() && popup.on_popup_input_event(event);
}

bool UiWindow::dispatch_gamepad_scroll_to_focused_containers(UiElement& root,const UiInputEvent& event)
{
    if (event.type != UiInputEventType::MouseWheel || event.device != elysia::input::InputDevice::Gamepad)
        return false;

    std::vector<UiScrollContainer*> containers;
    collect_focused_scroll_containers(root,containers);
    for (UiScrollContainer* container : containers)
    {
        if (container && container->is_passive_scroll_target_usable())
        {
            (void)container->on_ui_input_event(event);
            return true;
        }
    }
    return false;
}

bool UiWindow::dispatch_passive_scroll_input(UiElement& root,const UiInputEvent& event)
{
    const bool gamepad_wheel = event.type == UiInputEventType::MouseWheel
        && event.device == elysia::input::InputDevice::Gamepad;
    if (!gamepad_wheel && !is_passive_scroll_action(event))
        return false;

    if (is_passive_scroll_action(event))
    {
        std::vector<UiScrollContainer*> focused_containers;
        collect_focused_scroll_containers(root,focused_containers);
        for (UiScrollContainer* container : focused_containers)
        {
            if (container && container->is_passive_scroll_target_usable())
            {
                (void)container->handle_passive_scroll_input(event);
                return true;
            }
        }
    }

    UiScrollContainer* target = resolve_passive_scroll_target(root);
    if (!target)
        return false;

    (void)target->handle_passive_scroll_input(event);
    return true;
}

void UiWindow::promote_scroll_target_at(UiElement& root,int mouse_x,int mouse_y) noexcept
{
    std::vector<UiScrollContainer*> containers;
    collect_scroll_containers(root,containers);
    for (UiScrollContainer* container : containers)
    {
        if (container && container->is_passive_scroll_target_usable()
            && container->screen_rect().contains({ static_cast<float>(mouse_x),static_cast<float>(mouse_y) }))
        {
            _gamepad_scroll_target = container;
            return;
        }
    }
}

UiScrollContainer* UiWindow::resolve_passive_scroll_target(UiElement& root) noexcept
{
    prune_gamepad_scroll_target();
    if (_gamepad_scroll_target
        && contains_child_element(root,_gamepad_scroll_target)
        && _gamepad_scroll_target->is_passive_scroll_target_usable())
    {
        return _gamepad_scroll_target;
    }

    std::vector<UiScrollContainer*> containers;
    collect_scroll_containers(root,containers);
    for (UiScrollContainer* container : containers)
    {
        if (container && container->is_passive_scroll_target_usable())
        {
            _gamepad_scroll_target = container;
            return container;
        }
    }
    return nullptr;
}

void UiWindow::submit_active_transient_popup_render_commands(
    std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    const UiTransientPopup* popup = active_transient_popup();
    if (!popup)
        return;

    const std::size_t begin = out_commands.size();
    popup->submit_popup_render_commands(out_commands);
    apply_clip_to_range(out_commands,begin,content_rect());
}

void UiWindow::submit_tooltip_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    for (const UiTooltip* tooltip : _tooltips)
    {
        if (!tooltip)
            continue;
        const std::size_t begin = out_commands.size();
        tooltip->submit_tooltip_render_commands(out_commands);
        apply_clip_to_range(out_commands,begin,content_rect());
    }
}

bool UiWindow::is_live_child_element(const UiElement* element) const noexcept
{
    return contains_child_element(*this,element);
}

bool UiWindow::uses_pointer_focus_policy(elysia::input::InputDevice device) noexcept
{
    return device == elysia::input::InputDevice::Mouse;
}

}

elysia::input::InputCapture elysia::ui::UiWindow::input_capture() const noexcept
{
    if (active_modal_overlay() || active_transient_popup())
        return elysia::input::AllInputCapture;
    return UiChildHost::input_capture();
}
