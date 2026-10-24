#include "ui_dropdown.h"

#include "../containers/ui_list_container.h"
#include "../layout/ui_layout_geometry.h"
#include "../style/ui_style_defaults.h"
#include "../window/ui_window.h"

#include <algorithm>
#include <utility>

namespace elysia::ui
{
using elysia::typography::UiTypographyRole;

namespace
{
[[nodiscard]] bool is_primary_mouse_press(const UiInputEvent& event) noexcept
{
    return event.type == UiInputEventType::PointerPressed
        && event.device == elysia::input::InputDevice::Mouse
        && event.control == elysia::input::RawInputControl::MouseLeft;
}
}

UiDropdown::UiDropdown(const elysia::core::Rect& rect,int order) noexcept
    : UiControl(rect,order),_trigger(rect,order)
{
    reset();
}

UiDropdown::UiDropdown(
    const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiDropdown(elysia::core::Rect(position.x,position.y,size.x,size.y),order) {}

UiDropdown::UiDropdown(
    const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiDropdown(elysia::core::Rect::from_center(center,size),order) {}

UiDropdown::~UiDropdown()
{
    unregister_from_window();
}

void UiDropdown::reset() noexcept
{
    unregister_from_window();
    UiControl::reset();

    _trigger.reset();
    _trigger.set_typography_role(UiTypographyRole::Button);
    _trigger.set_on_click([this]() { toggle(); });

    _popup_panel.reset();
    _popup_scroll.reset();
    _popup_scroll.set_scroll_axis(UiScrollAxis::Vertical);
    _popup_scroll.set_scrollbar_visibility(UiScrollBarVisibility::Auto);
    _popup_scroll.set_scroll_step(elysia::core::Vector2(0.0f,32.0f));

    _options.clear();
    _on_selection_changed = nullptr;
    _style_state.reset(UiStyleDefaults::dropdown());
    _visual_role = UiDropdownVisualRole::Default;
    _selected_index.reset();
    _focused_option.reset();
    _expanded = false;
    create_popup_content();
    sync_visual_state();
}

void UiDropdown::set_enabled(bool enabled)
{
    UiControl::set_enabled(enabled);
    if (!enabled)
        close();
    sync_visual_state();
}

void UiDropdown::set_focused(bool focused)
{
    UiControl::set_focused(focused);
    if (!_expanded)
        _trigger.set_focused(is_focused());
    else
        set_focused_option(_focused_option);
}

bool UiDropdown::on_ui_input_event(const UiInputEvent& event)
{
    sync_visual_state();
    if (_expanded)
        return on_popup_input_event(event);
    return _trigger.on_ui_input_event(event);
}

void UiDropdown::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;

    auto* self = const_cast<UiDropdown*>(this);
    self->sync_visual_state();
    _trigger.submit_ui_render_commands(out_commands);
}

void UiDropdown::set_options(std::vector<UiDropdownOption> options)
{
    const std::optional<std::size_t> previous = _selected_index;
    _options = std::move(options);
    rebuild_option_buttons();

    if (previous && *previous < _options.size() && _options[*previous].enabled)
        _selected_index = previous;
    else
        _selected_index = first_enabled_option();

    if (!_selected_index)
        close();
    _focused_option = _selected_index;
    sync_visual_state();
}

const std::vector<UiDropdownOption>& UiDropdown::options() const noexcept
{
    return _options;
}

void UiDropdown::add_option(UiDropdownOption option)
{
    _options.push_back(std::move(option));
    rebuild_option_buttons();
    if (!_selected_index)
        _selected_index = first_enabled_option();
    _focused_option = _selected_index;
    sync_visual_state();
}

void UiDropdown::clear_options()
{
    _options.clear();
    _selected_index.reset();
    _focused_option.reset();
    rebuild_option_buttons();
    close();
    sync_visual_state();
}

std::optional<std::size_t> UiDropdown::selected_index() const noexcept
{
    return _selected_index;
}

bool UiDropdown::set_selected_index(std::size_t index)
{
    if (index >= _options.size() || !_options[index].enabled)
        return false;

    const bool changed = _selected_index != index;
    _selected_index = index;
    _focused_option = index;
    sync_visual_state();
    const UiDropdownSelectionChangedCallback callback = changed ? _on_selection_changed : nullptr;
    if (callback)
        callback(index);
    return true;
}

void UiDropdown::set_on_selection_changed(UiDropdownSelectionChangedCallback on_selection_changed)
{
    _on_selection_changed = std::move(on_selection_changed);
}

void UiDropdown::open()
{
    if (_expanded || !is_enabled() || !_selected_index || !_window)
        return;

    _expanded = true;
    _focused_option = _selected_index;
    sync_popup_layout();
    set_focused_option(_focused_option);
    _window->activate_transient_popup(*this);
}

void UiDropdown::close() noexcept
{
    _expanded = false;
    _focused_option.reset();
    _popup_scroll.set_scope_focused(false);
    _trigger.set_focused(is_focused());
}

void UiDropdown::toggle()
{
    if (_expanded)
        close();
    else
        open();
}

bool UiDropdown::is_open() const noexcept
{
    return _expanded;
}

void UiDropdown::register_with_window(UiWindow& window)
{
    if (_window && _window != &window)
        _window->unregister_transient_popup(*this);
    _window = &window;
    _window->register_transient_popup(*this);
}

void UiDropdown::unregister_from_window() noexcept
{
    if (_window)
        _window->unregister_transient_popup(*this);
    _window = nullptr;
    close();
}

void UiDropdown::set_base_style(const UiDropdownBaseStyle& style) noexcept
{
    _style_state.set_base_style(style.layout);
    _trigger.set_base_style(style.trigger);
    _popup_panel.set_base_style(style.popup);
    _popup_scroll.set_base_style(style.scroll);
    if (_popup_list)
        for (std::size_t index = 0; index < _popup_list->child_count(); ++index)
            if (UiButton* button = option_button_at(index)) button->set_base_style(style.option);
    if (_expanded) sync_popup_layout();
}

void UiDropdown::set_style_overrides(const UiDropdownStyleOverrides& overrides) noexcept
{
    _style_state.set_style_overrides(overrides);
    if (_expanded)
        sync_popup_layout();
}

const UiDropdownStyle& UiDropdown::style() const noexcept
{
    return _style_state.effective_style();
}

const UiDropdownStyleOverrides& UiDropdown::style_overrides() const noexcept { return _style_state.style_overrides(); }
bool UiDropdown::has_style_overrides() const noexcept
{
    return _style_state.has_style_overrides();
}

void UiDropdown::clear_style_overrides() noexcept
{
    _style_state.clear_style_overrides();
    if (_expanded)
        sync_popup_layout();
}

void UiDropdown::set_visual_role(UiDropdownVisualRole role) noexcept
{
    _visual_role = role;
    notify_base_style_invalidated();
}

UiDropdownVisualRole UiDropdown::visual_role() const noexcept
{
    return _visual_role;
}

UiElement& UiDropdown::popup_owner() noexcept
{
    return *this;
}

const UiElement& UiDropdown::popup_owner() const noexcept
{
    return *this;
}

bool UiDropdown::contains_popup_point(int mouse_x,int mouse_y) const noexcept
{
    elysia::core::Vector2 popup_point(static_cast<float>(mouse_x),static_cast<float>(mouse_y));
    if (_window)
        popup_point -= _window->presentation_translation();
    return contains_trigger_point(mouse_x,mouse_y)
        || (_expanded && _popup_panel.screen_rect().contains(popup_point));
}

void UiDropdown::on_window_detached(UiWindow& window) noexcept
{
    if (_window != &window)
        return;
    _window = nullptr;
    close();
}

bool UiDropdown::on_popup_input_event(const UiInputEvent& event)
{
    if (!_expanded)
        return false;

    sync_popup_layout();
    if (event.type == UiInputEventType::ActionPressed && event.action == UiAction::Cancel)
    {
        close();
        return true;
    }

    if (event.type == UiInputEventType::ActionPressed
        && (event.action == UiAction::NavigateUp || event.action == UiAction::NavigateDown))
    {
        set_focused_option(next_enabled_option(event.action == UiAction::NavigateUp ? -1 : 1));
        ensure_focused_option_visible();
        return _focused_option.has_value();
    }

    if (event.action == UiAction::Confirm)
    {
        if (event.type == UiInputEventType::ActionPressed)
            return true;
        if (event.type == UiInputEventType::ActionReleased && _focused_option)
        {
            return commit_selected_index(*_focused_option);
        }
    }

    if (is_pointer_event(event) && contains_trigger_point(event.mouse_x,event.mouse_y))
        return _trigger.on_ui_input_event(event);

    UiInputEvent popup_event = event;
    if (_window && is_pointer_event(popup_event))
    {
        const elysia::core::Vector2 local_pointer = _window->presentation_to_layout_point(
            elysia::core::Vector2(static_cast<float>(event.mouse_x),static_cast<float>(event.mouse_y)));
        popup_event.mouse_x = static_cast<int>(local_pointer.x);
        popup_event.mouse_y = static_cast<int>(local_pointer.y);
    }
    if (is_pointer_event(event) && _popup_panel.screen_rect().contains(elysia::core::Vector2(
        static_cast<float>(popup_event.mouse_x),static_cast<float>(popup_event.mouse_y))))
    {
        const bool handled = _popup_scroll.on_ui_input_event(popup_event);
        // Pointer focus is resolved by UiListContainer. Mirror that result only
        // after dispatch so keyboard/gamepad confirmation continues from the
        // option the user actually sees focused.
        if (_expanded)
            sync_focused_option_from_popup_list();
        return handled;
    }

    return false;
}

void UiDropdown::submit_popup_render_commands(
    std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!_expanded || !is_visible())
        return;

    auto* self = const_cast<UiDropdown*>(this);
    self->sync_popup_layout();
    _popup_panel.submit_ui_render_commands(out_commands);
    _popup_scroll.submit_ui_render_commands(out_commands);
}

void UiDropdown::create_popup_content()
{
    auto list = std::make_unique<UiListContainer>();
    list->set_direction(UiListDirection::Vertical);
    list->set_item_spacing(0.0f);
    _popup_list = list.get();
    _popup_scroll.set_content(std::move(list));
}

void UiDropdown::rebuild_option_buttons()
{
    if (!_popup_list)
        return;

    _popup_list->clear_children();
    const float option_height = std::max(1.0f,style().option_height);
    const float width = std::max(0.0f,size().x);
    for (std::size_t index = 0; index < _options.size(); ++index)
    {
        auto button = std::make_unique<UiButton>(elysia::core::Rect{ 0,0,width,option_height });
        button->set_text_content(_options[index].content);
        button->set_typography_role(UiTypographyRole::Button);
        button->set_enabled(_options[index].enabled);
        button->set_on_click([this,index]()
        {
            (void)commit_selected_index(index);
        });
        _popup_list->add_back(std::move(button));
    }

    // clear_children() invalidates the list's previously cached focus entries.
    // Rebuild them immediately so a later close/unregister cannot apply focus
    // state through pointers to the destroyed option buttons.
    _popup_list->set_focused_target(nullptr);
}

void UiDropdown::sync_visual_state()
{
    const bool has_selection = _selected_index.has_value() && *_selected_index < _options.size();
    _trigger.set_screen_rect(screen_rect());
    _trigger.set_visible(is_visible());
    _trigger.set_active(is_active());
    _trigger.set_opacity(opacity());
    _trigger.set_enabled(is_enabled() && has_selection);
    _trigger.set_text_content(has_selection ? _options[*_selected_index].content : UiTextContent{});
    if (!_expanded)
        _trigger.set_focused(is_focused());
}

void UiDropdown::sync_popup_layout()
{
    if (!_expanded || !_popup_list)
        return;

    const UiDropdownStyle& current_style = style();
    elysia::core::Vector2 popup_translation = accumulated_presentation_translation();
    if (_window)
        popup_translation -= _window->presentation_translation();
    const elysia::core::Rect trigger_rect = screen_rect().translated(popup_translation);
    const elysia::core::Rect bounds = _window
        ? _window->content_bounds().translated(
            _window->accumulated_presentation_translation() - _window->presentation_translation())
        : trigger_rect;
    const float width = trigger_rect.width();
    const float gap = std::max(0.0f,current_style.popup_gap);
    const float desired_height = static_cast<float>(_options.size()) * std::max(1.0f,current_style.option_height);
    const float target_height = std::min(desired_height,std::max(0.0f,current_style.popup_max_height));
    const float available_down = std::max(0.0f,bounds.bottom() - trigger_rect.bottom() - gap);
    const float available_up = std::max(0.0f,trigger_rect.top() - bounds.top() - gap);
    const bool open_upward = available_down < target_height && available_up > available_down;
    const float available_height = open_upward ? available_up : available_down;
    const float height = std::min(target_height,available_height);
    const float max_left = std::max(bounds.left(),bounds.right() - width);
    const float left = std::clamp(trigger_rect.left(),bounds.left(),max_left);
    const float top = open_upward ? trigger_rect.top() - gap - height : trigger_rect.bottom() + gap;
    const elysia::core::Rect popup_rect(left,top,width,height);

    _popup_panel.set_screen_rect(popup_rect);
    _popup_panel.set_visible(is_visible());
    _popup_panel.set_active(is_active());
    _popup_panel.set_opacity(opacity());
    _popup_scroll.set_screen_rect(popup_rect);
    _popup_scroll.set_visible(is_visible());
    _popup_scroll.set_active(is_active());
    _popup_scroll.set_opacity(opacity());
    // An expanded transient popup is the active input region even when the
    // owner control was opened with the pointer and its parent scope has not
    // assigned keyboard focus to it. Keep the popup scope active while open so
    // its list can own and render the focused option consistently.
    _popup_scroll.set_scope_focused(_expanded && is_visible() && is_active());
    _popup_list->set_size(elysia::core::Vector2(width,desired_height));

    for (std::size_t index = 0; index < _popup_list->child_count(); ++index)
    {
        if (UiButton* button = option_button_at(index))
            button->set_size(elysia::core::Vector2(width,std::max(1.0f,current_style.option_height)));
    }

    _popup_scroll.mark_layout_dirty();
    _popup_scroll.update_layout_if_dirty();
}


std::optional<std::size_t> UiDropdown::first_enabled_option() const noexcept
{
    for (std::size_t index = 0; index < _options.size(); ++index)
    {
        if (_options[index].enabled)
            return index;
    }
    return std::nullopt;
}

std::optional<std::size_t> UiDropdown::next_enabled_option(int direction) const noexcept
{
    if (_options.empty())
        return std::nullopt;

    const std::size_t start = _focused_option.value_or(_selected_index.value_or(0));
    for (std::size_t step = 1; step <= _options.size(); ++step)
    {
        const std::size_t index = direction < 0
            ? (start + _options.size() - (step % _options.size())) % _options.size()
            : (start + step) % _options.size();
        if (_options[index].enabled)
            return index;
    }
    return std::nullopt;
}

bool UiDropdown::commit_selected_index(std::size_t index)
{
    if (index >= _options.size() || !_options[index].enabled)
        return false;
    const bool changed = _selected_index != index;
    _selected_index = index;
    _focused_option = index;
    sync_visual_state();
    close();
    const UiDropdownSelectionChangedCallback callback = changed ? _on_selection_changed : nullptr;
    if (callback)
        callback(index);
    return true;
}

void UiDropdown::set_focused_option(std::optional<std::size_t> index)
{
    _focused_option = index;
    _trigger.set_focused(!_expanded && is_focused());
    if (!_popup_list)
        return;

    UiButton* focused_button = index ? option_button_at(*index) : nullptr;
    // Let the list focus host own the selection. Setting UiButton::focused
    // directly is only temporary: the list repairs/applies its own focus state
    // on the next input event and would otherwise overwrite the dropdown's
    // focused option (usually back to the first entry).
    _popup_list->set_focused_target(focused_button);
}

void UiDropdown::sync_focused_option_from_popup_list()
{
    if (!_popup_list)
    {
        _focused_option.reset();
        return;
    }

    const UiControl* focused = _popup_list->focused_target();
    for (std::size_t index = 0; index < _options.size(); ++index)
    {
        if (_options[index].enabled && option_button_at(index) == focused)
        {
            _focused_option = index;
            return;
        }
    }

    _focused_option.reset();
}

void UiDropdown::ensure_focused_option_visible() noexcept
{
    if (!_focused_option)
        return;
    if (UiButton* button = option_button_at(*_focused_option))
        _popup_scroll.ensure_visible(button->screen_rect());
}

UiButton* UiDropdown::option_button_at(std::size_t index) noexcept
{
    return _popup_list ? dynamic_cast<UiButton*>(_popup_list->child_at(index)) : nullptr;
}

const UiButton* UiDropdown::option_button_at(std::size_t index) const noexcept
{
    return _popup_list ? dynamic_cast<const UiButton*>(_popup_list->child_at(index)) : nullptr;
}

bool UiDropdown::contains_trigger_point(int mouse_x,int mouse_y) const noexcept
{
    return presentation_screen_rect().contains(elysia::core::Vector2(static_cast<float>(mouse_x),static_cast<float>(mouse_y)));
}

bool UiDropdown::is_pointer_event(const UiInputEvent& event) const noexcept
{
    return event.type == UiInputEventType::MouseMoved
        || event.type == UiInputEventType::PointerPressed
        || event.type == UiInputEventType::PointerReleased
        || event.type == UiInputEventType::MouseWheel;
}

}
