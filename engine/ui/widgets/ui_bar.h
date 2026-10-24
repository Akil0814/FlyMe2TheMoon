#pragma once

#include "../style/ui_style.h"
#include "../style/ui_visual_roles.h"
#include "../style/ui_visual_styles.h"
#include "../core/ui_element.h"

namespace elysia::ui
{
enum class BarFillDirection
{
    LeftToRight,
    RightToLeft,
    TopToBottom,
    BottomToTop
};

class UiBar : public UiElement
{
public:
    explicit UiBar(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiBar(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiBar(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;
    ~UiBar() override = default;

    void reset() noexcept override;

    // Defines the numeric range used to convert values into a fill ratio.
    void set_range(float min_value,float max_value);
    void set_value(float value);
    // Sets the fill amount directly as a normalized ratio, bypassing range conversion.
    void set_ratio(float ratio);

    [[nodiscard]] float min_value() const;
    [[nodiscard]] float max_value() const;
    [[nodiscard]] float value() const;
    [[nodiscard]] float ratio() const;

    void set_base_style(const UiBarStyle& style) noexcept;
    void set_style_overrides(const UiBarStyleOverrides& overrides) noexcept;
    [[nodiscard]] const UiBarStyle& style() const noexcept;
    [[nodiscard]] const UiBarStyleOverrides& style_overrides() const noexcept;
    [[nodiscard]] bool has_style_overrides() const noexcept;
    void clear_style_overrides() noexcept;

    void set_visual_role(UiBarVisualRole role) noexcept;
    [[nodiscard]] UiBarVisualRole visual_role() const noexcept;

    void set_fill_direction(BarFillDirection direction);
    [[nodiscard]] BarFillDirection fill_direction() const;

    void set_padding(int padding);
    [[nodiscard]] int padding() const;

    // Emits background, fill, and optional border commands for the current bar state.
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

private:
    // Returns the drawable interior after subtracting visual padding.
    [[nodiscard]] elysia::core::Rect content_rect(const elysia::core::Rect& rect) const;
    // Computes the filled portion of the bar using the current direction and ratio.
    [[nodiscard]] elysia::core::Rect fill_rect(const elysia::core::Rect& rect) const;

private:
    float _min_value = 0.0f;
    float _max_value = 1.0f;
    float _value = 0.0f;
    UiStyleState<UiBarStyle> _style_state;
    UiBarVisualRole _visual_role = UiBarVisualRole::Default;
    BarFillDirection _fill_direction = BarFillDirection::LeftToRight;
    int _padding = 0;
};

}
