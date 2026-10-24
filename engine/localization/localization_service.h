#pragma once

#include "localized_text_style.h"
#include "text_texture_cache.h"
#include "localization_failure.h"
#include "../tools/singleton.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#define ELYSIA_LOCALIZATION (::elysia::localization::LocalizationService::instance())

namespace elysia::localization
{
class LocalizationService final : public elysia::tools::Singleton<LocalizationService>
{
	friend elysia::tools::Singleton<LocalizationService>;

public:
	[[nodiscard]] std::string_view tr(std::string_view key) const;
	[[nodiscard]] SDL_Texture* get_text_texture(
		std::string_view key,
		const LocalizedTextStyle& style);
	[[nodiscard]] SDL_Texture* get_raw_text_texture(
		std::string_view text,
		const LocalizedTextStyle& style);
	[[nodiscard]] CachedTexturePtr create_uncached_raw_text_texture(
		std::string_view text,
		const LocalizedTextStyle& style);
	[[nodiscard]] bool measure_raw_text(
		std::string_view text,
		const LocalizedTextStyle& style,
		int& out_width,
		int& out_height) const;

	[[nodiscard]] std::uint64_t font_generation() const noexcept;
	[[nodiscard]] std::expected<void,LocalizationFailure> set_language(std::string language);
	[[nodiscard]] const std::string& current_language() const;
	[[nodiscard]] const std::vector<std::string>& supported_languages() const;

private:
	LocalizationService() = default;
};
}
