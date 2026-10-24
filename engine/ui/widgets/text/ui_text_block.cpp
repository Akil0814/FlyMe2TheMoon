#include "engine/core/render/sdl_texture_size.h"
#include "ui_text_block.h"

#include "../../style/ui_style_defaults.h"
#include "../../../core/render/render_command.h"
#include "../../../localization/localization_service.h"
#include "../../../localization/localized_text_style.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <utility>

namespace elysia::ui
{
using elysia::typography::UiTypographyRole;

namespace
{
[[nodiscard]] UiLabelVisualRole label_role_for_text_block(UiTextBlockVisualRole role) noexcept
{
    switch (role)
    {
    case UiTextBlockVisualRole::Muted:
        return UiLabelVisualRole::Muted;
    case UiTextBlockVisualRole::Default:
    default:
        return UiLabelVisualRole::Default;
    }
}
}

UiTextBlock::UiTextBlock(const elysia::core::Rect& rect,int order) noexcept
    : UiElement(rect,order)
{
    reset();
}

UiTextBlock::UiTextBlock(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiElement(position,size,order)
{
    reset();
}

UiTextBlock::UiTextBlock(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiElement(center,size,from_center,order)
{
    reset();
}

void UiTextBlock::reset() noexcept
{
    UiElement::reset();
    _text_content = UiTextContent{};
    _style_state.reset(UiStyleDefaults::text_block());
    _visual_role = UiTextBlockVisualRole::Default;
    _typography_role = UiTypographyRole::DialogBody;
    _font_source_override.reset();
    _horizontal_align = resolve_ui_typography(_typography_role).horizontal_align_default;
    _padding = 0;
}

elysia::core::Vector2 UiTextBlock::content_extent() const noexcept
{
    const elysia::core::Rect content = content_rect();
    const float width = content.width();
    if (!has_text() || width <= 0.0f)
        return elysia::core::Vector2(std::max(0.0f,size().x),std::max(0.0f,size().y));

    auto* localization_service = ELYSIA_LOCALIZATION;

    const UiResolvedTextStyle typography = resolve_ui_typography(_typography_role);
    elysia::localization::LocalizedTextStyle text_style;
    text_style.typography_role = _typography_role;
    text_style.font_source_override = _font_source_override;
    text_style.color = _style_state.effective_style().text;
    text_style.wrap_width = typography.wrap_allowed ? std::max(0,static_cast<int>(width)) : 0;

    int measured_width = 0;
    int measured_height = 0;
    const std::string text = resolved_text();
    if (!localization_service->measure_raw_text(text,text_style,measured_width,measured_height))
        return elysia::core::Vector2(std::max(0.0f,size().x),std::max(0.0f,size().y));

    const float padding = static_cast<float>(_padding);
    return elysia::core::Vector2(
        std::max(static_cast<float>(measured_width) + padding * 2.0f,size().x),
        std::max(static_cast<float>(measured_height) + padding * 2.0f,size().y));
}

void UiTextBlock::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;

    const elysia::core::Rect block_rect = screen_rect();
    if (block_rect.is_empty())
        return;

    const UiTextBlockStyle& style = _style_state.effective_style();
    if (style.draw_background)
        out_commands.push_back(elysia::core::make_ui_fill_rect_command(block_rect,apply_opacity(style.background),style.corner_radius));

    if (!has_text())
        return;

    auto* localization_service = ELYSIA_LOCALIZATION;

    const UiResolvedTextStyle typography = resolve_ui_typography(_typography_role);
    elysia::localization::LocalizedTextStyle text_style;
    text_style.typography_role = _typography_role;
    text_style.font_source_override = _font_source_override;
    text_style.color = style.text;
    text_style.wrap_width = typography.wrap_allowed ? std::max(0,static_cast<int>(content_rect().width())) : 0;

    SDL_Texture* text_texture = nullptr;
    switch (_text_content.kind)
    {
    case UiTextContentKind::TextKey:
        text_texture = localization_service->get_text_texture(_text_content.value,text_style);
        break;
    case UiTextContentKind::RawText:
        text_texture = localization_service->get_raw_text_texture(_text_content.value,text_style);
        break;
    case UiTextContentKind::None:
    default:
        return;
    }
    if (!text_texture)
        return;

    const elysia::core::Rect text_rect = text_render_rect(text_texture);
    if (text_rect.is_empty())
        return;

    auto command = elysia::core::make_ui_texture_command(text_texture,text_rect);
    apply_opacity(command);
    out_commands.push_back(command);
}

void UiTextBlock::set_text_content(UiTextContent text_content)
{
    _text_content = std::move(text_content);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

const UiTextContent& UiTextBlock::text_content() const noexcept
{
    return _text_content;
}

void UiTextBlock::clear_text()
{
    set_text_content(UiTextContent{});
}

void UiTextBlock::set_base_style(const UiTextBlockStyle& style) noexcept
{
    _style_state.set_base_style(style);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiTextBlock::set_style_overrides(const UiTextBlockStyleOverrides& overrides) noexcept
{
    _style_state.set_style_overrides(overrides);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

const UiTextBlockStyle& UiTextBlock::style() const noexcept
{
    return _style_state.effective_style();
}

const UiTextBlockStyleOverrides& UiTextBlock::style_overrides() const noexcept { return _style_state.style_overrides(); }
bool UiTextBlock::has_style_overrides() const noexcept
{
    return _style_state.has_style_overrides();
}

void UiTextBlock::clear_style_overrides() noexcept
{
    _style_state.clear_style_overrides();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiTextBlock::set_visual_role(UiTextBlockVisualRole role) noexcept
{
    _visual_role = role;
    notify_base_style_invalidated();
}

UiTextBlockVisualRole UiTextBlock::visual_role() const noexcept
{
    return _visual_role;
}

void UiTextBlock::set_typography_role(UiTypographyRole role) noexcept
{
    _typography_role = role;
    _horizontal_align = resolve_ui_typography(_typography_role).horizontal_align_default;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

UiTypographyRole UiTextBlock::typography_role() const noexcept
{
    return _typography_role;
}

void UiTextBlock::set_font_source_override(
    elysia::typography::FontSource source) noexcept
{
    if (_font_source_override == source)
        return;
    _font_source_override = source;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiTextBlock::clear_font_source_override() noexcept
{
    if (!_font_source_override)
        return;
    _font_source_override.reset();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

std::optional<elysia::typography::FontSource>
UiTextBlock::font_source_override() const noexcept
{
    return _font_source_override;
}

void UiTextBlock::set_padding(int padding) noexcept
{
    _padding = std::max(0,padding);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

int UiTextBlock::padding() const noexcept
{
    return _padding;
}

void UiTextBlock::set_horizontal_align(TextHorizontalAlign align) noexcept
{
    _horizontal_align = align;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

TextHorizontalAlign UiTextBlock::horizontal_align() const noexcept
{
    return _horizontal_align;
}

bool UiTextBlock::has_text() const noexcept
{
    return !_text_content.empty();
}

elysia::core::Rect UiTextBlock::content_rect() const noexcept
{
    const elysia::core::Rect& rect = screen_rect();
    const float width = std::max(0.0f,rect.width());
    const float height = std::max(0.0f,rect.height());
    const float padding = static_cast<float>(_padding);
    const float pad_x = std::min(padding,width * 0.5f);
    const float pad_y = std::min(padding,height * 0.5f);

    elysia::core::Rect content = rect;
    content.set_x(rect.x() + pad_x);
    content.set_y(rect.y() + pad_y);
    content.set_width(width - pad_x * 2.0f);
    content.set_height(height - pad_y * 2.0f);
    return content;
}

elysia::core::Rect UiTextBlock::text_render_rect(SDL_Texture* text_texture) const noexcept
{
    if (!text_texture)
        return elysia::core::Rect::zero();

    int texture_width = 0;
    int texture_height = 0;
    if (!elysia::core::texture_pixel_size(text_texture,&texture_width,&texture_height))
        return elysia::core::Rect::zero();
    if (texture_width <= 0 || texture_height <= 0)
        return elysia::core::Rect::zero();

    const elysia::core::Rect available_rect = content_rect();
    if (available_rect.is_empty())
        return elysia::core::Rect::zero();

    float x = available_rect.x();
    switch (_horizontal_align)
    {
    case TextHorizontalAlign::Center:
        x = available_rect.center().x - static_cast<float>(texture_width) * 0.5f;
        break;
    case TextHorizontalAlign::Right:
        x = available_rect.right() - static_cast<float>(texture_width);
        break;
    case TextHorizontalAlign::Left:
    default:
        x = available_rect.x();
        break;
    }

    return elysia::core::Rect(
        x,
        available_rect.y(),
        static_cast<float>(texture_width),
        static_cast<float>(texture_height));
}

std::string UiTextBlock::resolved_text() const
{
    if (!has_text())
        return {};

    if (_text_content.kind == UiTextContentKind::RawText)
        return _text_content.value;

    return std::string(ELYSIA_LOCALIZATION->tr(_text_content.value));
}

}
