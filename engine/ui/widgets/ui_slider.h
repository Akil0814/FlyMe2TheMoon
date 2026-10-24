#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include "../core/ui_control.h"
#include "../style/ui_style.h"
#include "../style/ui_interaction_style.h"
#include "number/ui_number.h"
#include "ui_bar.h"
#include "ui_drag_handle.h"

namespace elysia::ui
{
    enum class UiSliderOrientation { Horizontal, Vertical };
    enum class UiSliderValueDisplay { None, Value, Percent };

    // Sound keys played as slider focus, movement, and settle state change.
    struct UiSliderSounds
    {
        std::string on_focus{};
        std::string on_slide{};
        std::string on_settle{};
        double min_slide_sound_interval = 0.05;
    };

    // Visual styling for slider chrome, fill/value text, and drag handle.
    struct UiSliderStyle
    {
        UiChromeStyle chrome{};
        UiEnabledDisabledColors fill{};
        UiEnabledDisabledColors text{};
        UiDragHandleStyle handle{};
    };
    struct UiSliderStyleOverrides
    {
        UiChromeStyleOverrides chrome{};
        UiEnabledDisabledColorsOverrides fill{};
        UiEnabledDisabledColorsOverrides text{};
        UiDragHandleStyleOverrides handle{};
    };
    template<> struct UiStyleOverrideTraits<UiSliderStyle> { using Overrides=UiSliderStyleOverrides; static bool empty(const Overrides& o) noexcept { return elysia::ui::empty(o.chrome)&&elysia::ui::empty(o.fill)&&elysia::ui::empty(o.text)&&UiStyleOverrideTraits<UiDragHandleStyle>::empty(o.handle); } static void apply(UiSliderStyle& s,const Overrides& o) noexcept { apply_ui_style_overrides(s.chrome,o.chrome); apply_ui_style_overrides(s.fill,o.fill); apply_ui_style_overrides(s.text,o.text); UiStyleOverrideTraits<UiDragHandleStyle>::apply(s.handle,o.handle); } };

    // Bundles slider range, presentation, sounds, and numeric formatting rules.
    struct UiSliderConfig
    {
        UiSliderOrientation orientation = UiSliderOrientation::Horizontal;
        UiSliderValueDisplay value_display = UiSliderValueDisplay::None;
        std::optional<UiSliderSounds> slider_sound;
        float min_value = 0.0f;
        float max_value = 1.0f;
        float value = 0.0f;
        std::optional<float> step = std::nullopt;
        std::optional<UiSliderStyleOverrides> style_overrides = std::nullopt;
        float bar_thickness = 6.0f;
        int value_decimal_places = 0;
        bool value_trim_trailing_zeros = true;
        bool value_keep_decimal_point = false;
        float value_digit_spacing = 0.0f;
        std::optional<float> value_fixed_glyph_advance = std::nullopt;
        std::optional<float> value_target_height = std::nullopt;
    };

    using UiSliderValueChangedCallback = std::function<void(float value)>;

