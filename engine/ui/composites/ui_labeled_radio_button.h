#pragma once

#include "../widgets/ui_radio_button.h"
#include "../widgets/label/ui_label.h"
#include "../text/ui_text_content.h"
#include "../text/ui_typography.h"

namespace elysia::ui
{
enum class UiLabeledRadioLabelPlacement { Left,Right };
enum class UiLabeledRadioTextPlacement { NearIndicator,FarEdge };

struct UiLabeledRadioButtonConfig
{
    UiRadioButtonConfig radio{};
    UiTextContent text_content{};
    UiLabeledRadioLabelPlacement label_placement = UiLabeledRadioLabelPlacement::Right;
    UiLabeledRadioTextPlacement text_placement = UiLabeledRadioTextPlacement::NearIndicator;
    float label_spacing = 8.0f;
};

class UiLabeledRadioButton final : public UiControl, public UiRadioItem
{
public:
    explicit UiLabeledRadioButton(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiLabeledRadioButton(const elysia::core::Rect& rect,const UiLabeledRadioButtonConfig& config,int order = 0) noexcept;
    void reset() noexcept override;
    void set_base_styles(
        const UiRadioButtonStyle& radio,
        const UiLabelStyle& label,
        const UiEnabledDisabledColors& text_colors) noexcept;
    void set_enabled(bool enabled) override;
    void set_focused(bool focused) override;
    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out) const override;
    void set_selected(bool selected) noexcept override;
    [[nodiscard]] bool is_selected() const noexcept override;
    void set_on_selection_changed(UiRadioButtonSelectedCallback callback);
    void set_text_content(UiTextContent content);
    [[nodiscard]] elysia::core::Color resolved_text_color() const noexcept;
    void set_label_placement(UiLabeledRadioLabelPlacement placement) noexcept;
    void set_text_placement(UiLabeledRadioTextPlacement placement) noexcept;
    void set_label_spacing(float spacing) noexcept;
    void set_typography_role(
        elysia::typography::UiTypographyRole role) noexcept;
    [[nodiscard]] UiElement& radio_item_element() noexcept override { return *this; }
    [[nodiscard]] const UiElement& radio_item_element() const noexcept override { return *this; }

private:
    void sync_children() const;
    [[nodiscard]] elysia::core::Rect indicator_rect() const noexcept;
    [[nodiscard]] elysia::core::Rect label_rect() const noexcept;
    [[nodiscard]] UiInputEvent routed_event(const UiInputEvent& event) const noexcept;
    mutable UiRadioButton _radio;
    mutable UiLabel _label;
    UiTextContent _text_content;
    elysia::typography::UiTypographyRole _typography_role =
        elysia::typography::UiTypographyRole::RadioLabel;
    UiLabelStyle _theme_label_style{};
    UiEnabledDisabledColors _theme_text_colors{};
    UiLabeledRadioLabelPlacement _label_placement = UiLabeledRadioLabelPlacement::Right;
    UiLabeledRadioTextPlacement _text_placement = UiLabeledRadioTextPlacement::NearIndicator;
    float _label_spacing = 8.0f;
};
}
