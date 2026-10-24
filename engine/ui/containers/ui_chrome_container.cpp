#include "ui_chrome_container.h"

#include "../focus/ui_focus_scope_utils.h"
#include "../input/contracts/ui_input_event_receiver.h"
#include "../layout/ui_anchor_layout.h"
#include "../layout/ui_layout_geometry.h"
#include "../style/ui_style_defaults.h"
#include "../../core/render/render_command.h"

#include <algorithm>

namespace elysia::ui
{
namespace
{
class HeaderActionList : public UiListContainer
{
public:
    using UiListContainer::UiListContainer;

    void update(double delta) override
    {
        UiChildHost::update(delta);
    }

    void on_ui_input_frame(const UiInputFrame& input) override
    {
        UiChildHost::on_ui_input_frame(input);
    }

    bool on_ui_input_event(const UiInputEvent& event) override
    {
        return UiChildHost::on_ui_input_event(event);
    }

    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override
    {
        UiChildHost::submit_ui_render_commands(out_commands);
    }
};

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

[[nodiscard]] bool is_primary_mouse_press(const UiInputEvent& event) noexcept
{
    return event.type == UiInputEventType::PointerPressed
        && event.device == elysia::input::InputDevice::Mouse
        && event.control == elysia::input::RawInputControl::MouseLeft;
}

[[nodiscard]] elysia::core::Vector2 slot_intrinsic_extent(const UiChildHost* host) noexcept
{
    if (!host)
        return elysia::core::Vector2::zero();

    elysia::core::Vector2 extent = elysia::core::Vector2::zero();
    for (std::size_t index = 0; index < host->child_count(); ++index)
    {
        const UiElement* child = host->child_at(index);
        if (!child || child->is_destroyed() || !child->is_active() || !child->is_visible())
            continue;

        const elysia::core::Vector2 child_extent = child->content_extent();
        extent.x = std::max(extent.x,child_extent.x);
        extent.y = std::max(extent.y,child_extent.y);
    }

    return extent;
}

void collect_focusable_controls(
    const UiElement& element,
    std::vector<UiControl*>& out_controls,
    bool recurse_into_nested_scopes
)
{
    if (element.is_destroyed() || !element.is_active() || !element.is_visible())
        return;

    if (const auto* control = dynamic_cast<const UiControl*>(&element))
        out_controls.push_back(const_cast<UiControl*>(control));

    const auto* child_host = dynamic_cast<const UiChildHost*>(&element);
    if (!child_host)
        return;

    for (std::size_t index = 0; index < child_host->child_count(); ++index)
    {
        const UiElement* child = child_host->child_at(index);
        if (!child)
            continue;
        if (!recurse_into_nested_scopes && dynamic_cast<const UiFocusScope*>(child))
            continue;
        collect_focusable_controls(*child,out_controls,recurse_into_nested_scopes);
    }
}

}

UiChromeContainer::SlotHost::SlotHost(const elysia::core::Rect& rect,int order) noexcept
    : UiChildHost(rect,order) {}

void UiChromeContainer::SlotHost::set_fill_children(bool fill_children) noexcept
{
    _fill_children = fill_children;
    mark_layout_dirty();
}

void UiChromeContainer::SlotHost::rebuild_layout()
{
    if (!_fill_children)
    {
        layout::layout_anchored_children(children(),content_rect());
        return;
    }

    const elysia::core::Rect bounds = content_rect();
    for (ChildEntry& child : children())
    {
        if (child.element)
            child.element->set_screen_rect(bounds);
    }
}

UiChromeContainer::UiChromeContainer(const elysia::core::Rect& rect,int order) noexcept
    : UiControlFocusScopeHost(rect,order)
{
    reset();
}

UiChromeContainer::UiChromeContainer(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiChromeContainer(elysia::core::Rect(position.x,position.y,size.x,size.y),order) {}

UiChromeContainer::UiChromeContainer(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiChromeContainer(elysia::core::Rect::from_center(center,size),order) {}

void UiChromeContainer::reset() noexcept
{
    UiControlFocusScopeHost::reset();
    reset_delegated_focus_state();
    clear_internal_host_pointers();
    _style_state.reset(UiStyleDefaults::chrome_container());
    _header_padding = UiLayoutPadding{ 8.0f,6.0f,8.0f,6.0f };
    _body_padding = UiLayoutPadding{};
    _header_height = 48.0f;
    _header_visible = true;
    _scope_focused = false;
    _body_scope_active = false;
    create_internal_hosts();
}

void UiChromeContainer::update(double delta)
{
    sync_body_scope_focus();
    UiControlFocusScopeHost::update(delta);
    sync_body_scope_focus();
}

void UiChromeContainer::on_ui_input_frame(const UiInputFrame& input)
{
    sync_body_scope_focus();
    UiControlFocusScopeHost::on_ui_input_frame(input);
    sync_body_scope_focus();
}

bool UiChromeContainer::on_ui_input_event(const UiInputEvent& event)
{
    sync_body_scope_focus();

    UiFocusScope* body_scope = delegated_body_scope();
    const bool targets_header = event_targets_header(event);
    const bool targets_body = event_targets_body_scope(event);

    if (body_scope)
    {
        if (is_primary_mouse_press(event))
        {
            if (targets_header)
                (void)leave_body_scope();
            else if (targets_body)
                (void)enter_body_scope(false);
        }
        else if (!_body_scope_active && targets_body)
        {
            (void)enter_body_scope(false);
        }
    }

    body_scope = delegated_body_scope();
    if (_body_scope_active && body_scope)
    {
        const bool route_to_header = targets_header && event.type != UiInputEventType::PointerReleased;
        if (!route_to_header)
        {
            const bool should_dispatch_to_body = event.device != elysia::input::InputDevice::Mouse
                || targets_body
                || event.type == UiInputEventType::MouseMoved
                || event.type == UiInputEventType::PointerReleased;

            bool handled = false;
            if (should_dispatch_to_body)
            {
                if (auto* receiver = dynamic_cast<UiInputEventReceiver*>(&body_scope->focus_scope_element()))
                    handled = receiver->on_ui_input_event(event);
            }

            cleanup_destroyed_children();
            refresh_focus_registry();
            ensure_valid_focus();
            body_scope = delegated_body_scope();
            if (!handled && body_scope && event.type == UiInputEventType::ActionPressed
                && event.action == UiAction::NavigateUp
                && !body_scope->can_navigate(UiAction::NavigateUp)
                && header_has_focusable_target())
            {
                handled = leave_body_scope();
            }
            sync_body_scope_focus();
            apply_focus_state();
            return handled;
        }

        (void)leave_body_scope();
    }

    bool handled = UiControlFocusScopeHost::on_ui_input_event(event);
    if (!handled
        && event.type == UiInputEventType::ActionPressed
        && event.action == UiAction::NavigateDown
        && body_scope
        && body_scope->has_focusable_target())
    {
        handled = enter_body_scope(true);
    }

    sync_body_scope_focus();
    return handled;
}

void UiChromeContainer::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;

    auto* self = const_cast<UiChromeContainer*>(this);
    self->cleanup_destroyed_children();
    self->update_layout_if_dirty();
    self->refresh_focus_registry();
    self->ensure_valid_focus();
    self->sync_body_scope_focus();
    self->apply_focus_state();

    const elysia::core::Rect& rect = screen_rect();
    const UiChromeContainerStyle& style = _style_state.effective_style();
    const float corner_radius = style.corner_radius;
    if (style.draw_background && !rect.is_empty())
        out_commands.push_back(elysia::core::make_ui_fill_rect_command(rect,apply_opacity(style.background),corner_radius));
    if (_header_visible && style.draw_header_background && !header_rect().is_empty())
        elysia::core::append_ui_fill_top_rounded_rect_commands(
            out_commands,rect,header_rect(),apply_opacity(style.header_background),corner_radius);
    submit_child_render_commands(out_commands);
    if (style.draw_border && !rect.is_empty())
        out_commands.push_back(elysia::core::make_ui_draw_rect_command(
            rect,apply_opacity(style.border),corner_radius,style.border_width));
}

elysia::core::Vector2 UiChromeContainer::content_extent() const noexcept
{
    const elysia::core::Vector2 explicit_size = size();
    const elysia::core::Vector2 header_extent = desired_header_extent();

    const UiElement* body_element = body_content();
    const elysia::core::Vector2 body_extent = body_element ? body_element->content_extent() : elysia::core::Vector2::zero();
    const float body_width = _body_padding.left + body_extent.x + _body_padding.right;
    const float body_height = _body_padding.top + body_extent.y + _body_padding.bottom;

    return elysia::core::Vector2(
        std::max(explicit_size.x,std::max(header_extent.x,body_width)),
        std::max(explicit_size.y,header_extent.y + body_height)
    );
}

void UiChromeContainer::set_scope_focused(bool focused) noexcept
{
    _scope_focused = focused;
    sync_body_scope_focus();
}

bool UiChromeContainer::is_scope_focused() const noexcept
{
    return _scope_focused;
}

bool UiChromeContainer::has_focusable_target() const noexcept
{
    if (UiControlFocusScopeHost::has_focusable_target())
        return true;

    if (const UiFocusScope* body_scope = delegated_body_scope())
        return body_scope->has_focusable_target();

    return false;
}

bool UiChromeContainer::focus_first_available()
{
    sync_body_scope_focus();

    if (_body_scope_active)
    {
        if (const UiFocusScope* body_scope = delegated_body_scope();
            body_scope && is_control_usable(body_scope->focused_target()))
        {
            return true;
        }
    }

    // Normal chrome entry favors header actions; specialized composites may explicitly enter body.
    if (header_has_focusable_target())
    {
        _body_scope_active = false;
        sync_body_scope_focus();
        return UiControlFocusScopeHost::focus_first_available();
    }

    if (const UiFocusScope* body_scope = delegated_body_scope(); body_scope && body_scope->has_focusable_target())
        return enter_body_scope(true);

    return UiControlFocusScopeHost::focus_first_available();
}

bool UiChromeContainer::focus_body_first_available()
{
    // Refresh nested geometry and focus entries before crossing into the delegated body scope.
    cleanup_destroyed_children();
    update_layout_if_dirty();
    refresh_focus_registry();
    ensure_valid_focus();
    return enter_body_scope(true);
}

UiControl* UiChromeContainer::focused_target() const noexcept
{
    if (_body_scope_active)
    {
        if (const UiFocusScope* body_scope = delegated_body_scope())
            return body_scope->focused_target();
    }

    return UiControlFocusScopeHost::focused_target();
}

bool UiChromeContainer::can_navigate(UiAction action) const noexcept
{
    if (!is_navigation_action(action))
        return false;

    if (_body_scope_active)
    {
        if (const UiFocusScope* body_scope = delegated_body_scope())
        {
            if (body_scope->can_navigate(action))
                return true;
            return action == UiAction::NavigateUp && header_has_focusable_target();
        }
        return false;
    }

    if (UiControlFocusScopeHost::can_navigate(action))
        return true;

    if (action == UiAction::NavigateDown)
    {
        if (const UiFocusScope* body_scope = delegated_body_scope())
            return header_has_focusable_target() && body_scope->has_focusable_target();
    }

    return false;
}

UiElement* UiChromeContainer::add_left_action(std::unique_ptr<UiElement> action,UiChromeActionInsertPosition position)
{
    return add_action(_left_actions,std::move(action),position);
}

void UiChromeContainer::clear_left_actions()
{
    clear_children(_left_actions);
}

UiElement* UiChromeContainer::add_title_child(std::unique_ptr<UiElement> child,UiLayoutChildOptions options)
{
    return _title_slot ? _title_slot->add_child(std::move(child),options) : nullptr;
}

void UiChromeContainer::clear_title_children()
{
    clear_children(_title_slot);
}

UiElement* UiChromeContainer::add_right_action(std::unique_ptr<UiElement> action,UiChromeActionInsertPosition position)
{
    return add_action(_right_actions,std::move(action),position);
}

void UiChromeContainer::clear_right_actions()
{
    clear_children(_right_actions);
}

UiElement* UiChromeContainer::set_body(std::unique_ptr<UiElement> body_element)
{
    if (!_body)
        return nullptr;

    clear_children(_body);
    if (!body_element)
    {
        _body_scope_active = false;
        sync_body_scope_focus();
        invalidate_intrinsic_layout();
        return nullptr;
    }

    UiElement* added = _body->add_child(std::move(body_element));
    sync_body_scope_focus();
    invalidate_intrinsic_layout();
    return added;
}

UiElement* UiChromeContainer::body_content_mutable() noexcept
{
    if (!_body)
        return nullptr;

    UiElement* child = _body->child_at(0);
    return child && !child->is_destroyed() ? child : nullptr;
}

const UiElement* UiChromeContainer::body_content() const noexcept
{
    if (!_body)
        return nullptr;

    const UiElement* child = _body->child_at(0);
    return child && !child->is_destroyed() ? child : nullptr;
}

void UiChromeContainer::clear_body()
{
    if (!_body)
        return;
    clear_children(_body);
    _body_scope_active = false;
    sync_body_scope_focus();
    invalidate_intrinsic_layout();
}

void UiChromeContainer::set_header_visible(bool visible) noexcept
{
    _header_visible = visible;
    if (_left_actions)
        _left_actions->set_visible(visible);
    if (_title_slot)
        _title_slot->set_visible(visible);
    if (_right_actions)
        _right_actions->set_visible(visible);
    invalidate_intrinsic_layout();
}

bool UiChromeContainer::header_visible() const noexcept
{
    return _header_visible;
}

void UiChromeContainer::set_header_height(float height) noexcept
{
    _header_height = layout::clamp_non_negative(height);
    invalidate_intrinsic_layout();
}

float UiChromeContainer::header_height() const noexcept
{
    return _header_height;
}

void UiChromeContainer::set_header_padding(const UiLayoutPadding& padding) noexcept
{
    _header_padding = padding;
    invalidate_intrinsic_layout();
}

const UiLayoutPadding& UiChromeContainer::header_padding() const noexcept
{
    return _header_padding;
}

void UiChromeContainer::set_body_padding(const UiLayoutPadding& padding) noexcept
{
    _body_padding = padding;
    invalidate_intrinsic_layout();
}

const UiLayoutPadding& UiChromeContainer::body_padding() const noexcept
{
    return _body_padding;
}

void UiChromeContainer::set_base_style(const UiChromeContainerStyle& style) noexcept
{
    _style_state.set_base_style(style);
}

void UiChromeContainer::set_style_overrides(const UiChromeContainerStyleOverrides& overrides) noexcept
{
    _style_state.set_style_overrides(overrides);
}

const UiChromeContainerStyle& UiChromeContainer::style() const noexcept
{
    return _style_state.effective_style();
}

const UiChromeContainerStyleOverrides& UiChromeContainer::style_overrides() const noexcept { return _style_state.style_overrides(); }
bool UiChromeContainer::has_style_overrides() const noexcept
{
    return _style_state.has_style_overrides();
}

void UiChromeContainer::clear_style_overrides() noexcept
{
    _style_state.clear_style_overrides();
}


void UiChromeContainer::rebuild_layout()
{
    if (!_left_actions || !_title_slot || !_right_actions || !_body)
        return;

    _left_actions->set_size(elysia::core::Vector2::zero());
    _right_actions->set_size(elysia::core::Vector2::zero());

    const elysia::core::Rect header = layout::padded_content_rect(header_rect(),_header_padding);
    const float left_width = _header_visible ? std::min(_left_actions->content_extent().x,header.width()) : 0.0f;
    const float right_width = _header_visible
        ? std::min(_right_actions->content_extent().x,std::max(0.0f,header.width() - left_width))
        : 0.0f;
    const float title_width = std::max(0.0f,header.width() - left_width - right_width);

    _left_actions->set_screen_rect(elysia::core::Rect(header.left(),header.top(),left_width,header.height()));
    _right_actions->set_screen_rect(elysia::core::Rect(header.right() - right_width,header.top(),right_width,header.height()));
    _title_slot->set_screen_rect(elysia::core::Rect(header.left() + left_width,header.top(),title_width,header.height()));
    _body->set_screen_rect(layout::padded_content_rect(body_rect(),_body_padding));

    _left_actions->set_visible(_header_visible);
    _left_actions->set_active(_header_visible);
    _title_slot->set_visible(_header_visible);
    _title_slot->set_active(_header_visible);
    _right_actions->set_visible(_header_visible);
    _right_actions->set_active(_header_visible);

    // UiDialog derives its body layout from the payload's rect immediately after
    // updating chrome. Settle this nested slot now so the first overlay frame uses
    // the final body bounds instead of the payload's construction-time bounds.
    _body->update_layout_if_dirty();
}

void UiChromeContainer::rebuild_focus_registry()
{
    std::vector<UiControl*> header_controls;
    std::vector<UiControl*> body_controls;

    if (_header_visible)
    {
        collect_controls_from(*_left_actions,header_controls);
        collect_controls_from(*_title_slot,header_controls);
        collect_controls_from(*_right_actions,header_controls);
    }

    if (!delegated_body_scope())
        collect_controls_from(*_body,body_controls,false);

    UiControl* first_header = header_controls.empty() ? nullptr : header_controls.front();
    UiControl* first_body = body_controls.empty() ? nullptr : body_controls.front();

    std::vector<FocusEntry> entries;
    entries.reserve(header_controls.size() + body_controls.size());

    for (std::size_t index = 0; index < header_controls.size(); ++index)
    {
        UiFocusNeighbors neighbors;
        if (index > 0)
            neighbors.left = header_controls[index - 1];
        if (index + 1 < header_controls.size())
            neighbors.right = header_controls[index + 1];
        if (first_body)
            neighbors.down = first_body;
        entries.push_back(FocusEntry{ header_controls[index],neighbors });
    }

    for (std::size_t index = 0; index < body_controls.size(); ++index)
    {
        UiFocusNeighbors neighbors;
        if (index > 0)
        {
            neighbors.up = body_controls[index - 1];
            neighbors.left = body_controls[index - 1];
        }
        else if (first_header)
        {
            neighbors.up = first_header;
        }
        if (index + 1 < body_controls.size())
        {
            neighbors.down = body_controls[index + 1];
            neighbors.right = body_controls[index + 1];
        }
        entries.push_back(FocusEntry{ body_controls[index],neighbors });
    }

    set_focus_entries(std::move(entries));
}

void UiChromeContainer::create_internal_hosts()
{
    auto left_actions = std::make_unique<HeaderActionList>();
    left_actions->set_direction(UiListDirection::Horizontal);
    left_actions->set_item_spacing(8.0f);
    _left_actions = left_actions.get();
    UiChildHost::add_child(std::move(left_actions));

    auto title_slot = std::make_unique<SlotHost>();
    _title_slot = title_slot.get();
    UiChildHost::add_child(std::move(title_slot));

    auto right_actions = std::make_unique<HeaderActionList>();
    right_actions->set_direction(UiListDirection::Horizontal);
    right_actions->set_item_spacing(8.0f);
    _right_actions = right_actions.get();
    UiChildHost::add_child(std::move(right_actions));

    auto body = std::make_unique<SlotHost>();
    body->set_fill_children(true);
    _body = body.get();
    UiChildHost::add_child(std::move(body));
}

void UiChromeContainer::clear_internal_host_pointers() noexcept
{
    _left_actions = nullptr;
    _title_slot = nullptr;
    _right_actions = nullptr;
    _body = nullptr;
}

void UiChromeContainer::collect_controls_from(
    const UiElement& element,
    std::vector<UiControl*>& out_controls,
    bool recurse_into_nested_scopes
) const
{
    collect_focusable_controls(element,out_controls,recurse_into_nested_scopes);
}

UiElement* UiChromeContainer::add_action(
    UiListContainer* actions,
    std::unique_ptr<UiElement> child,
    UiChromeActionInsertPosition position)
{
    if (!actions || !child)
        return nullptr;

    UiElement* raw = child.get();
    if (position == UiChromeActionInsertPosition::Front)
        actions->add_front(std::move(child));
    else
        actions->add_back(std::move(child));
    return raw;
}

void UiChromeContainer::clear_children(UiChildHost* host) noexcept
{
    if (host)
        host->clear_children();
}

UiFocusScope* UiChromeContainer::delegated_body_scope() noexcept
{
    return delegated_scope_for_region(delegated_focus_region(body_content_mutable()));
}

const UiFocusScope* UiChromeContainer::delegated_body_scope() const noexcept
{
    return delegated_scope_for_region(delegated_focus_region(body_content()));
}

UiControl* UiChromeContainer::first_header_focusable() const noexcept
{
    std::vector<UiControl*> controls;
    if (_header_visible)
    {
        collect_controls_from(*_left_actions,controls);
        collect_controls_from(*_title_slot,controls);
        collect_controls_from(*_right_actions,controls);
    }

    auto found = std::find_if(controls.begin(),controls.end(),[](const UiControl* control)
    {
        return is_control_usable(control);
    });
    return found != controls.end() ? *found : nullptr;
}

bool UiChromeContainer::header_has_focusable_target() const noexcept
{
    return first_header_focusable() != nullptr;
}

bool UiChromeContainer::enter_body_scope(bool focus_first)
{
    UiElement* body_region = delegated_focus_region(body_content_mutable());
    UiFocusScope* body_scope = delegated_scope_for_region(body_region);
    if (!body_scope || !body_scope->has_focusable_target())
        return false;

    if (!focus_delegated_region(body_region,focus_first))
        return false;

    _body_scope_active = true;
    sync_body_scope_focus();
    return true;
}

bool UiChromeContainer::leave_body_scope()
{
    _body_scope_active = false;
    sync_body_scope_focus();

    UiControl* restore_target = UiControlFocusScopeHost::focused_target();
    if (!is_control_usable(restore_target))
        restore_target = first_header_focusable();

    if (restore_target)
    {
        UiControlFocusScopeHost::set_focused_target(restore_target);
        return true;
    }

    return false;
}

void UiChromeContainer::sync_body_scope_focus() noexcept
{
    UiElement* body_region = delegated_focus_region(body_content_mutable());
    UiFocusScope* body_scope = delegated_scope_for_region(body_region);
    if (_body_scope_active && !body_scope)
        _body_scope_active = false;

    UiControlFocusScopeHost::set_scope_focused(_scope_focused && !_body_scope_active);

    std::vector<UiElement*> regions;
    if (body_scope && body_region)
        regions.push_back(body_region);
    sync_delegated_scope_focus(_body_scope_active ? body_region : nullptr,_scope_focused,regions);
}

bool UiChromeContainer::event_targets_header(const UiInputEvent& event) const noexcept
{
    if (!_header_visible || !is_mouse_position_event(event))
        return false;

    return header_rect().contains(elysia::core::Vector2(
        static_cast<float>(event.mouse_x),
        static_cast<float>(event.mouse_y)
    ));
}

bool UiChromeContainer::event_targets_body_scope(const UiInputEvent& event) const noexcept
{
    if (!is_mouse_position_event(event))
        return false;

    if (const UiFocusScope* body_scope = delegated_body_scope())
        return body_scope->contains_focus_point(event.mouse_x,event.mouse_y);

    return false;
}

elysia::core::Vector2 UiChromeContainer::desired_header_extent() const noexcept
{
    if (!_header_visible)
        return elysia::core::Vector2::zero();

    const elysia::core::Vector2 left_extent = _left_actions
        ? _left_actions->content_extent()
        : elysia::core::Vector2::zero();
    const elysia::core::Vector2 title_extent = slot_intrinsic_extent(_title_slot);
    const elysia::core::Vector2 right_extent = _right_actions
        ? _right_actions->content_extent()
        : elysia::core::Vector2::zero();

    const float intrinsic_height = _header_padding.top
        + std::max({ left_extent.y,title_extent.y,right_extent.y,0.0f })
        + _header_padding.bottom;
    return elysia::core::Vector2(
        _header_padding.left + left_extent.x + title_extent.x + right_extent.x + _header_padding.right,
        std::max(_header_height,intrinsic_height)
    );
}

elysia::core::Rect UiChromeContainer::header_rect() const noexcept
{
    if (!_header_visible)
        return elysia::core::Rect::zero();
    const elysia::core::Rect content = content_rect();
    const float header_height = std::min(desired_header_extent().y,content.height());
    return elysia::core::Rect(content.left(),content.top(),content.width(),header_height);
}

elysia::core::Rect UiChromeContainer::body_rect() const noexcept
{
    const elysia::core::Rect content = content_rect();
    if (!_header_visible)
        return content;
    const float header_height = std::min(desired_header_extent().y,content.height());
    return elysia::core::Rect(
        content.left(),
        content.top() + header_height,
        content.width(),
        std::max(0.0f,content.height() - header_height)
    );
}

}