    class UiSlider : public UiControl
    {
    public:
        explicit UiSlider(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
        UiSlider(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
        UiSlider(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;
        UiSlider(const elysia::core::Rect& rect,const UiSliderConfig& config,int order = 0) noexcept;
        UiSlider(const elysia::core::Vector2& position,const elysia::core::Vector2& size,const UiSliderConfig& config,int order = 0) noexcept;
        UiSlider(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,const UiSliderConfig& config,int order = 0) noexcept;
        ~UiSlider() override = default;

        void cancel_input_interaction() noexcept override
        {
            clear_drag_state();
        }
        void reset() noexcept override;
        void set_enabled(bool enabled) override;
        void set_focused(bool focused) override;

        bool on_ui_input_event(const UiInputEvent& event) override;
        void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

        // Applies range, presentation, sounds, and formatting as one slider update.
        void set_slider_config(const UiSliderConfig& config);
        // Defines the numeric range used to convert between value and slider ratio.
        void set_range(float min_value,float max_value);
        void set_value(float value);
        // Sets the slider by normalized ratio instead of by numeric value.
        void set_ratio(float ratio);
        [[nodiscard]] float min_value() const noexcept;
        [[nodiscard]] float max_value() const noexcept;
        [[nodiscard]] float value() const noexcept;
        [[nodiscard]] float ratio() const noexcept;
        void set_step(std::optional<float> step);
        [[nodiscard]] const std::optional<float>& step() const noexcept;

        void set_orientation(UiSliderOrientation orientation) noexcept;
        [[nodiscard]] UiSliderOrientation orientation() const noexcept;
        void set_value_display(UiSliderValueDisplay display) noexcept;
        [[nodiscard]] UiSliderValueDisplay value_display() const noexcept;
        // Reports whether keyboard/gamepad input currently owns the slider's primary axis.
        [[nodiscard]] bool is_adjusting() const noexcept;

        void set_sounds(const UiSliderSounds& sounds);
        void clear_sounds() noexcept;
        [[nodiscard]] const std::optional<UiSliderSounds>& sounds() const noexcept;
        void set_on_value_changed(UiSliderValueChangedCallback on_value_changed);

        void set_base_style(const UiSliderStyle& style) noexcept;
    void set_style_overrides(const UiSliderStyleOverrides& overrides);
        [[nodiscard]] const UiSliderStyle& style() const noexcept;
        [[nodiscard]] const UiSliderStyleOverrides& style_overrides() const noexcept;
        [[nodiscard]] bool has_style_overrides() const noexcept;
        void clear_style_overrides() noexcept;

        void set_value_decimal_places(int decimal_places);
        [[nodiscard]] int value_decimal_places() const noexcept;
        void set_value_trim_trailing_zeros(bool trim_trailing_zeros);
        [[nodiscard]] bool value_trims_trailing_zeros() const noexcept;
        void set_value_keep_decimal_point(bool keep_decimal_point);
        [[nodiscard]] bool value_keeps_decimal_point() const noexcept;
        void set_value_digit_spacing(float spacing);
        [[nodiscard]] float value_digit_spacing() const noexcept;
        void set_value_fixed_glyph_advance(float advance);
        [[nodiscard]] std::optional<float> value_fixed_glyph_advance() const noexcept;
        void clear_value_fixed_glyph_advance();
        void set_value_target_height(float height);
        [[nodiscard]] std::optional<float> value_target_height() const noexcept;
        void clear_value_target_height();

        void set_bar_thickness(float thickness) noexcept;
        [[nodiscard]] float bar_thickness() const noexcept;

    private:
        struct SliderLayout;
        // Creates the owned child widgets used to render the bar, thumb, and optional value.
        void initialize_child_widgets();
        // Applies the config payload without exposing intermediate slider state.
        void apply_slider_config(const UiSliderConfig& config);
        // Pushes the computed layout into the owned child widgets.
        void sync_child_rects(const SliderLayout& layout) const;
        // Mirrors enabled, focus, and style state into the owned child widgets.
        void sync_child_visuals() const;
        // Recomputes the optional formatted numeric display from the current slider value.
        void sync_value_number_content() const;
        // Measures track, thumb, and optional value regions for the current slider state.
        [[nodiscard]] SliderLayout compute_layout() const noexcept;
        [[nodiscard]] float clamped_ratio() const noexcept;
        // Snaps a raw value to the configured step before it becomes visible state.
        [[nodiscard]] float snapped_value(float value) const noexcept;
        // Returns the increment used for keyboard-style slider adjustments.
        [[nodiscard]] float action_step() const noexcept;
        // Converts a pointer position into a normalized ratio along the active axis.
        [[nodiscard]] float ratio_from_point(const SliderLayout& layout,const elysia::core::Vector2& point) const noexcept;
        // Computes thumb drag bounds that keep the handle constrained to the track.
        [[nodiscard]] elysia::core::Rect handle_drag_bounds(const SliderLayout& layout) const noexcept;
        // Returns true only when the slider should react to action or pointer input.
        [[nodiscard]] bool can_interact() const noexcept;
        [[nodiscard]] bool can_receive_pointer() const noexcept;
        [[nodiscard]] bool contains_pointer(int mouse_x,int mouse_y) const noexcept;
        [[nodiscard]] bool contains_track_or_handle(const SliderLayout& layout,int mouse_x,int mouse_y) const noexcept;
        [[nodiscard]] bool is_primary_pointer_event(const UiInputEvent& event) const noexcept;
        // Updates the value and optionally notifies listeners only when it changed.
        [[nodiscard]] bool set_value_internal(float value,bool notify);
        // Recomputes value from a pointer position and optionally notifies listeners.
        [[nodiscard]] bool update_value_from_point(const SliderLayout& layout,const elysia::core::Vector2& point,bool notify);
        [[nodiscard]] elysia::core::Color current_background_color() const noexcept;
        [[nodiscard]] elysia::core::Color current_border_color() const noexcept;
        [[nodiscard]] elysia::core::Color current_fill_color() const noexcept;
        [[nodiscard]] elysia::core::Color current_text_color() const noexcept;
        // Wires thumb dragging callbacks back into slider value updates.
        void bind_handle_callbacks();
        // Clears pointer dragging and keyboard/gamepad adjustment after interaction becomes invalid.
        void clear_drag_state() noexcept;
        // Plays a configured sound only when the corresponding key is present.
        void play_sound_if_set(std::string_view sound_key) const;
        // Throttles repeated slide sounds while the thumb is actively moving.
        void play_slide_sound_if_allowed();

    private:
        mutable UiBar _bar;
        mutable UiDragHandle _handle;
        mutable UiNumber _value_number;
        std::optional<UiSliderSounds> _sounds;
        UiStyleState<UiSliderStyle> _style_state;
        UiSliderValueChangedCallback _on_value_changed;
        UiSliderOrientation _orientation = UiSliderOrientation::Horizontal;
        UiSliderValueDisplay _value_display = UiSliderValueDisplay::None;
        bool _is_adjusting = false;
        bool _drag_value_changed = false;
        float _bar_thickness = 6.0f;
        float _min_value = 0.0f;
        float _max_value = 1.0f;
        float _value = 0.0f;
        std::optional<float> _step = std::nullopt;
        std::uint32_t _last_slide_sound_ticks = 0;
        bool _has_last_slide_sound_tick = false;
    };
}


