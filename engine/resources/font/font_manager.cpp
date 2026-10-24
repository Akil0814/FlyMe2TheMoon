#include "font_manager.h"
#include "../../tools/logger.h"
namespace elysia::resources
{
FontManager::~FontManager()
{
	clear();
}

std::expected<void,ResourceFailure> FontManager::load_font(
	const std::string& key,
	const std::filesystem::path& file_path,
	int point_size
)
{
	if (key.empty())
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Load font failed: key is empty."));

	if (file_path.empty())
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Load font failed: file path is empty.",key));

	if (point_size <= 0)
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Load font failed: point size is invalid.",
			key,file_path));

	TTF_Font* font = TTF_OpenFont(file_path.string().c_str(), point_size);
	if (!font)
		return std::unexpected(make_resource_failure(
			ResourceError::DecodeFailed,std::string("Load font failed: ") + SDL_GetError(),
			key,file_path));

	return store_font(key, font);
}

std::expected<void,ResourceFailure> FontManager::load_font(
	const FontLoadRequest& request)
{
	auto result = load_font(request.key,request.file_path,request.point_size);
	if (result) return result;
	return std::unexpected(make_resource_failure(
		result.error().code,result.error().diagnostic.message,"font",request.key,
		request.file_path,request.origin,result.error().diagnostic.origin));
}

std::expected<void,ResourceFailure> FontManager::store_font(
	const std::string& key,TTF_Font* font)
{
	if (key.empty())
	{
		if (font)
		{
			TTF_CloseFont(font);
		}
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Store font failed: key is empty."));
	}

	if (!font)
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Store font failed: font is null.",key));

	FontPool::iterator iterator = _font_pool.find(key);
	if (iterator != _font_pool.end())
	{
		if (iterator->second)
		{
			TTF_CloseFont(iterator->second);
		}

		iterator->second = font;
		return {};
	}

	_font_pool.emplace(key, font);
	return {};
}

bool FontManager::has_font(std::string_view key) const noexcept
{
	return !key.empty() && _font_pool.contains(std::string(key));
}

TTF_Font* FontManager::find_font(const std::string_view& key) const
{
	if (key.empty())
	{
		ELYSIA_LOG_WARN("resource","Find font failed: key is empty.");
		return nullptr;
	}

	FontPool::const_iterator iterator = _font_pool.find(std::string(key));
	if (iterator == _font_pool.end())
	{
		ELYSIA_LOG_WARN("resource","Find font failed: resource does not exist: "
			<< key);
		return nullptr;
	}

	return iterator->second;
}

void FontManager::clear()
{
	for (FontPool::value_type& font : _font_pool)
	{
		if (font.second)
			TTF_CloseFont(font.second);
	}

	_font_pool.clear();
}

size_t FontManager::resource_count() const
{
	return _font_pool.size();
}

}
