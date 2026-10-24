#pragma once

#include "../core/ui_text_align.h"
#include "../../typography/font_roles.h"

namespace elysia::ui
{
// Rendering inputs resolved from a role; colors remain part of the widget's visual style.
struct UiResolvedTextStyle
{
    bool wrap_allowed = false;
    TextHorizontalAlign horizontal_align_default = TextHorizontalAlign::Left;
};

[[nodiscard]] static inline UiResolvedTextStyle resolve_ui_typography(
    elysia::typography::UiTypographyRole role) noexcept
{
    using elysia::typography::UiTypographyRole;

    switch (role)
    {
    case UiTypographyRole::LabelMuted:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Left
        };
    case UiTypographyRole::Caption:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Left
        };
    case UiTypographyRole::Title:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Left
        };
    case UiTypographyRole::Heading:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Left
        };
    case UiTypographyRole::Subtitle:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Left
        };
    case UiTypographyRole::Button:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Center
        };
    case UiTypographyRole::ButtonCompact:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Center
        };
    case UiTypographyRole::Input:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Left
        };
    case UiTypographyRole::InputPlaceholder:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Left
        };
    case UiTypographyRole::Number:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Left
        };
    case UiTypographyRole::DialogTitle:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Left
        };
    case UiTypographyRole::DialogBody:
        return UiResolvedTextStyle{
            .wrap_allowed = true,
            .horizontal_align_default = TextHorizontalAlign::Left
        };
    case UiTypographyRole::DialogAction:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Center
        };
    case UiTypographyRole::SliderValue:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Center
        };
    case UiTypographyRole::CheckboxLabel:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Left
        };
    case UiTypographyRole::RadioLabel:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Left
        };
    case UiTypographyRole::Label:
    default:
        return UiResolvedTextStyle{
            .wrap_allowed = false,
            .horizontal_align_default = TextHorizontalAlign::Left
        };
    }
}
}
