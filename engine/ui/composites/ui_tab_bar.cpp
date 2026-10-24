#include "ui_tab_bar.h"

#include "../widgets/ui_button.h"

namespace elysia::ui
{
namespace
{
constexpr float tab_width = 120.0f;
constexpr float tab_height = 40.0f;
}

UiTabBar::UiTabBar(const elysia::core::Rect& rect,int order) noexcept : UiListContainer(rect,order)
{
    reset();
}

void UiTabBar::reset() noexcept
{
    UiListContainer::reset();
    set_direction(UiListDirection::Horizontal);
    _selected = nullptr;
    _last_focused.reset();
    _on_focused_changed = {};
    _on_selected_changed = {};
    _suppress_callbacks = false;
}

void UiTabBar::update(double delta)
{
    UiListContainer::update(delta);
    sync_state(true);
}

bool UiTabBar::on_ui_input_event(const UiInputEvent& event)
{
    const bool handled = UiListContainer::on_ui_input_event(event);
    sync_state(true);
    return handled;
}

UiButton* UiTabBar::add_tab(UiTextContent content)
{
    UiButtonConfig config{};
    config.content = std::move(content);
    auto button = std::make_unique<UiButton>(
        elysia::core::Rect{ 0,0,tab_width,tab_height },config,0);
    UiButton* raw = button.get();
    raw->set_on_click([this,raw]()
    {
        if (auto index = index_of(raw))
        {
            (void)set_focused_index(*index);
            (void)set_selected_index(*index);
        }
    });
    if (!add_child(std::move(button),UiLayoutChildOptions{
            ._size_override = { tab_width,tab_height },
            ._use_size_override = true
        }))
        return nullptr;
    sync_state(false);
    return raw;
}

std::unique_ptr<UiElement> UiTabBar::extract_tab(std::size_t index)
{
    if (index >= child_count())
        return nullptr;
    UiButton* removed = button_at(index);
    const bool removed_selected = removed == _selected;
    if (removed == focused_target())
        set_focused_target(nullptr);
    auto result = extract_child(index);
    if (removed_selected)
        _selected = nullptr;
    sync_state(false);
    return result;
}

void UiTabBar::clear_tabs()
{
    _suppress_callbacks = true;
    set_focused_target(nullptr);
    clear_children();
    _selected = nullptr;
    _last_focused.reset();
    _suppress_callbacks = false;
}

std::optional<std::size_t> UiTabBar::focused_index() const noexcept
{
    return index_of(dynamic_cast<const UiButton*>(focused_target()));
}

std::optional<std::size_t> UiTabBar::selected_index() const noexcept
{
    return index_of(_selected);
}

bool UiTabBar::set_focused_index(std::size_t index)
{
    UiButton* button = button_at(index);
    if (!button)
        return false;
    const auto before = focused_index();
    set_focused_target(button);
    const auto after = focused_index();
    _last_focused = after;
    const IndexChangedCallback callback = (!_suppress_callbacks && before != after) ? _on_focused_changed : nullptr;
    if (callback)
        callback(after);
    return after == index;
}

bool UiTabBar::set_selected_index(std::size_t index)
{
    UiButton* button = button_at(index);
    if (!button)
        return false;
    const auto before = selected_index();
    _selected = button;
    refresh_styles();
    const auto selected = selected_index();
    const IndexChangedCallback callback = (!_suppress_callbacks && before != selected) ? _on_selected_changed : nullptr;
    if (callback)
        callback(selected);
    return true;
}

void UiTabBar::clear_selection() noexcept
{
    _selected = nullptr;
    refresh_styles();
}

void UiTabBar::set_on_focus_changed(IndexChangedCallback callback) { _on_focused_changed = std::move(callback); }
void UiTabBar::set_on_selection_changed(IndexChangedCallback callback) { _on_selected_changed = std::move(callback); }

UiButton* UiTabBar::button_at(std::size_t index) const noexcept
{
    return dynamic_cast<UiButton*>(const_cast<UiElement*>(child_at(index)));
}

std::optional<std::size_t> UiTabBar::index_of(const UiButton* button) const noexcept
{
    if (!button)
        return std::nullopt;
    for (std::size_t i = 0; i < child_count(); ++i)
        if (button_at(i) == button)
            return i;
    return std::nullopt;
}

void UiTabBar::sync_state(bool notify)
{
    const auto focused = focused_index();
    const bool focus_changed = focused != _last_focused;
    _last_focused = focused;
    if (_selected && !selected_index())
        _selected = nullptr;
    refresh_styles();
    const IndexChangedCallback callback = (notify && !_suppress_callbacks && focus_changed) ? _on_focused_changed : nullptr;
    if (callback)
        callback(focused);
}

void UiTabBar::refresh_styles()
{
    for (std::size_t i = 0; i < child_count(); ++i)
    {
        UiButton* button = button_at(i);
        if (!button)
            continue;
        const UiButtonVisualRole role = button == _selected ? UiButtonVisualRole::Primary : UiButtonVisualRole::Default;
        if (button->visual_role() != role)
            button->set_visual_role(role);
    }
}
}
