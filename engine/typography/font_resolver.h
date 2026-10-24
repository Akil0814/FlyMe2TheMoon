#pragma once

#include "font_roles.h"
#include "font_settings.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace elysia::resources
{
class ResourceService;
}

namespace elysia::typography
{
enum class FontResolveErrorCode
{
    NotConfigured,
    InvalidRole,
    InvalidPointSize,
    InvalidSource,
    UnsupportedLanguage,
    FontUnavailable
};

struct FontResolveError
{
    FontResolveErrorCode code = FontResolveErrorCode::FontUnavailable;
    std::string message;
};

struct ResolvedFont
{
    TTF_Font* font = nullptr;
    int point_size = 0;
    FontSource source = FontSource::EngineBuiltIn;
    std::uint64_t generation = 0;
};

class FontResolver
{
public:
    FontResolver() = default;
    ~FontResolver() = default;

    FontResolver(const FontResolver&) = delete;
    FontResolver& operator=(const FontResolver&) = delete;
    FontResolver(FontResolver&&) = delete;
    FontResolver& operator=(FontResolver&&) = delete;

    [[nodiscard]] std::expected<void,FontResolveError> configure(
        const ResolvedFontSettings& settings,
        const elysia::resources::ResourceService& resource_service,
        std::span<const std::string> supported_languages);
    void shutdown() noexcept;

    [[nodiscard]] std::expected<ResolvedFont,FontResolveError> resolve_ui(
        UiTypographyRole role,
        std::string_view language,
        std::optional<FontSource> source_override = std::nullopt) const;
    [[nodiscard]] std::expected<ResolvedFont,FontResolveError> resolve_effect(
        EffectTypographyRole role) const;

    [[nodiscard]] std::expected<void,FontResolveError>
        activate_project_fonts();
    void deactivate_project_fonts() noexcept;

    [[nodiscard]] bool configured() const noexcept;
    [[nodiscard]] bool project_fonts_active() const noexcept;
    [[nodiscard]] std::uint64_t generation() const noexcept;
    [[nodiscard]] std::span<const int> project_point_sizes() const noexcept;

private:
    [[nodiscard]] std::expected<ResolvedFont,FontResolveError> resolve(
        FontSource configured_source,
        std::string_view language,
        int point_size) const;
    [[nodiscard]] std::expected<ResolvedFont,FontResolveError> resolve_exact(
        FontSource source,
        std::string_view language,
        int point_size) const;
    [[nodiscard]] std::expected<TTF_Font*,FontResolveError> find_font(
        FontSource source,
        std::string_view language,
        int point_size) const;
    [[nodiscard]] std::expected<void,FontResolveError>
        validate_engine_fonts() const;
    [[nodiscard]] std::expected<void,FontResolveError>
        validate_project_fonts() const;

private:
    std::optional<ResolvedFontSettings> _settings;
    const elysia::resources::ResourceService* _resource_service = nullptr;
    std::vector<std::string> _supported_languages;
    std::uint64_t _generation = 0;
    bool _configured = false;
    bool _project_fonts_active = false;
};
}
