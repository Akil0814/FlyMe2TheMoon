#pragma once

#include "../core/render/color.h"
#include "../ui/text/ui_typography.h"
#include "../typography/font_settings.h"

#include <optional>

namespace elysia::localization
{
struct LocalizedTextStyle
{
	elysia::typography::UiTypographyRole typography_role =
		elysia::typography::UiTypographyRole::Label;
	std::optional<elysia::typography::FontSource> font_source_override;
	elysia::core::Color color{};
	int wrap_width = 0;
};

}
