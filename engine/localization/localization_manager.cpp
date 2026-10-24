#include "../tools/logger.h"
#include "../builtin/resources/builtin_resources.h"
#include "localization_manager.h"

#include "../core/render/sdl_convert.h"
#include "../io/json/json_loader.h"
#include "../io/loaders/i18n_manifest_loader.h"
#include "../io/path/path_manager.h"
#include "../typography/font_resolver.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <utility>

namespace elysia::localization
{
namespace
{
using TranslationTable = std::unordered_map<std::string, std::string>;

inline void hash_combine(std::size_t& seed,std::size_t value) noexcept
{
	seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

std::expected<void,std::string> flatten_locale_json(
	const elysia::io::json& node,
	const std::string& prefix,
	const std::string& pointer,
	TranslationTable& out_table
)
{
	if (node.is_string())
	{
		out_table[prefix] = node.get<std::string>();
		return {};
	}

	if (!node.is_object())
		return std::unexpected(pointer.empty() ? "/" : pointer);

	for (auto iterator = node.begin(); iterator != node.end(); ++iterator)
	{
		const std::string key = prefix.empty()
			? iterator.key()
			: prefix + "." + iterator.key();
		std::string escaped = iterator.key();
		size_t position = 0;
		while ((position = escaped.find('~',position)) != std::string::npos)
		{
			escaped.replace(position,1,"~0");
			position += 2;
		}
		position = 0;
		while ((position = escaped.find('/',position)) != std::string::npos)
		{
			escaped.replace(position,1,"~1");
			position += 2;
		}
		auto nested = flatten_locale_json(
			iterator.value(),key,pointer + "/" + escaped,out_table);
		if (!nested) return nested;
	}

	return {};
}

}

bool LocalizationManager::MissingTranslationWarningKey::operator==(
	const MissingTranslationWarningKey& other) const noexcept
{
	return locale == other.locale && key == other.key;
}

std::size_t LocalizationManager::MissingTranslationWarningKeyHash::operator()(
	const MissingTranslationWarningKey& value) const noexcept
{
	std::size_t seed = std::hash<std::string>{}(value.locale);
	hash_combine(seed,std::hash<std::string>{}(value.key));
	return seed;
}

std::expected<void,LocalizationFailure> LocalizationManager::initialize(
	SDL_Renderer* renderer,
	const std::filesystem::path& manifest_path,
	std::string initial_language,
	const elysia::typography::FontResolver* font_resolver
)
{
	shutdown();

	if (!renderer)
		return std::unexpected(make_localization_failure(
			LocalizationError::Dependency,
			"Localization initialization failed: renderer is null.","renderer"));
	if (!font_resolver)
		return std::unexpected(make_localization_failure(
			LocalizationError::Dependency,
			"Localization initialization failed: FontResolver is null.","font-resolver"));

	elysia::io::PathManager* path_manager = elysia::io::PathManager::instance();
	if (!path_manager || !path_manager->is_initialized())
		return std::unexpected(make_localization_failure(
			LocalizationError::Dependency,
			"Localization initialization failed: path manager is not initialized.",
			"path-manager"));

	elysia::io::I18nManifestLoader manifest_loader;
	auto manifest_result = manifest_loader.load(manifest_path);
	if (!manifest_result)
		return std::unexpected(LocalizationFailure{
			LocalizationError::Manifest,std::move(manifest_result.error().diagnostic)});
	_manifest = std::move(*manifest_result);

	if (_manifest.default_language.empty())
		return std::unexpected(make_localization_failure(
			LocalizationError::Manifest,
			"Localization initialization failed: default language is empty.",
			"i18n-manifest",{},manifest_path,manifest_path,"/default_language"));

	if (_manifest.languages.empty())
		return std::unexpected(make_localization_failure(
			LocalizationError::Manifest,
			"Localization initialization failed: supported language list is empty.",
			"i18n-manifest",{},manifest_path,manifest_path,"/languages"));

	if (!is_supported_language(_manifest.default_language))
		_manifest.languages.push_back(_manifest.default_language);

	_renderer = renderer;
	_font_resolver = font_resolver;
	_manifest_path = manifest_path;
	_i18n_root = path_manager->assets() / "i18n";

	if (auto default_result = ensure_language_loaded(_manifest.default_language);
		!default_result)
	{
		auto failure = std::move(default_result.error());
		shutdown();
		return std::unexpected(std::move(failure));
	}

	if (initial_language.empty() || !is_supported_language(initial_language))
		initial_language = _manifest.default_language;

	if (auto initial_result = ensure_language_loaded(initial_language); !initial_result)
	{
		const std::string formatted = elysia::core::format_failure_diagnostic(
			initial_result.error().diagnostic,initial_result.error().error_code(),
			"localization",path_manager->root());
		elysia::tools::Logger::instance()->warn(
			"localization",formatted,initial_result.error().diagnostic.origin);
		initial_language = _manifest.default_language;
	}

	_current_language = initial_language;
	_initialized = true;
	_text_texture_cache.clear();
	return {};
}

void LocalizationManager::shutdown()
{
	_text_texture_cache.clear();
	_translation_tables.clear();
	_warned_missing_translations.clear();
	_manifest = elysia::io::I18nManifest{};
	_manifest_path.clear();
	_i18n_root.clear();
	_current_language.clear();
	_font_resolver = nullptr;
	_renderer = nullptr;
	_initialized = false;
}

std::string_view LocalizationManager::tr(std::string_view key) const
{
	if (!_initialized)
		return key;

	const TranslationResolution resolution = resolve_translation(key);
	if (resolution.found)
		return resolution.text;

	warn_missing_translation_once(key);
	return key;
}

LocalizationManager::TranslationResolution LocalizationManager::resolve_translation(
	std::string_view key) const
{
	const auto current_table_iterator = _translation_tables.find(_current_language);
	if (current_table_iterator != _translation_tables.end())
	{
		if (const std::string* current_translation =
			lookup_translation(current_table_iterator->second,key))
		{
			return {*current_translation,true};
		}
	}

	const auto default_table_iterator = _translation_tables.find(_manifest.default_language);
	if (default_table_iterator != _translation_tables.end())
	{
		if (const std::string* default_translation =
			lookup_translation(default_table_iterator->second,key))
		{
			return {*default_translation,true};
		}
	}

	if (key.starts_with("engine."))
	{
		const auto current_locale =
			elysia::builtin::builtin_locale_id(_current_language);
		if (const std::string* translation =
			elysia::builtin::BuiltinResources::instance()->find_translation(current_locale, key))
			return {*translation,true};
		if (_current_language != "en")
		{
			if (const std::string* fallback = elysia::builtin::BuiltinResources::instance()->find_translation(
				elysia::builtin::BuiltinLocaleId::English, key))
				return {*fallback,true};
		}
	}

	return {key,false};
}

SDL_Texture* LocalizationManager::get_text_texture(
	std::string_view key,
	const LocalizedTextStyle& style
)
{
	if (!_initialized)
		return nullptr;

	return _text_texture_cache.get_or_create(
		_current_language,
		_font_resolver ? _font_resolver->generation() : 0,
		key,
		style,
		[this, key, style]()
		{
			return create_text_texture(key, style);
		});
}

SDL_Texture* LocalizationManager::get_raw_text_texture(
	std::string_view text,
	const LocalizedTextStyle& style
)
{
	if (!_initialized)
		return nullptr;

	return _text_texture_cache.get_or_create_raw(
		_current_language,
		_font_resolver ? _font_resolver->generation() : 0,
		text,
		style,
		[this, text, style]()
		{
			return create_raw_text_texture(text, style);
		});
}

CachedTexturePtr LocalizationManager::create_uncached_raw_text_texture(
	std::string_view text,
	const LocalizedTextStyle& style
)
{
	if (!_initialized)
		return {};

	return create_raw_text_texture(text,style);
}

bool LocalizationManager::measure_raw_text(
	std::string_view text,
	const LocalizedTextStyle& style,
	int& out_width,
	int& out_height
) const
{
	out_width = 0;
	out_height = 0;

	if (!_initialized)
		return false;

	TTF_Font* font = resolve_text_font(style);
	if (!font)
		return false;

	if (text.empty())
	{
		out_height = TTF_GetFontHeight(font);
		return true;
	}

	const std::string raw_text(text);
	if (style.wrap_width > 0)
	{
		// SDL_ttf has no wrapped measurement API. Rendering to a temporary surface is
		// the same layout path used for the final texture, so its extent is authoritative.
		SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font, raw_text.c_str(), 0, SDL_Color{ 255,255,255,255 }, style.wrap_width);
		if (!surface)
		{
			ELYSIA_LOG_WARN("localization","Measure wrapped raw text failed, error: " << SDL_GetError());
			return false;
		}

		out_width = surface->w;
		out_height = surface->h;
		SDL_DestroySurface(surface);
		return true;
	}

