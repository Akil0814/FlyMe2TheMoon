#include "ui_option_selector.h"

#include "../style/ui_style_defaults.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace elysia::ui
{
namespace
{
float finite_non_negative(float value) noexcept
{
    return std::isfinite(value) ? std::max(0.0f,value) : 0.0f;
}

UiOptionSelectorStyle default_style() noexcept
{
    UiOptionSelectorStyle result;
    result.chrome = UiStyleDefaults::interactive_chrome();
    result.arrow = UiStyleDefaults::button();
    result.text = UiStyleDefaults::text();
    return result;
}

bool is_direction(UiAction action) noexcept
{
    return action == UiAction::NavigateLeft || action == UiAction::NavigateRight
        || action == UiAction::NavigateUp || action == UiAction::NavigateDown;
}
}

UiOptionSelector::UiOptionSelector(const elysia::core::Rect& rect,int order) noexcept
    : UiControl(rect,order)
{
    reset();
}

UiOptionSelector::UiOptionSelector(const elysia::core::Vector2& position,
    const elysia::core::Vector2& size,int order) noexcept : UiOptionSelector(elysia::core::Rect{position,size},order) {}

UiOptionSelector::UiOptionSelector(const elysia::core::Vector2& center,
    const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiOptionSelector(elysia::core::Rect::from_center(center,size),order) {}

void UiOptionSelector::reset() noexcept
{
    UiControl::reset();
    _previous.reset();
    _next.reset();
    _label.reset();
    _options.clear();
    _selected_index.reset();
    _on_selection_changed = nullptr;
    _style_state.reset(default_style());
    _orientation = UiOptionSelectorOrientation::Horizontal;
    _wrap_selection = false;
    _hovered_region = Region::None;
    cancel_input_interaction();
}

void UiOptionSelector::cancel_input_interaction() noexcept
{
    _is_adjusting = false;
    _pressed_region = Region::None;
    _previous.cancel_input_interaction();
    _next.cancel_input_interaction();
}

void UiOptionSelector::set_enabled(bool enabled)
{
    UiControl::set_enabled(enabled);
    if (!enabled)
        cancel_input_interaction();
}

void UiOptionSelector::set_focused(bool focused)
{
    UiControl::set_focused(focused);
    if (!focused)
    {
        cancel_input_interaction();
        _hovered_region = Region::None;
    }
}

void UiOptionSelector::set_options(std::vector<UiOptionSelectorOption> options)
{
    cancel_input_interaction();
    _options = std::move(options);
    if (!_selected_index || *_selected_index >= _options.size() || !_options[*_selected_index].enabled)
    {
        _selected_index.reset();
        for (std::size_t index = 0; index < _options.size(); ++index)
            if (_options[index].enabled)
            {
                _selected_index = index;
                break;
            }
    }
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

const std::vector<UiOptionSelectorOption>& UiOptionSelector::options() const noexcept { return _options; }
void UiOptionSelector::clear_options() { set_options({}); }
std::optional<std::size_t> UiOptionSelector::selected_index() const noexcept { return _selected_index; }

bool UiOptionSelector::set_selected_index(std::size_t index)
{
    if (index >= _options.size() || !_options[index].enabled)
        return false;
    if (_selected_index == index)
        return true;
    _selected_index = index;
    _pressed_region = Region::None;
    _previous.cancel_input_interaction();
    _next.cancel_input_interaction();
    sync_children();
    const auto callback = _on_selection_changed;
    if (callback)
        callback(index);
    return true;
}

std::optional<std::size_t> UiOptionSelector::adjacent_index(int direction) const noexcept
{
    if (!_selected_index)
        return std::nullopt;
    std::size_t index = *_selected_index;
    for (std::size_t visited = 1; visited < _options.size(); ++visited)
    {
        if (direction < 0)
        {
            if (index == 0)
            {
                if (!_wrap_selection) break;
                index = _options.size();
            }
            --index;
        }
        else
        {
            ++index;
            if (index == _options.size())
            {
                if (!_wrap_selection) break;
                index = 0;
            }
        }
        if (_options[index].enabled)
            return index;
    }
    return std::nullopt;
}

bool UiOptionSelector::select_previous()
{
    const auto index = adjacent_index(-1);
    return index && set_selected_index(*index);
}

bool UiOptionSelector::select_next()
{
    const auto index = adjacent_index(1);
    return index && set_selected_index(*index);
}

void UiOptionSelector::set_on_selection_changed(UiOptionSelectorSelectionChangedCallback callback)
{
    _on_selection_changed = std::move(callback);
}

void UiOptionSelector::set_orientation(UiOptionSelectorOrientation orientation) noexcept
{
    if (_orientation == orientation) return;
    cancel_input_interaction();
    _orientation = orientation;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}
UiOptionSelectorOrientation UiOptionSelector::orientation() const noexcept { return _orientation; }
void UiOptionSelector::set_wrap_selection(bool wrap) noexcept { _wrap_selection = wrap; }
bool UiOptionSelector::wraps_selection() const noexcept { return _wrap_selection; }
bool UiOptionSelector::is_adjusting() const noexcept { return _is_adjusting; }

bool UiOptionSelector::can_receive_pointer() const noexcept
{
    return is_visible() && is_active() && is_enabled() && !is_destroyed();
}
bool UiOptionSelector::can_interact() const noexcept { return can_receive_pointer() && is_focused(); }

void UiOptionSelector::toggle_adjustment() noexcept
{
    _is_adjusting = !_is_adjusting && (adjacent_index(-1).has_value() || adjacent_index(1).has_value());
}

UiOptionSelector::Layout UiOptionSelector::compute_layout() const noexcept
{
    const auto& rect = screen_rect();
    const auto& current = style();
    const float pad_x = std::min(finite_non_negative(current.padding),rect.width() * 0.5f);
    const float pad_y = std::min(finite_non_negative(current.padding),rect.height() * 0.5f);
    const elysia::core::Rect interior{rect.x() + pad_x,rect.y() + pad_y,
        rect.width() - 2 * pad_x,rect.height() - 2 * pad_y};
    const bool horizontal = _orientation == UiOptionSelectorOrientation::Horizontal;
    const float extent = horizontal ? interior.width() : interior.height();
    const float side = std::min(finite_non_negative(current.arrow_extent),extent * 0.5f);
    const float gap = std::min(finite_non_negative(current.spacing),(extent - 2 * side) * 0.5f);
    const float center_extent = std::max(0.0f,extent - 2 * (side + gap));
    if (horizontal)
        return {{interior.x(),interior.y(),side,interior.height()},
            {interior.x() + side + gap,interior.y(),center_extent,interior.height()},
            {interior.right() - side,interior.y(),side,interior.height()}};
    return {{interior.x(),interior.y(),interior.width(),side},
        {interior.x(),interior.y() + side + gap,interior.width(),center_extent},
        {interior.x(),interior.bottom() - side,interior.width(),side}};
}

UiOptionSelector::Region UiOptionSelector::region_at(const UiInputEvent& event) const noexcept
{
    const auto point = presentation_to_layout_point({static_cast<float>(event.mouse_x),static_cast<float>(event.mouse_y)});
    const auto layout = compute_layout();
    if (!layout.previous.is_empty() && layout.previous.contains(point)) return Region::Previous;
    if (!layout.next.is_empty() && layout.next.contains(point)) return Region::Next;
    if (!layout.center.is_empty() && layout.center.contains(point)) return Region::Center;
    return Region::None;
}

bool UiOptionSelector::on_ui_input_event(const UiInputEvent& event)
{
    if (!can_receive_pointer())
    {
        cancel_input_interaction();
        return false;
    }
    const bool pointer_event = event.type == UiInputEventType::MouseMoved
        || event.type == UiInputEventType::PointerPressed || event.type == UiInputEventType::PointerReleased;
    if (pointer_event)
    {
        const bool primary = event.device == elysia::input::InputDevice::Mouse
            && event.control == elysia::input::RawInputControl::MouseLeft;
        if (event.type != UiInputEventType::MouseMoved && !primary) return false;
        const auto region = region_at(event);
        _hovered_region = region;
        if (event.type == UiInputEventType::MouseMoved && _pressed_region != region)
        {
            _pressed_region = Region::None;
            _previous.cancel_input_interaction();
            _next.cancel_input_interaction();
        }
        if (event.type == UiInputEventType::MouseMoved || event.type == UiInputEventType::PointerPressed)
            set_focused(presentation_screen_rect().contains({static_cast<float>(event.mouse_x),static_cast<float>(event.mouse_y)}));
        sync_children();
        UiInputEvent local = event;
        const auto point = presentation_to_layout_point({static_cast<float>(event.mouse_x),static_cast<float>(event.mouse_y)});
        local.mouse_x = static_cast<int>(std::lround(point.x));
        local.mouse_y = static_cast<int>(std::lround(point.y));
        (void)_previous.on_ui_input_event(local);
        (void)_next.on_ui_input_event(local);
        if (event.type == UiInputEventType::MouseMoved) return false;
        if (event.type == UiInputEventType::PointerPressed)
        {
            _pressed_region = region;
            if ((region == Region::Previous && !adjacent_index(-1))
                || (region == Region::Next && !adjacent_index(1)))
                _pressed_region = Region::None;
            return region != Region::None;
        }
        const auto pressed = std::exchange(_pressed_region,Region::None);
        if (pressed == Region::None) return false;
        if (pressed != region) return true;
        if (region == Region::Center) { toggle_adjustment(); return true; }
        // Internal buttons have no callbacks; external notification is always the final operation.
        if (region == Region::Previous) { (void)select_previous(); return true; }
        if (region == Region::Next) { (void)select_next(); return true; }
        return true;
    }
    if (!can_interact()) return false;
    if (event.action == UiAction::Tab && event.type == UiInputEventType::ActionPressed)
    {
        cancel_input_interaction();
        return false;
    }
    if (event.action == UiAction::Confirm)
    {
        if (event.type == UiInputEventType::ActionPressed) toggle_adjustment();
        return event.type == UiInputEventType::ActionPressed || event.type == UiInputEventType::ActionReleased;
    }
    if (event.action == UiAction::Cancel && _is_adjusting)
    {
        if (event.type == UiInputEventType::ActionPressed) cancel_input_interaction();
        return true;
    }
    if (!_is_adjusting || !is_direction(event.action)) return false;
    if (event.type == UiInputEventType::ActionPressed)
    {
        const bool horizontal = _orientation == UiOptionSelectorOrientation::Horizontal;
        if (event.action == (horizontal ? UiAction::NavigateLeft : UiAction::NavigateUp))
            (void)select_previous();
        else if (event.action == (horizontal ? UiAction::NavigateRight : UiAction::NavigateDown))
            (void)select_next();
    }
    return true;
}

void UiOptionSelector::sync_children() const
{
    const auto layout = compute_layout();
    const auto sync_button = [&](UiButton& button,const elysia::core::Rect& rect,bool available,Region region)
    {
        button.set_screen_rect(rect);
        button.set_base_style(style().arrow);
        button.set_enabled(is_enabled() && available);
        button.set_active(is_active());
        button.set_visible(is_visible());
        button.set_opacity(opacity());
        button.set_focused(is_focused() && _hovered_region == region);
    };
    sync_button(_previous,layout.previous,adjacent_index(-1).has_value(),Region::Previous);
    sync_button(_next,layout.next,adjacent_index(1).has_value(),Region::Next);
    _label.set_screen_rect(layout.center);
    _label.set_text_content(_selected_index ? _options[*_selected_index].content : ui_raw_text("--"));
    auto label_style = UiStyleDefaults::label();
    label_style.draw_background = false;
    label_style.text = resolve_enabled_disabled_color(style().text,is_enabled());
    _label.set_base_style(label_style);
    _label.set_horizontal_align(TextHorizontalAlign::Center);
    _label.set_vertical_align(TextVerticalAlign::Center);
    _label.set_text_fit_mode(UiLabelTextFitMode::ShrinkToFit);
    _label.set_visible(is_visible());
    _label.set_active(is_active());
    _label.set_opacity(opacity());
}

void UiOptionSelector::draw_arrow(std::vector<elysia::core::UiRenderCommand>& out,
    const elysia::core::Rect& rect,bool previous,bool enabled) const
{
    if (rect.is_empty()) return;
    const auto center = rect.center();
    const float radius = std::min(rect.width(),rect.height()) * 0.2f;
    const float sign = previous ? -1.0f : 1.0f;
    const auto color = apply_opacity(resolve_enabled_disabled_color(style().arrow.text,enabled));
    elysia::core::Vector2 start,tip,end;
    if (_orientation == UiOptionSelectorOrientation::Horizontal)
    {
        start = {center.x - sign * radius * 0.5f,center.y - radius};
        tip = {center.x + sign * radius * 0.5f,center.y};
        end = {start.x,center.y + radius};
    }
    else
    {
        start = {center.x - radius,center.y - sign * radius * 0.5f};
        tip = {center.x,center.y + sign * radius * 0.5f};
        end = {center.x + radius,start.y};
    }
    out.push_back(elysia::core::make_ui_draw_line_command(start,tip,color));
    out.push_back(elysia::core::make_ui_draw_line_command(tip,end,color));
}

void UiOptionSelector::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out) const
{
    if (!is_visible() || screen_rect().is_empty()) return;
    const auto& chrome = style().chrome;
    if (chrome.draw_background)
        out.push_back(elysia::core::make_ui_fill_rect_command(screen_rect(),apply_opacity(
            resolve_interactive_color(chrome.background,is_enabled(),is_focused(),_is_adjusting)),chrome.corner_radius));
    if (chrome.draw_border)
        out.push_back(elysia::core::make_ui_draw_rect_command(screen_rect(),apply_opacity(
            resolve_interactive_color(chrome.border,is_enabled(),is_focused(),_is_adjusting)),chrome.corner_radius,chrome.border_width));
    sync_children();
    _previous.submit_ui_render_commands(out);
    _next.submit_ui_render_commands(out);
    const auto layout = compute_layout();
    draw_arrow(out,layout.previous,true,_previous.is_enabled());
    draw_arrow(out,layout.next,false,_next.is_enabled());
    _label.submit_ui_render_commands(out);
}

void UiOptionSelector::set_base_style(const UiOptionSelectorStyle& value) noexcept
{
    _style_state.set_base_style(value);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}
void UiOptionSelector::set_style_overrides(const UiOptionSelectorStyleOverrides& overrides) noexcept
{
    _style_state.set_style_overrides(overrides);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}
const UiOptionSelectorStyle& UiOptionSelector::style() const noexcept { return _style_state.effective_style(); }
const UiOptionSelectorStyleOverrides& UiOptionSelector::style_overrides() const noexcept { return _style_state.style_overrides(); }
bool UiOptionSelector::has_style_overrides() const noexcept { return _style_state.has_style_overrides(); }
void UiOptionSelector::clear_style_overrides() noexcept
{
    _style_state.clear_style_overrides();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}
}
