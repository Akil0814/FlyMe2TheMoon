#include "ui_button_group.h"

#include <utility>

namespace elysia::ui
{
UiButtonGroup::UiButtonGroup(const elysia::core::Rect& rect,int order) noexcept
    : UiListContainer(rect,order) {}

UiButtonGroup::UiButtonGroup(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiListContainer(position,size,order) {}

UiButtonGroup::UiButtonGroup(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiListContainer(center,size,from_center,order) {}

void UiButtonGroup::reset() noexcept
{
    UiListContainer::reset();
    _selected_button = nullptr;
    _auto_select_first = true;
    _is_syncing_selection = false;
}

void UiButtonGroup::update(double delta)
{
    cleanup_destroyed_children();
    sync_selection();
    UiListContainer::update(delta);
    cleanup_destroyed_children();
    sync_selection();
}

void UiButtonGroup::on_ui_input_frame(const UiInputFrame& input)
{
    cleanup_destroyed_children();
    sync_selection();
    UiListContainer::on_ui_input_frame(input);
    cleanup_destroyed_children();
    sync_selection();
}

bool UiButtonGroup::on_ui_input_event(const UiInputEvent& event)
{
    cleanup_destroyed_children();
    sync_selection();
    const bool handled = UiListContainer::on_ui_input_event(event);
    cleanup_destroyed_children();
    sync_selection();
    return handled;
}

void UiButtonGroup::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    auto* self = const_cast<UiButtonGroup*>(this);
    self->cleanup_destroyed_children();
    self->sync_selection();
    UiListContainer::submit_ui_render_commands(out_commands);
}

UiButton* UiButtonGroup::add_button(std::unique_ptr<UiButton> button)
{
    if (!button)
        return nullptr;

    UiButton* raw_button = button.get();
    raw_button->prepend_on_click([this,raw_button]()
    {
        (void)select_button(raw_button);
    });

    if (!UiListContainer::add_back(std::move(button)))
        return nullptr;

    sync_selection();
    return raw_button;
}

std::optional<std::size_t> UiButtonGroup::selected_index() const noexcept
{
    return find_button_index(_selected_button);
}

bool UiButtonGroup::set_selected_index(std::size_t index)
{
    cleanup_destroyed_children();
    return select_button(button_at(index));
}

void UiButtonGroup::set_auto_select_first(bool enabled) noexcept
{
    _auto_select_first = enabled;
}

bool UiButtonGroup::auto_select_first() const noexcept
{
    return _auto_select_first;
}

bool UiButtonGroup::select_button(UiButton* button)
{
    if (!find_button_index(button))
        return false;

    if (_selected_button == button)
    {
        refresh_button_styles();
        return true;
    }

    _selected_button = button;
    refresh_button_styles();
    return true;
}

std::optional<std::size_t> UiButtonGroup::find_button_index(const UiButton* button) const noexcept
{
    if (!button)
        return std::nullopt;

    for (std::size_t index = 0; index < child_count(); ++index)
    {
        if (button_at(index) == button)
            return index;
    }
    return std::nullopt;
}

UiButton* UiButtonGroup::button_at(std::size_t index) const noexcept
{
    const UiElement* child = child_at(index);
    return child ? dynamic_cast<UiButton*>(const_cast<UiElement*>(child)) : nullptr;
}

void UiButtonGroup::sync_selection()
{
    if (_is_syncing_selection)
        return;

    _is_syncing_selection = true;

    if (!find_button_index(_selected_button))
    {
        _selected_button = nullptr;
        if (_auto_select_first)
        {
            for (std::size_t index = 0; index < child_count(); ++index)
            {
                if (UiButton* button = button_at(index))
                {
                    _selected_button = button;
                    break;
                }
            }
        }
    }

    refresh_button_styles();
    _is_syncing_selection = false;

}

void UiButtonGroup::refresh_button_styles() noexcept
{
    for (std::size_t index = 0; index < child_count(); ++index)
    {
        UiButton* button = button_at(index);
        if (!button)
            continue;

        const UiButtonVisualRole role = button == _selected_button
            ? UiButtonVisualRole::Primary
            : UiButtonVisualRole::Default;
        if (button->visual_role() == role)
            continue;

        button->set_visual_role(role);
    }
}

}
