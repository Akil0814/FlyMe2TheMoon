#include "ui_slider.h"

#include "../style/ui_style_defaults.h"
#include "../../audio/audio_service.h"
#include "../../core/render/render_command.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace elysia::ui
{
using elysia::typography::UiTypographyRole;

namespace
{
constexpr float kValueChangeEpsilon = 0.0001f;
constexpr float kTrackEdgePadding = 6.0f;

[[nodiscard]] bool nearly_equal(float a,float b) noexcept
{
    return std::fabs(a - b) <= kValueChangeEpsilon;
}

[[nodiscard]] float clamp_non_negative(float value) noexcept
{
    return std::max(0.0f,value);
}

[[nodiscard]] elysia::core::Rect take_right(elysia::core::Rect& rect,float width) noexcept
{
    width = std::clamp(width,0.0f,std::max(0.0f,rect.width()));
    const elysia::core::Rect slot(rect.right() - width,rect.y(),width,rect.height());
    rect.set_width(std::max(0.0f,rect.width() - width));
    return slot;
}

[[nodiscard]] elysia::core::Rect take_top(elysia::core::Rect& rect,float height) noexcept
{
    height = std::clamp(height,0.0f,std::max(0.0f,rect.height()));
    const elysia::core::Rect slot(rect.x(),rect.y(),rect.width(),height);
    rect.set_y(rect.y() + height);
    rect.set_height(std::max(0.0f,rect.height() - height));
    return slot;
}

[[nodiscard]] float preferred_side_slot_extent(const elysia::core::Rect& rect,const elysia::core::Vector2& handle_size) noexcept
{
    return std::min(std::max(56.0f,handle_size.x * 2.0f),std::max(0.0f,rect.width() * 0.35f));
}

[[nodiscard]] float preferred_vertical_slot_extent(
    const elysia::core::Rect& rect,
    const elysia::core::Vector2& handle_size,
    float target_height
) noexcept
{
    return std::min(std::max({ 24.0f,handle_size.y,target_height }) + 8.0f,std::max(0.0f,rect.height() * 0.35f));
}

[[nodiscard]] elysia::core::Rect inset_track_area_for_handle(
    const elysia::core::Rect& rect,
    const elysia::core::Vector2& handle_size,
    UiSliderOrientation orientation
) noexcept
{
    if (rect.is_empty())
        return rect;

    elysia::core::Rect inset = rect;
    if (orientation == UiSliderOrientation::Horizontal)
    {
        const float horizontal_inset = std::min(
            std::max(6.0f + kTrackEdgePadding,handle_size.x * 0.5f + kTrackEdgePadding),
            std::max(0.0f,rect.width() * 0.5f));
        inset.set_x(rect.x() + horizontal_inset);
        inset.set_width(std::max(0.0f,rect.width() - horizontal_inset * 2.0f));
    }
    else
    {
        const float vertical_inset = std::min(
            std::max(6.0f + kTrackEdgePadding,handle_size.y * 0.5f + kTrackEdgePadding),
            std::max(0.0f,rect.height() * 0.5f));
        inset.set_y(rect.y() + vertical_inset);
        inset.set_height(std::max(0.0f,rect.height() - vertical_inset * 2.0f));
    }

    return inset;
}
}

struct UiSlider::SliderLayout
{
    elysia::core::Rect track_area = elysia::core::Rect::zero();
    elysia::core::Rect bar_rect = elysia::core::Rect::zero();
    elysia::core::Rect handle_rect = elysia::core::Rect::zero();
    elysia::core::Rect value_rect = elysia::core::Rect::zero();
};

UiSlider::UiSlider(const elysia::core::Rect& rect,int order) noexcept
    : UiControl(rect,order)
{
    reset();
}

UiSlider::UiSlider(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiControl(position,size,order)
{
    reset();
}

UiSlider::UiSlider(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiControl(center,size,from_center,order)
{
    reset();
}

UiSlider::UiSlider(const elysia::core::Rect& rect,const UiSliderConfig& config,int order) noexcept
    : UiSlider(rect,order)
{
    set_slider_config(config);
}

UiSlider::UiSlider(const elysia::core::Vector2& position,const elysia::core::Vector2& size,const UiSliderConfig& config,int order) noexcept
    : UiSlider(elysia::core::Rect(position.x,position.y,size.x,size.y),config,order) {}

UiSlider::UiSlider(
    const elysia::core::Vector2& center,const elysia::core::Vector2& size,
    UiFromCenterTag,const UiSliderConfig& config,int order
) noexcept : UiSlider(elysia::core::Rect::from_center(center,size),config,order) {}

void UiSlider::reset() noexcept
{
    UiControl::reset();
    _bar.reset();
    _handle.reset();
    _value_number.reset();

    _sounds.reset();
    _style_state.reset(UiStyleDefaults::slider());
    _on_value_changed = nullptr;
    _orientation = UiSliderOrientation::Horizontal;
    _value_display = UiSliderValueDisplay::None;
    _is_adjusting = false;
    _drag_value_changed = false;
    _bar_thickness = 6.0f;
    _min_value = 0.0f;
    _max_value = 1.0f;
    _value = 0.0f;
    _step = std::nullopt;
    _last_slide_sound_ticks = 0;
    _has_last_slide_sound_tick = false;

    initialize_child_widgets();
}

void UiSlider::set_enabled(bool enabled)
{
    UiControl::set_enabled(enabled);
    _handle.set_enabled(enabled);
    if (!enabled)
        clear_drag_state();
}

void UiSlider::set_focused(bool focused)
{
    const bool was_focused = is_focused();
    UiControl::set_focused(focused);
    _handle.set_focused(is_focused());
    if (!is_focused())
        clear_drag_state();
    if (!was_focused && is_focused() && _sounds)
        play_sound_if_set(_sounds->on_focus);
}
bool UiSlider::on_ui_input_event(const UiInputEvent& event)
{
    if (event.type == UiInputEventType::MouseMoved)
    {
        if (!can_receive_pointer())
        {
            set_focused(false);
            _handle.set_focused(false);
            clear_drag_state();
            return false;
        }

        if (!_handle.is_dragging())
            sync_child_rects(compute_layout());
        sync_child_visuals();
        const bool handle_handled = _handle.on_ui_input_event(event);
        set_focused(_handle.is_dragging() || contains_pointer(event.mouse_x,event.mouse_y));
        _handle.set_focused(is_focused());
        if (!_handle.is_dragging())
            sync_child_rects(compute_layout());
        return handle_handled;
    }

    if (event.type == UiInputEventType::PointerPressed)
    {
        if (!is_primary_pointer_event(event) || !can_receive_pointer())
            return false;

        const SliderLayout layout = compute_layout();
        if (!contains_track_or_handle(layout,event.mouse_x,event.mouse_y))
            return false;

        const elysia::core::Vector2 pointer = presentation_to_layout_point(
            elysia::core::Vector2(static_cast<float>(event.mouse_x),static_cast<float>(event.mouse_y)));
        sync_child_rects(layout);
        sync_child_visuals();
        set_focused(true);
        _handle.set_focused(true);
        _is_adjusting = false;
        _drag_value_changed = false;
        _has_last_slide_sound_tick = false;
        if (!layout.handle_rect.contains(pointer))
        {
            _drag_value_changed = update_value_from_point(layout,pointer,true);
            sync_child_rects(compute_layout());
        }
        _handle.begin_drag_from_pointer(pointer);
        return true;
    }

    if (event.type == UiInputEventType::PointerReleased)
    {
        if (!is_primary_pointer_event(event))
            return false;

        const bool handle_handled = _handle.on_ui_input_event(event);
        _is_adjusting = false;
        sync_child_rects(compute_layout());
        sync_child_visuals();
        const bool is_inside = can_receive_pointer() && contains_pointer(event.mouse_x,event.mouse_y);
        set_focused(is_inside);
        _handle.set_focused(is_focused());
        return handle_handled;
    }

    if (!can_interact())
    {
        clear_drag_state();
        return false;
    }

    if (event.action == UiAction::Confirm)
    {
        if (event.type == UiInputEventType::ActionPressed)
        {
            _is_adjusting = !_is_adjusting;
            return true;
        }
        return event.type == UiInputEventType::ActionReleased;
    }

    if (event.type != UiInputEventType::ActionPressed || !_is_adjusting)
        return false;

    const float delta = action_step();
    float target_value = _value;
    bool handled = false;
    switch (event.action)
    {
    case UiAction::Home:
        target_value = _min_value;
        handled = true;
        break;
    case UiAction::End:
        target_value = _max_value;
        handled = true;
        break;
    case UiAction::NavigateLeft:
        if (_orientation == UiSliderOrientation::Horizontal)
        {
            target_value = _value - delta;
            handled = true;
        }
        break;
    case UiAction::NavigateRight:
        if (_orientation == UiSliderOrientation::Horizontal)
        {
            target_value = _value + delta;
            handled = true;
        }
        break;
    case UiAction::NavigateUp:
        if (_orientation == UiSliderOrientation::Vertical)
        {
            target_value = _value + delta;
            handled = true;
        }
        break;
    case UiAction::NavigateDown:
        if (_orientation == UiSliderOrientation::Vertical)
        {
            target_value = _value - delta;
            handled = true;
        }
        break;
    default:
        break;
    }

    if (!handled)
        return false;
    if (set_value_internal(target_value,true))
        play_slide_sound_if_allowed();
    return true;
}

void UiSlider::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;

    const elysia::core::Rect& slider_rect = screen_rect();
    if (slider_rect.is_empty())
        return;

    const SliderLayout layout = compute_layout();
    const UiSliderStyle& style = _style_state.effective_style();
    if (style.chrome.draw_background)
        out_commands.push_back(elysia::core::make_ui_fill_rect_command(slider_rect,apply_opacity(current_background_color()),style.chrome.corner_radius));

    sync_child_rects(layout);
    sync_child_visuals();
    sync_value_number_content();
    _bar.submit_ui_render_commands(out_commands);
    _handle.submit_ui_render_commands(out_commands);

    if (_value_display != UiSliderValueDisplay::None)
        _value_number.submit_ui_render_commands(out_commands);
    if (style.chrome.draw_border)
        out_commands.push_back(elysia::core::make_ui_draw_rect_command(
            slider_rect,
            apply_opacity(current_border_color()),
            style.chrome.corner_radius,
            style.chrome.border_width));
}

void UiSlider::set_slider_config(const UiSliderConfig& config)
{
    apply_slider_config(config);
}

void UiSlider::set_range(float min_value,float max_value)
{
    if (!std::isfinite(min_value))
        min_value = 0.0f;
    if (!std::isfinite(max_value))
        max_value = min_value;
    if (max_value < min_value)
        std::swap(min_value,max_value);

    _min_value = min_value;
    _max_value = max_value;
    (void)set_value_internal(_value,false);
}

void UiSlider::set_value(float value)
{
    (void)set_value_internal(value,true);
}

void UiSlider::set_ratio(float ratio)
{
    if (_max_value <= _min_value)
    {
        set_value(_min_value);
        return;
    }

    const float clamped = std::clamp(ratio,0.0f,1.0f);
    set_value(_min_value + (_max_value - _min_value) * clamped);
}

float UiSlider::min_value() const noexcept
{
    return _min_value;
}

float UiSlider::max_value() const noexcept
{
    return _max_value;
}

float UiSlider::value() const noexcept
{
    return _value;
}

float UiSlider::ratio() const noexcept
{
    return clamped_ratio();
}
void UiSlider::set_step(std::optional<float> step)
{
    if (step && (!std::isfinite(*step) || *step <= 0.0f))
        _step = std::nullopt;
    else
        _step = step;
    (void)set_value_internal(_value,false);
}

const std::optional<float>& UiSlider::step() const noexcept
{
    return _step;
}

void UiSlider::set_orientation(UiSliderOrientation orientation) noexcept
{
    _orientation = orientation;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

UiSliderOrientation UiSlider::orientation() const noexcept
{
    return _orientation;
}

void UiSlider::set_value_display(UiSliderValueDisplay display) noexcept
{
    _value_display = display;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

UiSliderValueDisplay UiSlider::value_display() const noexcept
{
    return _value_display;
}

bool UiSlider::is_adjusting() const noexcept
{
    return _is_adjusting;
}

void UiSlider::set_sounds(const UiSliderSounds& sounds)
{
    _sounds = sounds;
}

void UiSlider::clear_sounds() noexcept
{
    _sounds.reset();
}

const std::optional<UiSliderSounds>& UiSlider::sounds() const noexcept
{
    return _sounds;
}

void UiSlider::set_on_value_changed(UiSliderValueChangedCallback on_value_changed)
{
    _on_value_changed = std::move(on_value_changed);
}

void UiSlider::set_base_style(const UiSliderStyle& style) noexcept
{
    _style_state.set_base_style(style);
    _handle.set_base_style(this->style().handle);
}

void UiSlider::set_style_overrides(const UiSliderStyleOverrides& overrides)
{
    _style_state.set_style_overrides(overrides);
    _handle.set_base_style(style().handle);
    _handle.set_style_overrides(overrides.handle);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

const UiSliderStyle& UiSlider::style() const noexcept
{
    return _style_state.effective_style();
}

const UiSliderStyleOverrides& UiSlider::style_overrides() const noexcept { return _style_state.style_overrides(); }
bool UiSlider::has_style_overrides() const noexcept
{
    return _style_state.has_style_overrides();
}

void UiSlider::clear_style_overrides() noexcept
{
    _style_state.clear_style_overrides();
    _handle.clear_style_overrides();
    _handle.set_base_style(style().handle);
    sync_child_visuals();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiSlider::set_value_decimal_places(int decimal_places)
{
    _value_number.set_decimal_places(decimal_places);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

int UiSlider::value_decimal_places() const noexcept
{
    return _value_number.decimal_places();
}
void UiSlider::set_value_trim_trailing_zeros(bool trim_trailing_zeros)
{
    _value_number.set_trim_trailing_zeros(trim_trailing_zeros);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

bool UiSlider::value_trims_trailing_zeros() const noexcept
{
    return _value_number.trims_trailing_zeros();
}

void UiSlider::set_value_keep_decimal_point(bool keep_decimal_point)
{
    _value_number.set_keep_decimal_point(keep_decimal_point);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

bool UiSlider::value_keeps_decimal_point() const noexcept
{
    return _value_number.keeps_decimal_point();
}

void UiSlider::set_value_digit_spacing(float spacing)
{
    _value_number.set_digit_spacing(spacing);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

float UiSlider::value_digit_spacing() const noexcept
{
    return _value_number.digit_spacing();
}

void UiSlider::set_value_fixed_glyph_advance(float advance)
{
    _value_number.set_fixed_glyph_advance(advance);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

std::optional<float> UiSlider::value_fixed_glyph_advance() const noexcept
{
    return _value_number.fixed_glyph_advance();
}

void UiSlider::clear_value_fixed_glyph_advance()
{
    _value_number.clear_fixed_glyph_advance();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiSlider::set_value_target_height(float height)
{
    _value_number.set_target_height(height);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

std::optional<float> UiSlider::value_target_height() const noexcept
{
    return _value_number.target_height();
}

void UiSlider::clear_value_target_height()
{
    _value_number.clear_target_height();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiSlider::set_bar_thickness(float thickness) noexcept
{
    _bar_thickness = clamp_non_negative(thickness);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

float UiSlider::bar_thickness() const noexcept
{
    return _bar_thickness;
}

void UiSlider::apply_slider_config(const UiSliderConfig& config)
{
    if (config.slider_sound)
        set_sounds(*config.slider_sound);
    else
        clear_sounds();

    set_orientation(config.orientation);
    set_value_display(config.value_display);
    if (config.style_overrides)
        set_style_overrides(*config.style_overrides);
    else
        clear_style_overrides();
    set_bar_thickness(config.bar_thickness);
    set_range(config.min_value,config.max_value);
    set_step(config.step);
    set_value(config.value);
    set_value_decimal_places(config.value_decimal_places);
    set_value_trim_trailing_zeros(config.value_trim_trailing_zeros);
    set_value_keep_decimal_point(config.value_keep_decimal_point);
    set_value_digit_spacing(config.value_digit_spacing);
    if (config.value_fixed_glyph_advance)
        set_value_fixed_glyph_advance(*config.value_fixed_glyph_advance);
    else
        clear_value_fixed_glyph_advance();
    if (config.value_target_height)
        set_value_target_height(*config.value_target_height);
    else
        clear_value_target_height();
}

void UiSlider::initialize_child_widgets()
{
    // Slider sub-widgets are implementation details. The outer slider owns theme participation
    // and pushes resolved visuals into the bar/handle/value number manually.
    _bar.set_padding(0);
    _bar.set_fill_direction(BarFillDirection::LeftToRight);
    UiNumberStyleOverrides number_overrides{};
    number_overrides.draw_background = false;
    _value_number.set_style_overrides(number_overrides);
    _value_number.set_horizontal_align(TextHorizontalAlign::Center);
    _value_number.set_vertical_align(TextVerticalAlign::Center);
    _value_number.set_typography_role(UiTypographyRole::SliderValue);
    _value_number.set_target_height(24.0f);
    _value_number.set_padding(0);
    _value_number.set_decimal_places(0);
    _value_number.set_trim_trailing_zeros(true);
    _value_number.set_keep_decimal_point(false);
    _value_number.set_suffix(UiNumberSuffix::None);
    _handle.set_base_style(style().handle);
    sync_child_visuals();
    bind_handle_callbacks();
}

void UiSlider::sync_child_rects(const SliderLayout& layout) const
{
    _bar.set_screen_rect(layout.bar_rect);
    _handle.set_screen_rect(layout.handle_rect);
    _handle.set_drag_axis(_orientation == UiSliderOrientation::Horizontal ? UiDragAxis::Horizontal : UiDragAxis::Vertical);
    if (layout.handle_rect.is_empty() || layout.bar_rect.is_empty())
        _handle.clear_drag_bounds();
    else
        _handle.set_drag_bounds(handle_drag_bounds(layout));
    _value_number.set_screen_rect(layout.value_rect);
}

void UiSlider::sync_child_visuals() const
{
    // Rebuild child visuals from the slider's resolved effective style so manual slider
    // overrides and theme-driven updates both flow through one synchronization path.
    UiBarStyle bar_style = UiStyleDefaults::bar();
    bar_style.background = current_background_color();
    bar_style.fill = current_fill_color();
    bar_style.draw_border = false;

    UiNumberStyle number_style = UiStyleDefaults::number();
    number_style.text = current_text_color();
    number_style.draw_background = false;

    _bar.set_visible(!_bar.screen_rect().is_empty());
    _bar.set_opacity(opacity());
    _bar.set_range(_min_value,_max_value);
    _bar.set_value(_value);
    _bar.set_fill_direction(_orientation == UiSliderOrientation::Horizontal ? BarFillDirection::LeftToRight : BarFillDirection::BottomToTop);
    _bar.set_base_style(bar_style);
    _bar.set_padding(0);

    _handle.set_visible(!_handle.screen_rect().is_empty());
    _handle.set_enabled(is_enabled());
    _handle.set_opacity(opacity());
    _handle.set_focused(is_focused());
    UiDragHandleStyle handle_style = style().handle;
    if (_is_adjusting)
    {
        handle_style.chrome.background.focused = handle_style.chrome.background.active;
        handle_style.chrome.border.focused = handle_style.chrome.border.active;
    }
    _handle.set_base_style(handle_style);

    _value_number.set_visible(_value_display != UiSliderValueDisplay::None && !_value_number.screen_rect().is_empty());
    _value_number.set_opacity(opacity());
    _value_number.set_base_style(number_style);
    _value_number.set_horizontal_align(TextHorizontalAlign::Center);
    _value_number.set_vertical_align(TextVerticalAlign::Center);
    _value_number.set_padding(0);
}

void UiSlider::sync_value_number_content() const
{
    _value_number.set_suffix(_value_display == UiSliderValueDisplay::Percent ? UiNumberSuffix::Percent : UiNumberSuffix::None);
    _value_number.set_value(_value_display == UiSliderValueDisplay::Percent ? static_cast<double>(clamped_ratio() * 100.0f) : static_cast<double>(_value));
}

UiSlider::SliderLayout UiSlider::compute_layout() const noexcept
{
    SliderLayout layout;
    elysia::core::Rect remaining = screen_rect();
    if (remaining.is_empty())
        return layout;

    const elysia::core::Vector2 handle_size(
        clamp_non_negative(style().handle.size.x),
        clamp_non_negative(style().handle.size.y)
    );
    const float side_slot_extent = preferred_side_slot_extent(remaining,handle_size);
    const float target_height = _value_number.target_height().value_or(24.0f);
    const float vertical_slot_extent = preferred_vertical_slot_extent(remaining,handle_size,target_height);

    if (_value_display != UiSliderValueDisplay::None)
    {
        if (_orientation == UiSliderOrientation::Horizontal)
            layout.value_rect = take_right(remaining,side_slot_extent);
        else
            layout.value_rect = take_top(remaining,vertical_slot_extent);
    }

    layout.track_area = inset_track_area_for_handle(remaining,handle_size,_orientation);
    if (layout.track_area.is_empty())
        return layout;

    if (_orientation == UiSliderOrientation::Horizontal)
    {
        const float thickness = std::min(clamp_non_negative(_bar_thickness),std::max(0.0f,layout.track_area.height()));
        layout.bar_rect = elysia::core::Rect(
            layout.track_area.x(),
            layout.track_area.center().y - thickness * 0.5f,
            layout.track_area.width(),
            thickness
        );
    }
    else
    {
        const float thickness = std::min(clamp_non_negative(_bar_thickness),std::max(0.0f,layout.track_area.width()));
        layout.bar_rect = elysia::core::Rect(
            layout.track_area.center().x - thickness * 0.5f,
            layout.track_area.y(),
            thickness,
            layout.track_area.height()
        );
    }

    if (layout.bar_rect.is_empty())
        return layout;

    const elysia::core::Vector2 clamped_handle_size(
        std::min(handle_size.x,std::max(0.0f,layout.track_area.width())),
        std::min(handle_size.y,std::max(0.0f,layout.track_area.height()))
    );
    if (clamped_handle_size.x <= 0.0f || clamped_handle_size.y <= 0.0f)
        return layout;

    const float ratio = clamped_ratio();
    elysia::core::Vector2 handle_center = layout.bar_rect.center();
    if (_orientation == UiSliderOrientation::Horizontal)
        handle_center.x = layout.bar_rect.x() + layout.bar_rect.width() * ratio;
    else
        handle_center.y = layout.bar_rect.y() + layout.bar_rect.height() * (1.0f - ratio);
    layout.handle_rect = elysia::core::Rect::from_center(handle_center,clamped_handle_size);
    return layout;
}

float UiSlider::clamped_ratio() const noexcept
{
    const float range = _max_value - _min_value;
    if (range <= 0.0f)
        return 0.0f;
    return std::clamp((_value - _min_value) / range,0.0f,1.0f);
}

float UiSlider::snapped_value(float value) const noexcept
{
    const float min_value = std::min(_min_value,_max_value);
    const float max_value = std::max(_min_value,_max_value);
    float clamped = std::clamp(value,min_value,max_value);
    if (!_step || *_step <= 0.0f)
        return clamped;
    if (max_value <= min_value)
        return min_value;

    const float steps = std::round((clamped - min_value) / *_step);
    clamped = min_value + steps * *_step;
    return std::clamp(clamped,min_value,max_value);
}

float UiSlider::action_step() const noexcept
{
    if (_step && *_step > 0.0f)
        return *_step;
    const float range = _max_value - _min_value;
    return range > 0.0f ? range * 0.05f : 0.0f;
}
float UiSlider::ratio_from_point(const SliderLayout& layout,const elysia::core::Vector2& point) const noexcept
{
    if (layout.bar_rect.is_empty())
        return 0.0f;
    if (_orientation == UiSliderOrientation::Horizontal)
    {
        if (layout.bar_rect.width() <= 0.0f)
            return 0.0f;
        return std::clamp((point.x - layout.bar_rect.x()) / layout.bar_rect.width(),0.0f,1.0f);
    }

    if (layout.bar_rect.height() <= 0.0f)
        return 0.0f;
    return std::clamp(1.0f - (point.y - layout.bar_rect.y()) / layout.bar_rect.height(),0.0f,1.0f);
}

elysia::core::Rect UiSlider::handle_drag_bounds(const SliderLayout& layout) const noexcept
{
    if (_orientation == UiSliderOrientation::Horizontal)
    {
        return elysia::core::Rect(
            layout.bar_rect.x() - layout.handle_rect.width() * 0.5f,
            layout.handle_rect.y(),
            layout.bar_rect.width() + layout.handle_rect.width(),
            layout.handle_rect.height()
        );
    }

    return elysia::core::Rect(
        layout.handle_rect.x(),
        layout.bar_rect.y() - layout.handle_rect.height() * 0.5f,
        layout.handle_rect.width(),
        layout.bar_rect.height() + layout.handle_rect.height()
    );
}

bool UiSlider::can_interact() const noexcept
{
    return is_enabled() && is_focused() && is_active() && is_visible();
}

bool UiSlider::can_receive_pointer() const noexcept
{
    return is_enabled() && is_active() && is_visible();
}

bool UiSlider::contains_pointer(int mouse_x,int mouse_y) const noexcept
{
    return presentation_screen_rect().contains(elysia::core::Vector2(static_cast<float>(mouse_x),static_cast<float>(mouse_y)));
}

bool UiSlider::contains_track_or_handle(const SliderLayout& layout,int mouse_x,int mouse_y) const noexcept
{
    const elysia::core::Vector2 point = presentation_to_layout_point(
        elysia::core::Vector2(static_cast<float>(mouse_x),static_cast<float>(mouse_y)));
    return layout.bar_rect.contains(point) || layout.handle_rect.contains(point);
}

bool UiSlider::is_primary_pointer_event(const UiInputEvent& event) const noexcept
{
    return event.device == elysia::input::InputDevice::Mouse && event.control == elysia::input::RawInputControl::MouseLeft;
}

bool UiSlider::set_value_internal(float value,bool notify)
{
    float next_value = std::isfinite(value) ? value : _min_value;
    next_value = snapped_value(next_value);
    if (nearly_equal(next_value,_value))
        return false;

    _value = next_value;
    const UiSliderValueChangedCallback callback = notify ? _on_value_changed : nullptr;
    const float callback_value = _value;
    if (callback)
        callback(callback_value);
    return true;
}

bool UiSlider::update_value_from_point(const SliderLayout& layout,const elysia::core::Vector2& point,bool notify)
{
    if (_max_value <= _min_value)
        return set_value_internal(_min_value,notify);
    return set_value_internal(_min_value + (_max_value - _min_value) * ratio_from_point(layout,point),notify);
}

elysia::core::Color UiSlider::current_background_color() const noexcept
{
    return resolve_enabled_disabled_color(
        UiEnabledDisabledColors{
            style().chrome.background.idle,
            style().chrome.background.disabled
        },
        is_enabled());
}

elysia::core::Color UiSlider::current_border_color() const noexcept
{
    return resolve_interactive_color(
        style().chrome.border,is_enabled(),is_focused(),_is_adjusting || _handle.is_dragging());
}

elysia::core::Color UiSlider::current_fill_color() const noexcept
{
    return resolve_enabled_disabled_color(style().fill,is_enabled());
}

elysia::core::Color UiSlider::current_text_color() const noexcept
{
    return resolve_enabled_disabled_color(style().text,is_enabled());
}
void UiSlider::bind_handle_callbacks()
{
    _handle.set_on_dragged([this](const elysia::core::Vector2& center)
    {
        const SliderLayout layout = compute_layout();
        const bool changed = update_value_from_point(layout,center,true);
        _drag_value_changed = changed || _drag_value_changed;
        if (changed)
            play_slide_sound_if_allowed();
    });
    _handle.set_on_drag_ended([this](const elysia::core::Vector2&)
    {
        const bool should_settle = _drag_value_changed;
        _drag_value_changed = false;
        if (should_settle && _sounds)
            play_sound_if_set(_sounds->on_settle);
    });
}

void UiSlider::clear_drag_state() noexcept
{
    _handle.cancel_drag();
    _is_adjusting = false;
    _drag_value_changed = false;
}

void UiSlider::play_sound_if_set(std::string_view sound_key) const
{
    if (sound_key.empty())
        return;
    elysia::audio::AudioService::instance()->request_sound(sound_key,{
        .group = elysia::audio::SoundGroup::Ui
    });
}

void UiSlider::play_slide_sound_if_allowed()
{
    if (!_sounds)
        return;
    if (_sounds->on_slide.empty())
        return;

    const std::uint32_t now = static_cast<std::uint32_t>(SDL_GetTicks());
    const std::uint32_t min_interval_ms = static_cast<std::uint32_t>(std::max(0.0,_sounds->min_slide_sound_interval) * 1000.0);
    if (!_has_last_slide_sound_tick || now - _last_slide_sound_ticks >= min_interval_ms)
    {
        play_sound_if_set(_sounds->on_slide);
        _last_slide_sound_ticks = now;
        _has_last_slide_sound_tick = true;
    }
}

}


