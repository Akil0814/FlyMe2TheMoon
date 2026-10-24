#pragma once

#include "font_roles.h"

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace elysia::typography
{
class UiTypographyProfile
{
public:
    static constexpr std::size_t RoleCount =
        static_cast<std::size_t>(UiTypographyRole::Count);
    using PointSizes = std::array<int,RoleCount>;

    UiTypographyProfile() = delete;

    explicit constexpr UiTypographyProfile(PointSizes point_sizes) noexcept
        : _point_sizes(std::move(point_sizes))
    {
    }

    [[nodiscard]] constexpr int point_size(UiTypographyRole role) const
    {
        return _point_sizes.at(static_cast<std::size_t>(role));
    }

    [[nodiscard]] constexpr const PointSizes& point_sizes() const noexcept
    {
        return _point_sizes;
    }

    [[nodiscard]] static constexpr UiTypographyProfile engine_defaults() noexcept
    {
        return UiTypographyProfile(PointSizes{
            30, // Label
            30, // LabelMuted
            70, // Title
            50, // Subtitle
            30, // Button
            20, // ButtonCompact
            30, // Input
            20, // InputPlaceholder
            30, // Number
            60, // DialogTitle
            30, // DialogBody
            20, // DialogAction
            30, // SliderValue
            30, // CheckboxLabel
            30, // RadioLabel
            10, // Caption
            40  // Heading
        });
    }

private:
    PointSizes _point_sizes;
};

enum class FontSource
{
    EngineBuiltIn,
    Project
};

struct UiFontSettings
{
    FontSource source = FontSource::EngineBuiltIn;
    std::optional<UiTypographyProfile> typography_override;
};

struct FloatingNumberFontSettings
{
    FontSource source = FontSource::EngineBuiltIn;
    std::optional<int> point_size_override;
};

struct FontSettings
{
    UiFontSettings ui;
    FloatingNumberFontSettings floating_number;
};

class ResolvedFontSettings
{
public:
    static constexpr int DefaultFloatingNumberPointSize = 20;

    [[nodiscard]] FontSource ui_source() const noexcept;
    [[nodiscard]] const UiTypographyProfile& ui_typography() const noexcept;
    [[nodiscard]] FontSource floating_number_source() const noexcept;
    [[nodiscard]] int floating_number_point_size() const noexcept;
    [[nodiscard]] std::span<const int> engine_point_sizes() const noexcept;
    [[nodiscard]] std::span<const int> project_point_sizes() const noexcept;

private:
    friend std::expected<ResolvedFontSettings,std::string>
        resolve_font_settings(const FontSettings&);

    ResolvedFontSettings(
        FontSource ui_source,
        UiTypographyProfile ui_typography,
        FontSource floating_number_source,
        int floating_number_point_size,
        std::vector<int> engine_point_sizes,
        std::vector<int> project_point_sizes);

private:
    FontSource _ui_source;
    UiTypographyProfile _ui_typography;
    FontSource _floating_number_source;
    int _floating_number_point_size;
    std::vector<int> _engine_point_sizes;
    std::vector<int> _project_point_sizes;
};

[[nodiscard]] std::expected<ResolvedFontSettings,std::string>
    resolve_font_settings(const FontSettings& settings);
}
