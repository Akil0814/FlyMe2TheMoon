#include "localization_service.h"

#include "localization_manager.h"

#include <utility>

namespace elysia::localization
{
std::string_view LocalizationService::tr(std::string_view key) const
{
	return LocalizationManager::instance()->tr(key);
}

SDL_Texture* LocalizationService::get_text_texture(
	std::string_view key,
	const LocalizedTextStyle& style)
{
	return LocalizationManager::instance()->get_text_texture(key,style);
}

SDL_Texture* LocalizationService::get_raw_text_texture(
	std::string_view text,
	const LocalizedTextStyle& style)
{
	return LocalizationManager::instance()->get_raw_text_texture(text,style);
}

CachedTexturePtr LocalizationService::create_uncached_raw_text_texture(
	std::string_view text,
	const LocalizedTextStyle& style)
{
	return LocalizationManager::instance()->create_uncached_raw_text_texture(text,style);
}

bool LocalizationService::measure_raw_text(
	std::string_view text,
	const LocalizedTextStyle& style,
	int& out_width,
	int& out_height) const
{
	return LocalizationManager::instance()->measure_raw_text(
		text,style,out_width,out_height);
}

std::uint64_t LocalizationService::font_generation() const noexcept
{
	return LocalizationManager::instance()->font_generation();
}

std::expected<void,LocalizationFailure> LocalizationService::set_language(std::string language)
{
	return LocalizationManager::instance()->set_language(std::move(language));
}

const std::string& LocalizationService::current_language() const
{
	return LocalizationManager::instance()->current_language();
}

const std::vector<std::string>& LocalizationService::supported_languages() const
{
	return LocalizationManager::instance()->supported_languages();
}
}
