#pragma once

#include "../widgets/ui_checkbox.h"
#include "../widgets/label/ui_label.h"
#include "../text/ui_text_content.h"
#include "../text/ui_typography.h"

#include <optional>

namespace elysia::ui
{
enum class UiLabeledCheckboxLabelPlacement { Left,Right };
enum class UiLabeledCheckboxTextPlacement { NearBox,FarEdge };

struct UiLabeledCheckboxConfig
{
    UiCheckboxConfig checkbox{};
    UiTextContent text_content{};
    UiLabeledCheckboxLabelPlacement label_placement = UiLabeledCheckboxLabelPlacement::Right;
    float label_spacing = 8.0f;
    UiLabeledCheckboxTextPlacement text_placement = UiLabeledCheckboxTextPlacement::NearBox;
    std::optional<UiEnabledDisabledColors> text_colors;
};

class UiLabeledCheckbox final : public UiControl
{
public:
    explicit UiLabeledCheckbox(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiLabeledCheckbox(const elysia::core::Rect& rect,const UiLabeledCheckboxConfig& config,int order = 0) noexcept;
    void reset() noexcept override;
    void set_base_styles(
        const UiCheckboxStyle& checkbox,
        const UiLabelStyle& label,
        const UiEnabledDisabledColors& text_colors) noexcept;
    void set_enabled(bool enabled) override;
    void set_focused(bool focused) override;
    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out) const override;

    void set_labeled_checkbox_config(const UiLabeledCheckboxConfig& config);
    void set_state(UiCheckboxState state) noexcept;
    [[nodiscard]] UiCheckboxState state() const noexcept;
    void set_checked(bool checked) noexcept;
    [[nodiscard]] bool is_checked() const noexcept;
    void toggle();
    void set_on_toggled(UiCheckboxToggledCallback callback);
    void set_text_content(UiTextContent content);
    [[nodiscard]] const UiTextContent& text_content() const noexcept;
    [[nodiscard]] elysia::core::Color resolved_text_color() const noexcept;
    void set_label_placement(UiLabeledCheckboxLabelPlacement placement) noexcept;
    void set_text_placement(UiLabeledCheckboxTextPlacement placement) noexcept;
    void set_label_spacing(float spacing) noexcept;
    void set_typography_role(
        elysia::typography::UiTypographyRole role) noexcept;
    void set_label_padding(int padding) noexcept;
    void set_padding(int padding) noexcept;

private:
    void sync_children() const;
    [[nodiscard]] elysia::core::Rect indicator_rect() const noexcept;
    [[nodiscard]] elysia::core::Rect label_rect() const noexcept;
    [[nodiscard]] UiInputEvent event_for_indicator(const UiInputEvent& event) const noexcept;

    mutable UiCheckbox _checkbox;
    mutable UiLabel _label;
    UiTextContent _text_content;
    elysia::typography::UiTypographyRole _typography_role =
        elysia::typography::UiTypographyRole::CheckboxLabel;
    UiLabeledCheckboxLabelPlacement _label_placement = UiLabeledCheckboxLabelPlacement::Right;
    UiLabeledCheckboxTextPlacement _text_placement = UiLabeledCheckboxTextPlacement::NearBox;
    UiLabelStyle _theme_label_style{};
    UiEnabledDisabledColors _theme_text_colors{};
    std::optional<UiEnabledDisabledColors> _text_colors_override;
    float _label_spacing = 8.0f;
    int _indicator_padding = 4;
    int _label_padding = 0;
};
}