	if (!TTF_GetStringSize(font, raw_text.c_str(), 0, &out_width, &out_height))
	{
		ELYSIA_LOG_WARN("localization","Measure raw text failed, error: " << SDL_GetError());
		return false;
	}

	return true;
}

std::uint64_t LocalizationManager::font_generation() const noexcept
{
	return _font_resolver ? _font_resolver->generation() : 0;
}

std::expected<void,LocalizationFailure> LocalizationManager::set_language(
	std::string language)
{
	if (!_initialized)
		return std::unexpected(make_localization_failure(
			LocalizationError::Dependency,
			"Set language failed: localization manager is not initialized.",
			"localization-manager"));

	if (!is_supported_language(language))
		return std::unexpected(make_localization_failure(
			LocalizationError::Language,
			"Set language failed: unsupported language: " + language,
			"language",language));

	if (auto loaded = ensure_language_loaded(language); !loaded)
		return loaded;

	_current_language = std::move(language);
	_text_texture_cache.clear();
	return {};
}

const std::string& LocalizationManager::current_language() const
{
	return _current_language;
}

const std::vector<std::string>& LocalizationManager::supported_languages() const
{
	return _manifest.languages;
}

void LocalizationManager::clear_texture_cache()
{
	_text_texture_cache.clear();
}

bool LocalizationManager::is_supported_language(const std::string& language) const
{
	return std::find(
		_manifest.languages.begin(),
		_manifest.languages.end(),
		language) != _manifest.languages.end();
}

std::expected<void,LocalizationFailure> LocalizationManager::ensure_language_loaded(
	const std::string& language)
{
	if (_translation_tables.contains(language))
		return {};

	auto table = load_language_table(language);
	if (!table) return std::unexpected(std::move(table.error()));

	_translation_tables.emplace(language,std::move(*table));
	return {};
}

std::expected<LocalizationManager::TranslationTable,LocalizationFailure>
LocalizationManager::load_language_table(const std::string& language) const
{
	auto locale_directory = resolve_locale_directory(language);
	if (!locale_directory) return std::unexpected(std::move(locale_directory.error()));

	TranslationTable merged_table;
	for (const elysia::io::I18nManifestFile& manifest_file : _manifest.files)
	{
		const std::filesystem::path full_file_path =
			*locale_directory / manifest_file.path;
		elysia::io::JsonLoader loader;
		const auto open_result = loader.open_file(full_file_path);
		if (!open_result)
			return std::unexpected(make_localization_failure(
				LocalizationError::Language,
				"Load language table failed: " + open_result.error().message,
				"language-file",language,full_file_path,
				manifest_file.origin.config_path,manifest_file.origin.json_pointer,
				open_result.error().origin));

		auto flattened = flatten_locale_json(loader.root(),"","",merged_table);
		if (!flattened)
			return std::unexpected(make_localization_failure(
				LocalizationError::Language,
				"Load language table failed: unsupported locale JSON shape at "
					+ flattened.error() + ".",
				"language-file",language,full_file_path,full_file_path,
				flattened.error()));
	}

	return merged_table;
}

std::expected<std::filesystem::path,LocalizationFailure>
LocalizationManager::resolve_locale_directory(
	const std::string& language
) const
{
	const std::filesystem::path direct_path = _i18n_root / language;
	std::error_code error;
	if (std::filesystem::is_directory(direct_path,error))
		return direct_path;
	if (error && error != std::errc::no_such_file_or_directory)
		return std::unexpected(make_localization_failure(
			LocalizationError::Locale,
			"Resolve locale directory failed: " + error.message(),
			"locale-directory",language,direct_path,_manifest_path));
	return std::unexpected(make_localization_failure(
		LocalizationError::Locale,
		"Load language table failed: locale directory not found for " + language,
		"locale-directory",language,direct_path,_manifest_path));
}

const std::string* LocalizationManager::lookup_translation(
	const TranslationTable& table,
	std::string_view key
) const
{
	const auto iterator = table.find(std::string(key));
	if (iterator == table.end())
		return nullptr;

	return &iterator->second;
}

void LocalizationManager::warn_missing_translation_once(std::string_view key) const
{
	MissingTranslationWarningKey warning_key{
		_current_language,
		std::string(key)
	};
	if (!_warned_missing_translations.insert(std::move(warning_key)).second)
		return;

	ELYSIA_LOG_WARN("localization","Missing localization key: key='"
		<< key << "', locale='" << _current_language << "'");
}

TTF_Font* LocalizationManager::resolve_text_font(
	const LocalizedTextStyle& style) const
{
	if (!_font_resolver)
	{
		ELYSIA_LOG_WARN("localization",
			"Resolve text font failed: FontResolver is unavailable.");
		return nullptr;
	}

	const auto resolved = _font_resolver->resolve_ui(
		style.typography_role,
		_current_language,
		style.font_source_override);
	if (!resolved)
	{
		ELYSIA_LOG_WARN("localization",
			"Resolve text font failed: " << resolved.error().message);
		return nullptr;
	}

	return resolved->font;
}

CachedTexturePtr LocalizationManager::create_text_texture(
	std::string_view key,
	const LocalizedTextStyle& style
)
{
	if (!_renderer)
		return {};

	TTF_Font* font = resolve_text_font(style);
	if (!font)
		return {};

	const std::string translated_text(tr(key));
	if (translated_text.empty())
	{
		ELYSIA_LOG_WARN("localization","Create text texture failed: translated text is empty: "
			<< key);
		return {};
	}

	SDL_Surface* surface = nullptr;
    const SDL_Color text_color = elysia::core::to_sdl_color(style.color);
	if (style.wrap_width > 0)
	{
		surface = TTF_RenderText_Blended_Wrapped(font, translated_text.c_str(), 0, text_color, style.wrap_width);
	}
	else
	{
		surface = TTF_RenderText_Blended(font, translated_text.c_str(), 0, text_color);
	}

	if (!surface)
	{
		ELYSIA_LOG_WARN("localization","Create text texture failed: TTF render failed for key "
			<< key << ", error: " << SDL_GetError());
		return {};
	}

	SDL_Texture* texture = SDL_CreateTextureFromSurface(_renderer, surface);
	SDL_DestroySurface(surface);
	if (!texture)
	{
		ELYSIA_LOG_WARN("localization","Create text texture failed: SDL_CreateTextureFromSurface failed for key "
			<< key << ", error: " << SDL_GetError());
		return {};
	}

	return CachedTexturePtr(texture);
}

CachedTexturePtr LocalizationManager::create_raw_text_texture(
	std::string_view text,
	const LocalizedTextStyle& style
)
{
	if (!_renderer)
		return {};

	TTF_Font* font = resolve_text_font(style);
	if (!font)
		return {};

	const std::string raw_text(text);
	if (raw_text.empty())
		return {};

	SDL_Surface* surface = nullptr;
	const SDL_Color text_color = elysia::core::to_sdl_color(style.color);
	if (style.wrap_width > 0)
	{
		surface = TTF_RenderText_Blended_Wrapped(font, raw_text.c_str(), 0, text_color, style.wrap_width);
	}
	else
	{
		surface = TTF_RenderText_Blended(font, raw_text.c_str(), 0, text_color);
	}

	if (!surface)
	{
		ELYSIA_LOG_WARN("localization","Create raw text texture failed, error: "
			<< SDL_GetError());
		return {};
	}

	SDL_Texture* texture = SDL_CreateTextureFromSurface(_renderer, surface);
	SDL_DestroySurface(surface);
	if (!texture)
	{
		ELYSIA_LOG_WARN("localization","Create raw text texture failed: SDL_CreateTextureFromSurface failed, error: "
			<< SDL_GetError());
		return {};
	}

	return CachedTexturePtr(texture);
}

}
