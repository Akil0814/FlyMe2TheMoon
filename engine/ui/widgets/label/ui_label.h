#pragma once

#include "../../style/ui_style.h"
#include "../../style/ui_visual_roles.h"
#include "../../style/ui_visual_styles.h"
#include "../../core/ui_element.h"
#include "../../core/ui_text_align.h"
#include "../../text/ui_text_content.h"
#include "../../text/ui_typography.h"
#include "../../../typography/font_settings.h"

#include <optional>
#include <string>

struct SDL_Texture;

namespace elysia::ui
{
enum class UiLabelTextFitMode
{
    None,
    ShrinkToFit,
    ScaleToFit
};

// Single-line text widget. Typography chooses the source font and fit mode adapts it to the available area.
class UiLabel : public UiElement
{
public:
    explicit UiLabel(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0,UiTextContent text_content = {}) noexcept;

    UiLabel(const elysia::core::Vector2& position,const elysia::core::Vector2& size,
        int order = 0,UiTextContent text_content = {}) noexcept;

    UiLabel(const elysia::core::Vector2& center,const elysia::core::Vector2& size,
        UiFromCenterTag,int order = 0,UiTextContent text_content = {}) noexcept;

    ~UiLabel() override = default;

    void reset() noexcept override;
    // Emits background and text commands for the current label content and alignment.
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

    void set_text_content(UiTextContent text_content);
    [[nodiscard]] const UiTextContent& text_content() const noexcept;

    void set_base_style(const UiLabelStyle& style) noexcept;
    void set_style_overrides(const UiLabelStyleOverrides& overrides) noexcept;
    [[nodiscard]] const UiLabelStyle& style() const noexcept;
    [[nodiscard]] const UiLabelStyleOverrides& style_overrides() const noexcept;
    [[nodiscard]] bool has_style_overrides() const noexcept;
    void clear_style_overrides() noexcept;

    void set_visual_role(UiLabelVisualRole role) noexcept;
    [[nodiscard]] UiLabelVisualRole visual_role() const noexcept;

    void set_horizontal_align(TextHorizontalAlign align);
    [[nodiscard]] TextHorizontalAlign horizontal_align() const noexcept;

    void set_vertical_align(TextVerticalAlign align);
    [[nodiscard]] TextVerticalAlign vertical_align() const noexcept;

    void set_typography_role(
        elysia::typography::UiTypographyRole role) noexcept;
    [[nodiscard]] elysia::typography::UiTypographyRole
        typography_role() const noexcept;
    void set_font_source_override(
        elysia::typography::FontSource source) noexcept;
    void clear_font_source_override() noexcept;
    [[nodiscard]] std::optional<elysia::typography::FontSource>
        font_source_override() const noexcept;

    void set_text_fit_mode(UiLabelTextFitMode mode) noexcept;
    [[nodiscard]] UiLabelTextFitMode text_fit_mode() const noexcept;

    void set_padding(int padding);
    [[nodiscard]] int padding() const noexcept;

private:
    // Returns the padded interior used to position rendered text.
    [[nodiscard]] elysia::core::Rect content_rect() const noexcept;
    // Fits rendered text into the content rect with the configured uniform scaling and alignment.
    [[nodiscard]] elysia::core::Rect text_render_rect(SDL_Texture* text_texture) const noexcept;

private:
    UiTextContent _text_content;
    UiStyleState<UiLabelStyle> _style_state;
    UiLabelVisualRole _visual_role = UiLabelVisualRole::Default;
    elysia::typography::UiTypographyRole _typography_role =
        elysia::typography::UiTypographyRole::Label;
    std::optional<elysia::typography::FontSource> _font_source_override;
    UiLabelTextFitMode _text_fit_mode = UiLabelTextFitMode::ShrinkToFit;
    TextHorizontalAlign _horizontal_align = TextHorizontalAlign::Left;
    TextVerticalAlign _vertical_align = TextVerticalAlign::Top;
    int _padding = 0;
};
}
