#include "ui_list_container.h"

#include "../focus/ui_focus_scope_utils.h"

#include <algorithm>

namespace elysia::ui
{
namespace
{
[[nodiscard]] UiLayoutDirection to_layout_direction(UiListDirection direction) noexcept
{
    return direction == UiListDirection::Vertical ? UiLayoutDirection::Vertical : UiLayoutDirection::Horizontal;
}

[[nodiscard]] bool is_mouse_position_event(const UiInputEvent& event) noexcept
{
    if (event.device != elysia::input::InputDevice::Mouse)
        return false;

    switch (event.type)
    {
    case UiInputEventType::MouseMoved:
    case UiInputEventType::PointerPressed:
    case UiInputEventType::PointerReleased:
    case UiInputEventType::MouseWheel:
        return true;
    default:
        return false;
    }
}
}

UiListContainer::UiListContainer(const elysia::core::Rect& rect,int order) noexcept
    : UiControlFocusScopeHost(rect,order) {}

UiListContainer::UiListContainer(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiControlFocusScopeHost(position,size,order) {}

UiListContainer::UiListContainer(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiControlFocusScopeHost(center,size,from_center,order) {}

void UiListContainer::reset() noexcept
{
    UiControlFocusScopeHost::reset();
    reset_delegated_focus_state();
    _layout = layout::UiListLayoutConfig{};
}

void UiListContainer::set_scope_focused(bool focused) noexcept
{
    UiControlFocusScopeHost::set_scope_focused(focused);
    sync_child_scope_focus();
}

bool UiListContainer::focus_first_available()
{
    cleanup_destroyed_children();
    update_layout_if_dirty();
    refresh_focus_registry();

    const std::vector<UiElement*> regions = delegated_focus_regions(*this);
    for (UiElement* region : regions)
    {
        if (enter_delegated_region(*this,region))
        {
            sync_child_scope_focus();
            apply_focus_state();
            return true;
        }
    }

    // Use the internal setter: the public setter validates focus by calling this method,
    // which would recurse indefinitely when the list has no focusable children.
    (void)set_focused_target_internal(nullptr);
    sync_delegated_scope_focus(static_cast<UiElement*>(nullptr),is_scope_focused(),regions);
    apply_focus_state();
    return false;
}

bool UiListContainer::can_navigate(UiAction action) const noexcept
{
    if (!is_navigation_action(action))
        return false;

    auto* self = const_cast<UiListContainer*>(this);
    self->cleanup_destroyed_children();
    self->update_layout_if_dirty();
    self->refresh_focus_registry();
    self->ensure_valid_focus();
    self->sync_child_scope_focus();
    self->apply_focus_state();

    const UiControl* target = UiControlFocusScopeHost::focused_target();
    if (!target)
        return false;

    if (const UiFocusScope* scope = delegated_owner_scope_of(target))
    {
        if (scope->can_navigate(action))
            return true;
    }

    return neighbor_region_of(target,action) != nullptr;
}

void UiListContainer::update(double delta)
{
    sync_child_scope_focus();
    UiControlFocusScopeHost::update(delta);
    sync_host_delegated_focus_target(*this);
    sync_child_scope_focus();
}

void UiListContainer::on_ui_input_frame(const UiInputFrame& input)
{
    sync_child_scope_focus();
    UiControlFocusScopeHost::on_ui_input_frame(input);
    sync_host_delegated_focus_target(*this);
    sync_child_scope_focus();
}

bool UiListContainer::on_ui_input_event(const UiInputEvent& event)
{
    update_focus_input_device(event.device);
    cleanup_destroyed_children();
    update_layout_if_dirty();
    refresh_focus_registry();
    ensure_valid_focus();

    if (is_mouse_position_event(event))
    {
        UiControl* target_at_pointer = find_registered_target_at(event.mouse_x,event.mouse_y);
        if (event.type == UiInputEventType::PointerPressed && event.control == elysia::input::RawInputControl::MouseLeft)
            (void)set_focused_target_internal(target_at_pointer);
        sync_delegated_owner_scope_target(target_at_pointer);
    }

    sync_child_scope_focus();
    apply_focus_state();

    UiControl* target = UiControlFocusScopeHost::focused_target();
    UiFocusScope* scope = delegated_owner_scope_of(target);

    if ((event.action == UiAction::Confirm || event.action == UiAction::Cancel)
        && (event.type == UiInputEventType::ActionPressed || event.type == UiInputEventType::ActionReleased))
    {
        bool handled = false;
        if (scope)
        {
            if (auto* receiver = dynamic_cast<UiInputEventReceiver*>(&scope->focus_scope_element()))
                handled = receiver->on_ui_input_event(event);
        }
        else if (target)
        {
            if (auto* receiver = dynamic_cast<UiInputEventReceiver*>(target))
                handled = receiver->on_ui_input_event(event);
        }

        cleanup_destroyed_children();
        refresh_focus_registry();
        ensure_valid_focus();
        sync_host_delegated_focus_target(*this);
        sync_child_scope_focus();
        apply_focus_state();
        return handled;
    }

    if (event.type == UiInputEventType::ActionPressed && is_navigation_action(event.action))
    {
        bool handled = false;
        if (scope)
        {
            if (auto* receiver = dynamic_cast<UiInputEventReceiver*>(&scope->focus_scope_element()))
                handled = receiver->on_ui_input_event(event);
        }
        else if (!scope && target)
        {
            if (auto* receiver = dynamic_cast<UiInputEventReceiver*>(target))
                handled = receiver->on_ui_input_event(event);
        }

        cleanup_destroyed_children();
        refresh_focus_registry();
        ensure_valid_focus();
        sync_host_delegated_focus_target(*this);
        sync_child_scope_focus();
        apply_focus_state();

        if (!handled)
        {
            target = UiControlFocusScopeHost::focused_target();
            if (UiElement* neighbor_region = neighbor_region_of(target,event.action))
                handled = enter_delegated_region(*this,neighbor_region);
        }

        cleanup_destroyed_children();
        refresh_focus_registry();
        ensure_valid_focus();
        sync_delegated_owner_scope_target(UiControlFocusScopeHost::focused_target());
        sync_child_scope_focus();
        apply_focus_state();
        return handled;
    }

    const bool handled = UiControlFocusScopeHost::on_ui_input_event(event);
    sync_host_delegated_focus_target(*this);
    sync_child_scope_focus();
    return handled;
}

void UiListContainer::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;

    auto* self = const_cast<UiListContainer*>(this);
    self->cleanup_destroyed_children();
    self->update_layout_if_dirty();
    self->refresh_focus_registry();
    self->ensure_valid_focus();
    self->sync_host_delegated_focus_target(*self);
    self->sync_child_scope_focus();
    self->apply_focus_state();
    UiChildHost::submit_ui_render_commands(out_commands);
}

UiElement* UiListContainer::add_front(std::unique_ptr<UiElement> child)
{
    return insert_child(std::move(child),0,UiLayoutChildOptions{});
}

UiElement* UiListContainer::add_back(std::unique_ptr<UiElement> child)
{
    return insert_child(std::move(child),child_count(),UiLayoutChildOptions{});
}

UiElement* UiListContainer::add_child(std::unique_ptr<UiElement> child,UiLayoutChildOptions options)
{
    return insert_child(std::move(child),child_count(),options);
}

elysia::core::Vector2 UiListContainer::content_extent() const noexcept
{
    const elysia::core::Vector2 intrinsic = layout::intrinsic_list_extent(children(),padding(),_layout);
    const elysia::core::Vector2 explicit_size = size();
    return elysia::core::Vector2(
        std::max(explicit_size.x,intrinsic.x),
        std::max(explicit_size.y,intrinsic.y)
    );
}

void UiListContainer::set_direction(UiListDirection direction) noexcept
{
    _layout.direction = to_layout_direction(direction);
    invalidate_intrinsic_layout();
}

UiListDirection UiListContainer::direction() const noexcept
{
    return _layout.direction == UiLayoutDirection::Vertical ? UiListDirection::Vertical : UiListDirection::Horizontal;
}

void UiListContainer::set_cross_align(UiLayoutAlign align) noexcept
{
    if (_layout.cross_align == align)
        return;
    _layout.cross_align = align;
    invalidate_intrinsic_layout();
}

UiLayoutAlign UiListContainer::cross_align() const noexcept
{
    return _layout.cross_align;
}

void UiListContainer::set_item_spacing(float item_spacing) noexcept
{
    _layout.item_spacing = layout::clamp_non_negative(item_spacing);
    invalidate_intrinsic_layout();
}

float UiListContainer::item_spacing() const noexcept
{
    return _layout.item_spacing;
}

void UiListContainer::rebuild_layout()
{
    layout::layout_list_children(children(),content_rect(),_layout);
}

void UiListContainer::rebuild_focus_registry()
{
    std::vector<DelegatedRegionEntry> regions;
    const std::vector<UiElement*> direct_regions = delegated_focus_regions(*this);
    for (std::size_t index = 0; index < direct_regions.size(); ++index)
    {
        DelegatedRegionEntry entry{};
        entry.region = direct_regions[index];
        if (_layout.direction == UiLayoutDirection::Vertical)
        {
            entry.up = index > 0 ? direct_regions[index - 1] : nullptr;
            entry.down = index + 1 < direct_regions.size() ? direct_regions[index + 1] : nullptr;
        }
        else
        {
            entry.left = index > 0 ? direct_regions[index - 1] : nullptr;
            entry.right = index + 1 < direct_regions.size() ? direct_regions[index + 1] : nullptr;
        }
        regions.push_back(entry);
    }

    std::vector<FocusEntry> entries;
    build_delegated_focus_entries(regions,entries);
    set_focus_entries(std::move(entries));
}

UiElement* UiListContainer::neighbor_region_of(const UiControl* control,UiAction action) const noexcept
{
    if (!is_primary_axis_navigation(action))
        return nullptr;

    const std::vector<UiElement*> regions = delegated_focus_regions(*this);
    UiElement* current_region = delegated_region_of(control,regions);
    if (!current_region)
        return nullptr;

    auto found = std::find(regions.begin(),regions.end(),current_region);
    if (found == regions.end())
        return nullptr;

    if ((_layout.direction == UiLayoutDirection::Vertical && action == UiAction::NavigateUp)
        || (_layout.direction == UiLayoutDirection::Horizontal && action == UiAction::NavigateLeft))
    {
        while (found != regions.begin())
        {
            --found;
            if (first_focusable_control_in_delegated_region(*found))
                return *found;
        }
        return nullptr;
    }

    // Layout-only rows (such as settings hints) are not navigation stops.
    for (auto next = found + 1; next != regions.end(); ++next)
    {
        if (first_focusable_control_in_delegated_region(*next))
            return *next;
    }
    return nullptr;
}

void UiListContainer::sync_child_scope_focus() noexcept
{
    sync_delegated_scope_focus(UiControlFocusScopeHost::focused_target(),is_scope_focused(),delegated_focus_regions(*this));
}

bool UiListContainer::is_primary_axis_navigation(UiAction action) const noexcept
{
    if (_layout.direction == UiLayoutDirection::Vertical)
        return action == UiAction::NavigateUp || action == UiAction::NavigateDown;
    return action == UiAction::NavigateLeft || action == UiAction::NavigateRight;
}
}
