#include "ui_tab_container.h"

#include "ui_tab_bar.h"
#include "../containers/ui_tab_view.h"

#include <algorithm>
#include <cassert>

namespace elysia::ui
{
namespace { constexpr float tab_bar_height = 44.0f; }

UiTabContainer::UiTabContainer(const elysia::core::Rect& rect,int order) noexcept : UiControlFocusScopeHost(rect,order)
{
    reset();
}

void UiTabContainer::reset() noexcept
{
    UiControlFocusScopeHost::reset();
    reset_delegated_focus_state();
    _tab_bar = nullptr;
    _tab_view = nullptr;
    _on_focused_changed = {};
    _on_selected_changed = {};
    _mutating = false;
    create_internal_children();
    assert_invariant();
}

void UiTabContainer::update(double delta)
{
    assert_invariant();
    UiControlFocusScopeHost::update(delta);
    sync_host_delegated_focus_target(*this);
    sync_delegated_scope_focus(focused_target(),is_scope_focused(),delegated_focus_regions(*this));
    assert_invariant();
}

void UiTabContainer::on_ui_input_frame(const UiInputFrame& input)
{
    UiControlFocusScopeHost::on_ui_input_frame(input);
    sync_host_delegated_focus_target(*this);
    sync_delegated_scope_focus(focused_target(),is_scope_focused(),delegated_focus_regions(*this));
}

bool UiTabContainer::on_ui_input_event(const UiInputEvent& event)
{
    if (event.type == UiInputEventType::PointerPressed && event.device == elysia::input::InputDevice::Mouse
        && event.control == elysia::input::RawInputControl::MouseLeft)
    {
        update_focus_input_device(event.device);
        synchronize_focus_state();
        (void)set_focused_target_internal(find_registered_target_at(event.mouse_x,event.mouse_y));
        // Activate the owning page before its button records the press.
        sync_delegated_scope_focus(focused_target(),is_scope_focused(),delegated_focus_regions(*this));
    }
    const bool is_confirm = (event.action == UiAction::Confirm || event.action == UiAction::Cancel)
        && (event.type == UiInputEventType::ActionPressed || event.type == UiInputEventType::ActionReleased);
    const bool is_navigation = event.type == UiInputEventType::ActionPressed && is_navigation_action(event.action);

    if (is_confirm || is_navigation)
    {
        if (UiFocusScope* scope = delegated_owner_scope_of(focused_target()))
        {
            if (is_navigation && scope == _tab_bar && event.action == UiAction::NavigateDown)
            {
                if (_tab_view->focus_first_available())
                {
                    UiControl* target = _tab_view->focused_target();
                    set_focused_target(target);
                    if (focused_target() == target)
                    {
                        sync_delegated_scope_focus(focused_target(),is_scope_focused(),delegated_focus_regions(*this));
                        return true;
                    }
                }
            }
            if (auto* receiver = dynamic_cast<UiInputEventReceiver*>(&scope->focus_scope_element()))
            {
                if (receiver->on_ui_input_event(event))
                {
                    sync_host_delegated_focus_target(*this);
                    sync_delegated_scope_focus(focused_target(),is_scope_focused(),delegated_focus_regions(*this));
                    return true;
                }
            }
            if (is_navigation && scope == _tab_view && event.action == UiAction::NavigateUp)
            {
                UiControl* target = _tab_bar->focused_target();
                set_focused_target(target);
                if (focused_target() == target)
                {
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

bool UiTabContainer::focus_first_available()
{
    if (!_tab_bar || !_tab_bar->focus_first_available())
        return false;

    UiControl* target = _tab_bar->focused_target();
    set_focused_target(target);
    sync_delegated_scope_focus(focused_target(),is_scope_focused(),delegated_focus_regions(*this));
    return focused_target() == target;
}

UiTabAddResult UiTabContainer::add_tab(UiTextContent label,std::unique_ptr<UiElement> page)
{
    assert_invariant();
    if (!page)
        return { false,nullptr };

    _mutating = true;
    UiElement* page_raw = _tab_view->add_page(std::move(page));
    if (!page_raw)
    {
        _mutating = false;
        return { false,nullptr };
    }

    if (!_tab_bar->add_tab(std::move(label)))
    {
        auto rejected = _tab_view->extract_page(_tab_view->page_count() - 1);
        _mutating = false;
        assert_invariant();
        return { false,std::move(rejected) };
    }

    if (tab_count() == 1)
    {
        (void)_tab_bar->set_selected_index(0);
        (void)_tab_view->set_selected_index(0);
    }
    _mutating = false;
    mark_layout_dirty();
    refresh_focus_registry();
    assert_invariant();
    return { true,nullptr };
}

std::unique_ptr<UiElement> UiTabContainer::remove_tab(std::size_t index)
{
    assert_invariant();
    if (index >= tab_count())
        return nullptr;

    const auto old_focused = focused_index();
    const auto old_selected = selected_index();
    _mutating = true;
    auto removed_tab = _tab_bar->extract_tab(index);
    auto removed_page = _tab_view->extract_page(index);
    assert(removed_tab && removed_page);

    const std::size_t count = tab_count();
    if (count == 0)
    {
        _tab_bar->clear_selection();
        _tab_view->clear_selection();
    }
    else
    {
        if (old_selected)
        {
            const std::size_t next = *old_selected == index
                ? std::min(index,count - 1)
                : (*old_selected > index ? *old_selected - 1 : *old_selected);
            (void)_tab_bar->set_selected_index(next);
            (void)_tab_view->set_selected_index(next);
        }
        if (old_focused)
        {
            const std::size_t next = *old_focused == index
                ? std::min(index,count - 1)
                : (*old_focused > index ? *old_focused - 1 : *old_focused);
            (void)_tab_bar->set_focused_index(next);
        }
    }
    _mutating = false;
    mark_layout_dirty();
    refresh_focus_registry();
    assert_invariant();
    if (_on_selected_changed && old_selected != selected_index())
        _on_selected_changed(selected_index());
    if (_on_focused_changed && old_focused != focused_index())
        _on_focused_changed(focused_index());
    return removed_page;
}

void UiTabContainer::clear_tabs()
{
    assert_invariant();
    const auto old_focused = focused_index();
    const auto old_selected = selected_index();
    _mutating = true;
    _tab_bar->clear_tabs();
    _tab_view->clear_pages();
    _mutating = false;
    refresh_focus_registry();
    assert_invariant();
    if (_on_selected_changed && old_selected)
        _on_selected_changed(std::nullopt);
    if (_on_focused_changed && old_focused)
        _on_focused_changed(std::nullopt);
}

std::size_t UiTabContainer::tab_count() const noexcept { return _tab_bar ? _tab_bar->tab_count() : 0; }
std::size_t UiTabContainer::page_count() const noexcept { return _tab_view ? _tab_view->page_count() : 0; }
std::optional<std::size_t> UiTabContainer::focused_index() const noexcept { return _tab_bar ? _tab_bar->focused_index() : std::nullopt; }
std::optional<std::size_t> UiTabContainer::selected_index() const noexcept { return _tab_bar ? _tab_bar->selected_index() : std::nullopt; }
bool UiTabContainer::set_focused_index(std::size_t index) { assert_invariant(); return _tab_bar && _tab_bar->set_focused_index(index); }
bool UiTabContainer::set_selected_index(std::size_t index)
{
    assert_invariant();
    if (!_tab_bar || !_tab_view || index >= tab_count())
        return false;
    const auto before = selected_index();
    _mutating = true;
    const bool bar_ok = _tab_bar->set_selected_index(index);
    const bool view_ok = _tab_view->set_selected_index(index);
    _mutating = false;
    assert(bar_ok && view_ok);
    if (bar_ok && view_ok && before != selected_index() && _on_selected_changed)
        _on_selected_changed(selected_index());
    return bar_ok && view_ok;
}
void UiTabContainer::set_on_focus_changed(IndexChangedCallback callback) { _on_focused_changed = std::move(callback); }
void UiTabContainer::set_on_selection_changed(IndexChangedCallback callback) { _on_selected_changed = std::move(callback); }

void UiTabContainer::rebuild_layout()
{
    const auto rect = content_rect();
    if (_tab_bar)
        _tab_bar->set_screen_rect(elysia::core::Rect{ rect.left(),rect.top(),rect.width(),std::min(tab_bar_height,rect.height()) });
    if (_tab_view)
        _tab_view->set_screen_rect(elysia::core::Rect{ rect.left(),rect.top() + tab_bar_height,rect.width(),std::max(0.0f,rect.height() - tab_bar_height) });
}

void UiTabContainer::rebuild_focus_registry()
{
    std::vector<FocusEntry> entries;
    build_delegated_focus_entries({
        DelegatedRegionEntry{ _tab_bar,nullptr,_tab_view,nullptr,nullptr },
        DelegatedRegionEntry{ _tab_view,_tab_bar,nullptr,nullptr,nullptr }
    },entries);
    set_focus_entries(std::move(entries));
}

UiElement* UiTabContainer::add_child(std::unique_ptr<UiElement> child,UiLayoutChildOptions options)
{
    if (_tab_bar && _tab_view)
        return nullptr;
    return UiControlFocusScopeHost::add_child(std::move(child),options);
}

void UiTabContainer::create_internal_children()
{
    auto bar = std::make_unique<UiTabBar>();
    _tab_bar = bar.get();
    UiControlFocusScopeHost::add_child(std::move(bar));
    auto view = std::make_unique<UiTabView>();
    _tab_view = view.get();
    UiControlFocusScopeHost::add_child(std::move(view));
    _tab_bar->set_on_selection_changed([this](auto index) { handle_selected_changed(index); });
    _tab_bar->set_on_focus_changed([this](auto index) { handle_focused_changed(index); });
}

void UiTabContainer::assert_invariant() const noexcept { assert(tab_count() == page_count()); }
void UiTabContainer::handle_selected_changed(std::optional<std::size_t> index)
{
    if (_mutating)
        return;
    assert_invariant();
    if (index)
        (void)_tab_view->set_selected_index(*index);
    else
        _tab_view->clear_selection();
    refresh_focus_registry();
    if (_on_selected_changed)
        _on_selected_changed(index);
}
void UiTabContainer::handle_focused_changed(std::optional<std::size_t> index)
{
    if (!_mutating && _on_focused_changed)
        _on_focused_changed(index);
}
}
