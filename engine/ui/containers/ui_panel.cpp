#include "ui_panel.h"

#include "../layout/ui_layout_geometry.h"
#include "../style/ui_style_defaults.h"

#include <algorithm>

namespace elysia::ui
{
namespace
{
[[nodiscard]] UiPanelInsertDirection opposite_direction(UiPanelInsertDirection direction) noexcept
{
    switch (direction)
    {
    case UiPanelInsertDirection::Up:
        return UiPanelInsertDirection::Down;
    case UiPanelInsertDirection::Down:
        return UiPanelInsertDirection::Up;
    case UiPanelInsertDirection::Left:
        return UiPanelInsertDirection::Right;
    case UiPanelInsertDirection::Right:
    default:
        return UiPanelInsertDirection::Left;
    }
}

std::optional<UiControl*>& neighbor_slot(UiFocusNeighbors& neighbors,UiPanelInsertDirection direction) noexcept
{
    switch (direction)
    {
    case UiPanelInsertDirection::Up:
        return neighbors.up;
    case UiPanelInsertDirection::Down:
        return neighbors.down;
    case UiPanelInsertDirection::Left:
        return neighbors.left;
    case UiPanelInsertDirection::Right:
    default:
        return neighbors.right;
    }
}

[[nodiscard]] bool is_zero_margin(const UiLayoutMargin& margin) noexcept
{
    return margin.left == 0.0f
        && margin.top == 0.0f
        && margin.right == 0.0f
        && margin.bottom == 0.0f;
}

[[nodiscard]] bool uses_panel_anchor_layout(const UiLayoutChildOptions& options) noexcept
{
    return options._anchor != UiLayoutAnchor::TopLeft
        || !is_zero_margin(options._margin)
        || options._use_size_override
        || options._use_custom_cross_align
        || options._fill_cross_axis;
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

UiPanel::UiPanel(const elysia::core::Rect& rect,int order) noexcept
    : UiControlFocusScopeHost(rect,order)
{
    reset();
}

UiPanel::UiPanel(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiControlFocusScopeHost(position,size,order)
{
    reset();
}

UiPanel::UiPanel(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiControlFocusScopeHost(center,size,from_center,order)
{
    reset();
}

void UiPanel::reset() noexcept
{
    UiControlFocusScopeHost::reset();
    reset_delegated_focus_state();
    _style_state.reset(UiStyleDefaults::panel());
    _visual_role = UiPanelVisualRole::Default;
    _focus_links.clear();
    _last_focusable = nullptr;
    _last_child_layout_origin = elysia::core::Vector2::zero();
    _has_child_layout_origin = false;
}

void UiPanel::set_scope_focused(bool focused) noexcept
{
    UiControlFocusScopeHost::set_scope_focused(focused);
    sync_child_scope_focus();
}

bool UiPanel::focus_first_available()
{
    const bool focused = UiControlFocusScopeHost::focus_first_available();
    if (!focused)
    {
        // focused_target() validates by calling this method. Avoid querying it again
        // when an informational panel has no focusable children.
        sync_delegated_scope_focus(
            static_cast<UiElement*>(nullptr),
            is_scope_focused(),
            delegated_focus_regions(*this));
        return false;
    }

    sync_delegated_owner_scope_target(UiControlFocusScopeHost::focused_target());
    sync_child_scope_focus();
    return true;
}

bool UiPanel::can_navigate(UiAction action) const noexcept
{
    if (!is_navigation_action(action))
        return false;

    auto* self = const_cast<UiPanel*>(this);
    self->cleanup_destroyed_children();
    self->update_layout_if_dirty();
    self->refresh_focus_registry();
    self->ensure_valid_focus();
    self->sync_child_scope_focus();
    self->apply_focus_state();

    if (const UiControl* target = UiControlFocusScopeHost::focused_target())
    {
        if (const UiFocusScope* scope = delegated_owner_scope_of(target))
        {
            if (scope->can_navigate(action))
                return true;
        }
    }

    return UiControlFocusScopeHost::can_navigate(action);
}

void UiPanel::update(double delta)
{
    sync_child_scope_focus();
    UiControlFocusScopeHost::update(delta);
    sync_host_delegated_focus_target(*this);
    sync_child_scope_focus();
}

void UiPanel::on_ui_input_frame(const UiInputFrame& input)
{
    sync_child_scope_focus();
    UiControlFocusScopeHost::on_ui_input_frame(input);
    sync_host_delegated_focus_target(*this);
    sync_child_scope_focus();
}

bool UiPanel::on_ui_input_event(const UiInputEvent& event)
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

    if (UiControl* target = UiControlFocusScopeHost::focused_target())
    {
        if (UiFocusScope* scope = delegated_owner_scope_of(target))
        {
            const bool should_delegate_confirm = (event.action == UiAction::Confirm || event.action == UiAction::Cancel)
                && (event.type == UiInputEventType::ActionPressed || event.type == UiInputEventType::ActionReleased);
            const bool should_delegate_navigation = event.type == UiInputEventType::ActionPressed
                && is_navigation_action(event.action);

            if (should_delegate_confirm || should_delegate_navigation)
            {
                bool handled = false;
                if (auto* receiver = dynamic_cast<UiInputEventReceiver*>(&scope->focus_scope_element()))
                    handled = receiver->on_ui_input_event(event);

                cleanup_destroyed_children();
                refresh_focus_registry();
                ensure_valid_focus();
                sync_host_delegated_focus_target(*this);
                sync_child_scope_focus();
                apply_focus_state();

                if (should_delegate_confirm || handled)
                    return handled;
            }
        }
    }

    const bool handled = UiControlFocusScopeHost::on_ui_input_event(event);
    sync_host_delegated_focus_target(*this);
    sync_child_scope_focus();
    return handled;
}

void UiPanel::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;

    auto* self = const_cast<UiPanel*>(this);
    self->cleanup_destroyed_children();
    self->update_layout_if_dirty();
    self->refresh_focus_registry();
    self->ensure_valid_focus();
    self->sync_host_delegated_focus_target(*self);
    self->sync_child_scope_focus();
    self->apply_focus_state();

    const elysia::core::Rect& rect = screen_rect();
    const UiPanelStyle& style = _style_state.effective_style();
    if (style.draw_background && !rect.is_empty())
        out_commands.push_back(elysia::core::make_ui_fill_rect_command(rect,apply_opacity(style.background),style.corner_radius));
    submit_child_render_commands(out_commands);
    if (style.draw_border && !rect.is_empty())
        out_commands.push_back(elysia::core::make_ui_draw_rect_command(
            rect,apply_opacity(style.border),style.corner_radius,style.border_width));
}

elysia::core::Vector2 UiPanel::content_extent() const noexcept
{
    return size();
}

UiElement* UiPanel::add_child(std::unique_ptr<UiElement> child,UiPanelInsertDirection direction)
{
    return insert_panel_child(std::move(child),direction);
}

UiElement* UiPanel::add_child(std::unique_ptr<UiElement> child,UiLayoutChildOptions options)
{
    if (!uses_panel_anchor_layout(options))
        return insert_panel_child(std::move(child),UiPanelInsertDirection::Down);

    return UiChildHost::add_child(std::move(child),options);
}

void UiPanel::set_base_style(const UiPanelStyle& style) noexcept
{
    _style_state.set_base_style(style);
}

void UiPanel::set_style_overrides(const UiPanelStyleOverrides& overrides) noexcept
{
    _style_state.set_style_overrides(overrides);
}

const UiPanelStyle& UiPanel::style() const noexcept
{
    return _style_state.effective_style();
}

const UiPanelStyleOverrides& UiPanel::style_overrides() const noexcept { return _style_state.style_overrides(); }
bool UiPanel::has_style_overrides() const noexcept
{
    return _style_state.has_style_overrides();
}

void UiPanel::clear_style_overrides() noexcept
{
    _style_state.clear_style_overrides();
}

void UiPanel::set_visual_role(UiPanelVisualRole role) noexcept
{
    _visual_role = role;
    notify_host_base_style_invalidated();
}

UiPanelVisualRole UiPanel::visual_role() const noexcept
{
    return _visual_role;
}

void UiPanel::rebuild_layout()
{
    const elysia::core::Rect bounds = content_rect();
    const elysia::core::Vector2 current_origin = position();
    const elysia::core::Vector2 delta = _has_child_layout_origin
        ? (current_origin - _last_child_layout_origin)
        : current_origin;

    for (ChildEntry& entry : children())
    {
        if (!entry.element)
            continue;

        if (uses_panel_anchor_layout(entry.layout))
        {
            const elysia::core::Vector2 child_size = entry.layout._use_size_override
                ? layout::clamp_size(entry.layout._size_override)
                : layout::clamp_size(entry.element->size());
            entry.element->set_screen_rect(layout::anchored_rect(bounds,child_size,entry.layout._anchor,entry.layout._margin));
            continue;
        }

        if (!delta.is_zero())
            entry.element->set_position(entry.element->position() + delta);
    }

    _last_child_layout_origin = current_origin;
    _has_child_layout_origin = true;
}

void UiPanel::rebuild_focus_registry()
{
    prune_panel_links();

    std::vector<DelegatedRegionEntry> regions;
    for (UiElement* region : delegated_focus_regions(*this))
    {
        const FocusLink& link = ensure_link(*region);
        regions.push_back(DelegatedRegionEntry{ region,link.up,link.down,link.left,link.right });
    }

    std::vector<FocusEntry> entries;
    build_delegated_focus_entries(regions,entries);
    set_focus_entries(std::move(entries));
}

UiElement* UiPanel::insert_panel_child(std::unique_ptr<UiElement> child,UiPanelInsertDirection direction)
{
    if (!child)
        return nullptr;

    UiElement* focus_region = delegated_focus_region(child.get());
    UiElement* added = UiChildHost::add_child(std::move(child));
    if (added && _has_child_layout_origin)
        added->set_position(added->position() + _last_child_layout_origin);
    if (!added || !focus_region)
        return added;

    prune_panel_links();
    FocusLink& current = ensure_link(*focus_region);
    if (_last_focusable && _last_focusable != focus_region)
    {
        FocusLink& previous = ensure_link(*_last_focusable);
        UiElement*& previous_neighbor = [&]() -> UiElement*&
        {
            switch (direction)
            {
            case UiPanelInsertDirection::Up:
                return previous.up;
            case UiPanelInsertDirection::Down:
                return previous.down;
            case UiPanelInsertDirection::Left:
                return previous.left;
            case UiPanelInsertDirection::Right:
            default:
                return previous.right;
            }
        }();
        UiElement*& current_neighbor = [&]() -> UiElement*&
        {
            switch (opposite_direction(direction))
            {
            case UiPanelInsertDirection::Up:
                return current.up;
            case UiPanelInsertDirection::Down:
                return current.down;
            case UiPanelInsertDirection::Left:
                return current.left;
            case UiPanelInsertDirection::Right:
            default:
                return current.right;
            }
        }();
        previous_neighbor = focus_region;
        current_neighbor = _last_focusable;
    }
    _last_focusable = focus_region;
    return added;
}

void UiPanel::sync_child_scope_focus() noexcept
{
    sync_delegated_scope_focus(UiControlFocusScopeHost::focused_target(),is_scope_focused(),delegated_focus_regions(*this));
}

void UiPanel::prune_panel_links()
{
    const std::vector<UiElement*> live_regions = delegated_focus_regions(*this);
    auto is_live = [&live_regions](const UiElement* element)
    {
        return element && std::find(live_regions.begin(),live_regions.end(),element) != live_regions.end();
    };

    _focus_links.erase(std::remove_if(_focus_links.begin(),_focus_links.end(),[&](const FocusLink& link)
    {
        return !is_live(link.element);
    }),_focus_links.end());

    for (FocusLink& link : _focus_links)
    {
        if (!is_live(link.up))
            link.up = nullptr;
        if (!is_live(link.down))
            link.down = nullptr;
        if (!is_live(link.left))
            link.left = nullptr;
        if (!is_live(link.right))
            link.right = nullptr;
    }

    if (!is_live(_last_focusable))
        _last_focusable = live_regions.empty() ? nullptr : live_regions.back();
}

UiPanel::FocusLink& UiPanel::ensure_link(UiElement& element)
{
    if (FocusLink* link = find_link(element))
        return *link;

    _focus_links.push_back(FocusLink{ &element });
    return _focus_links.back();
}

UiPanel::FocusLink* UiPanel::find_link(UiElement& element) noexcept
{
    auto found = std::find_if(_focus_links.begin(),_focus_links.end(),[&element](const FocusLink& link)
    {
        return link.element == &element;
    });
    return found != _focus_links.end() ? &(*found) : nullptr;
}

const UiPanel::FocusLink* UiPanel::find_link(const UiElement& element) const noexcept
{
    auto found = std::find_if(_focus_links.begin(),_focus_links.end(),[&element](const FocusLink& link)
    {
        return link.element == &element;
    });
    return found != _focus_links.end() ? &(*found) : nullptr;
}

}

