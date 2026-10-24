#include "font_settings.h"

#include <algorithm>

namespace elysia::typography
{
namespace
{
bool valid_font_source(FontSource source) noexcept
{
    return source == FontSource::EngineBuiltIn
        || source == FontSource::Project;
}

std::vector<int> sorted_unique(std::vector<int> point_sizes)
{
    std::sort(point_sizes.begin(),point_sizes.end());
    point_sizes.erase(
        std::unique(point_sizes.begin(),point_sizes.end()),
        point_sizes.end());
    return point_sizes;
}
}

ResolvedFontSettings::ResolvedFontSettings(
    FontSource ui_source,
    UiTypographyProfile ui_typography,
    FontSource floating_number_source,
    int floating_number_point_size,
    std::vector<int> engine_point_sizes,
    std::vector<int> project_point_sizes)
    : _ui_source(ui_source)
    , _ui_typography(std::move(ui_typography))
    , _floating_number_source(floating_number_source)
    , _floating_number_point_size(floating_number_point_size)
    , _engine_point_sizes(std::move(engine_point_sizes))
    , _project_point_sizes(std::move(project_point_sizes))
{
}

FontSource ResolvedFontSettings::ui_source() const noexcept
{
    return _ui_source;
}

const UiTypographyProfile& ResolvedFontSettings::ui_typography() const noexcept
{
    return _ui_typography;
}

FontSource ResolvedFontSettings::floating_number_source() const noexcept
{
    return _floating_number_source;
}

int ResolvedFontSettings::floating_number_point_size() const noexcept
{
    return _floating_number_point_size;
}

std::span<const int> ResolvedFontSettings::engine_point_sizes() const noexcept
{
    return _engine_point_sizes;
}

std::span<const int> ResolvedFontSettings::project_point_sizes() const noexcept
{
    return _project_point_sizes;
}

std::expected<ResolvedFontSettings,std::string>
resolve_font_settings(const FontSettings& settings)
{
    if (!valid_font_source(settings.ui.source)
        || !valid_font_source(settings.floating_number.source))
    {
        return std::unexpected("Font source is invalid.");
    }

    const UiTypographyProfile ui_typography =
        settings.ui.typography_override.value_or(
            UiTypographyProfile::engine_defaults());
    const int floating_number_point_size =
        settings.floating_number.point_size_override.value_or(
            ResolvedFontSettings::DefaultFloatingNumberPointSize);

    std::vector<int> ui_point_sizes(
        ui_typography.point_sizes().begin(),
        ui_typography.point_sizes().end());
    if (std::ranges::any_of(
            ui_point_sizes,
            [](int point_size) { return point_size <= 0; }))
    {
        return std::unexpected("UI typography point sizes must be positive.");
    }
    if (floating_number_point_size <= 0)
    {
        return std::unexpected(
            "Floating-number typography point size must be positive.");
    }

    std::vector<int> engine_point_sizes = ui_point_sizes;
    engine_point_sizes.push_back(floating_number_point_size);
    engine_point_sizes = sorted_unique(std::move(engine_point_sizes));

    std::vector<int> project_point_sizes;
    if (settings.ui.source == FontSource::Project)
    {
        project_point_sizes.insert(
            project_point_sizes.end(),
            ui_point_sizes.begin(),
            ui_point_sizes.end());
    }
    if (settings.floating_number.source == FontSource::Project)
        project_point_sizes.push_back(floating_number_point_size);
    project_point_sizes = sorted_unique(std::move(project_point_sizes));

    return ResolvedFontSettings(
        settings.ui.source,
        ui_typography,
        settings.floating_number.source,
        floating_number_point_size,
        std::move(engine_point_sizes),
        std::move(project_point_sizes));
}
}
