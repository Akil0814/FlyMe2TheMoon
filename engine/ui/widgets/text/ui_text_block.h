#pragma once

#include "../../core/ui_element.h"
#include "../../core/ui_text_align.h"
#include "../../style/ui_style.h"
#include "../../style/ui_visual_roles.h"
#include "../../style/ui_visual_styles.h"
#include "../../text/ui_text_content.h"
#include "../../text/ui_typography.h"
#include "../../../typography/font_settings.h"

#include <optional>
#include <string>

struct SDL_Texture;

namespace elysia::ui
{
// Multi-line text widget whose intrinsic extent is measured from localized, wrapped output.
class UiTextBlock : public UiElement
{
public:
    explicit UiTextBlock(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiTextBlock(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiTextBlock(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;
    ~UiTextBlock() override = default;

    void reset() noexcept override;
    [[nodiscard]] elysia::core::Vector2 content_extent() const noexcept override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

    void set_text_content(UiTextContent text_content);
    [[nodiscard]] const UiTextContent& text_content() const noexcept;
    void clear_text();

    void set_base_style(const UiTextBlockStyle& style) noexcept;
    void set_style_overrides(const UiTextBlockStyleOverrides& overrides) noexcept;
    [[nodiscard]] const UiTextBlockStyle& style() const noexcept;
    [[nodiscard]] const UiTextBlockStyleOverrides& style_overrides() const noexcept;
    [[nodiscard]] bool has_style_overrides() const noexcept;
    void clear_style_overrides() noexcept;

    void set_visual_role(UiTextBlockVisualRole role) noexcept;
    [[nodiscard]] UiTextBlockVisualRole visual_role() const noexcept;

    void set_typography_role(
        elysia::typography::UiTypographyRole role) noexcept;
    [[nodiscard]] elysia::typography::UiTypographyRole
        typography_role() const noexcept;
    void set_font_source_override(
        elysia::typography::FontSource source) noexcept;
    void clear_font_source_override() noexcept;
    [[nodiscard]] std::optional<elysia::typography::FontSource>
        font_source_override() const noexcept;
    void set_padding(int padding) noexcept;
    [[nodiscard]] int padding() const noexcept;
    void set_horizontal_align(TextHorizontalAlign align) noexcept;
    [[nodiscard]] TextHorizontalAlign horizontal_align() const noexcept;

private:
    // TextKey and RawText share rendering but follow different localization paths.
    [[nodiscard]] bool has_text() const noexcept;
    [[nodiscard]] elysia::core::Rect content_rect() const noexcept;
    [[nodiscard]] elysia::core::Rect text_render_rect(SDL_Texture* text_texture) const noexcept;
    [[nodiscard]] std::string resolved_text() const;

private:
    UiTextContent _text_content;
    UiStyleState<UiTextBlockStyle> _style_state;
    UiTextBlockVisualRole _visual_role = UiTextBlockVisualRole::Default;
    elysia::typography::UiTypographyRole _typography_role =
        elysia::typography::UiTypographyRole::DialogBody;
    std::optional<elysia::typography::FontSource> _font_source_override;
    TextHorizontalAlign _horizontal_align = TextHorizontalAlign::Left;
    int _padding = 0;
};
}
