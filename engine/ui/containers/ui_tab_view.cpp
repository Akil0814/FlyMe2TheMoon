#include "ui_tab_view.h"

namespace elysia::ui
{
UiTabView::UiTabView(const elysia::core::Rect& rect,int order) noexcept : UiControlFocusScopeHost(rect,order)
{
    reset();
}

void UiTabView::reset() noexcept
{
    UiControlFocusScopeHost::reset();
    reset_delegated_focus_state();
    _selected_index.reset();
}

void UiTabView::update(double delta)
{
    UiControlFocusScopeHost::update(delta);
    sync_host_delegated_focus_target(*this);
    sync_delegated_scope_focus(focused_target(),is_scope_focused(),delegated_focus_regions(*this));
}

void UiTabView::on_ui_input_frame(const UiInputFrame& input)
{
    UiControlFocusScopeHost::on_ui_input_frame(input);
    sync_host_delegated_focus_target(*this);
    sync_delegated_scope_focus(focused_target(),is_scope_focused(),delegated_focus_regions(*this));
}

bool UiTabView::on_ui_input_event(const UiInputEvent& event)
{
    if (event.type == UiInputEventType::PointerPressed && event.device == elysia::input::InputDevice::Mouse
        && event.control == elysia::input::RawInputControl::MouseLeft)
    {
        update_focus_input_device(event.device);
        synchronize_focus_state();
        (void)set_focused_target_internal(find_registered_target_at(event.mouse_x,event.mouse_y));
        sync_delegated_scope_focus(focused_target(),is_scope_focused(),delegated_focus_regions(*this));
    }
    const bool routes_to_page = ((event.action == UiAction::Confirm || event.action == UiAction::Cancel)
            && (event.type == UiInputEventType::ActionPressed || event.type == UiInputEventType::ActionReleased))
        || (event.type == UiInputEventType::ActionPressed && is_navigation_action(event.action));

    if (routes_to_page)
    {
        if (UiFocusScope* page_scope = delegated_owner_scope_of(focused_target()))
        {
            if (auto* receiver = dynamic_cast<UiInputEventReceiver*>(&page_scope->focus_scope_element()))
            {
                if (receiver->on_ui_input_event(event))
                {
                    sync_host_delegated_focus_target(*this);
                    sync_delegated_scope_focus(focused_target(),is_scope_focused(),delegated_focus_regions(*this));
                    return true;
                }
            }
        }
    }

    const bool handled = UiControlFocusScopeHost::on_ui_input_event(event);
    sync_host_delegated_focus_target(*this);
    sync_delegated_scope_focus(focused_target(),is_scope_focused(),delegated_focus_regions(*this));
    return handled;
}

bool UiTabView::focus_first_available()
{
    UiElement* page = _selected_index && *_selected_index < child_count()
        ? child_at(*_selected_index)
        : nullptr;
    if (!page || !focus_delegated_region(page,true))
        return false;

    UiControl* target = first_focusable_control_in_delegated_region(page);
    set_focused_target(target);
    sync_delegated_scope_focus(focused_target(),is_scope_focused(),delegated_focus_regions(*this));
    return focused_target() == target;
}

UiElement* UiTabView::add_page(std::unique_ptr<UiElement> page)
{
    if (!page)
        return nullptr;
    page->set_visible(false);
    page->set_active(false);
    UiElement* added = UiControlFocusScopeHost::add_child(std::move(page));
    refresh_focus_registry();
    return added;
}

std::unique_ptr<UiElement> UiTabView::extract_page(std::size_t index)
{
    if (index >= child_count())
        return nullptr;
    set_focused_target(nullptr);
    auto page = extract_child(index);
    if (_selected_index)
    {
        if (*_selected_index == index)
            _selected_index.reset();
        else if (*_selected_index > index)
            --*_selected_index;
    }
    if (page)
    {
        page->set_visible(false);
        page->set_active(false);
    }
    sync_page_states();
    refresh_focus_registry();
    return page;
}

void UiTabView::clear_pages()
{
    set_focused_target(nullptr);
    clear_children();
    _selected_index.reset();
    refresh_focus_registry();
}

bool UiTabView::set_selected_index(std::size_t index)
{
    if (index >= child_count())
        return false;
    _selected_index = index;
    sync_page_states();
    mark_layout_dirty();
    refresh_focus_registry();
    return true;
}

void UiTabView::clear_selection() noexcept
{
    _selected_index.reset();
    sync_page_states();
    refresh_focus_registry();
}

void UiTabView::rebuild_layout()
{
    for (std::size_t i = 0; i < child_count(); ++i)
        if (UiElement* page = child_at(i))
            page->set_screen_rect(content_rect());
}

void UiTabView::rebuild_focus_registry()
{
    std::vector<FocusEntry> entries;
    if (_selected_index && *_selected_index < child_count())
    {
        UiElement* page = child_at(*_selected_index);
        build_delegated_focus_entries({ DelegatedRegionEntry{ page,nullptr,nullptr,nullptr,nullptr } },entries);
    }
    set_focus_entries(std::move(entries));
}

void UiTabView::sync_page_states() noexcept
{
    for (std::size_t i = 0; i < child_count(); ++i)
    {
        if (UiElement* page = child_at(i))
        {
            const bool selected = _selected_index && *_selected_index == i;
            page->set_visible(selected);
            page->set_active(selected);
        }
    }
    mark_layout_dirty();
}
}
