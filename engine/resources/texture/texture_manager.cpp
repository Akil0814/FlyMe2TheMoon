#include "../../tools/logger.h"
#include "texture_manager.h"
#include <unordered_set>
#include <utility>

namespace elysia::resources
{
std::expected<void,ResourceFailure> TextureManager::store_texture(
	const std::string& key,TexturePtr texture)
{
	return store_resource(key,TextureResource{
		.texture = std::move(texture)
	});
}

std::expected<void,ResourceFailure> TextureManager::store_animation_texture(
	const std::string& key,
	TexturePtr texture,
	TexturePtr coverage_mask)
{
	if (!coverage_mask)
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,
			"Store animation texture failed: coverage mask is null.",key));

	return store_resource(key,TextureResource{
		.texture = std::move(texture),
		.coverage_mask = std::move(coverage_mask)
	});
}

std::expected<void,ResourceFailure> TextureManager::store_animation_textures(
	std::vector<AnimationTextureResource>&& resources)
{
	std::unordered_set<std::string> keys;
	keys.reserve(resources.size());
	for (const AnimationTextureResource& resource : resources)
	{
		if (resource.key.empty() || !resource.texture
			|| !resource.coverage_mask
			|| _texture_pool.contains(resource.key)
			|| !keys.emplace(resource.key).second)
		{
			return std::unexpected(make_resource_failure(
				ResourceError::InvalidRequest,
				"Store animation texture batch failed validation.",resource.key));
		}
	}

	for (AnimationTextureResource& resource : resources)
	{
		_texture_pool.emplace(
			std::move(resource.key),
			TextureResource{
				.texture = std::move(resource.texture),
				.coverage_mask = std::move(resource.coverage_mask)
			});
	}
	return {};
}

std::expected<void,ResourceFailure> TextureManager::store_resource(
	const std::string& key,
	TextureResource resource)
{
	if (key.empty())
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Store texture failed: key is empty."));

	if (!resource.texture)
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Store texture failed: texture is null.",key));

	if (_texture_pool.contains(key))
		return {};

	_texture_pool.emplace(key, std::move(resource));
	return {};
}

SDL_Texture* TextureManager::find_texture(const std::string_view& key) const
{
	if (key.empty())
	{
		ELYSIA_LOG_WARN("resource","Find texture failed: key is empty.");
		return nullptr;
	}

	TexturePool::const_iterator iterator = _texture_pool.find(std::string(key));
	if (iterator == _texture_pool.end())
	{
		ELYSIA_LOG_WARN("resource","Find texture failed: resource does not exist: "
			<< key);
		return nullptr;
	}

	return iterator->second.texture.get();
}

SDL_Texture* TextureManager::find_coverage_mask(const std::string_view& key) const
{
	if (key.empty())
		return nullptr;

	const TexturePool::const_iterator iterator =
		_texture_pool.find(std::string(key));
	return iterator == _texture_pool.end()
		? nullptr
		: iterator->second.coverage_mask.get();
}

void TextureManager::clear()
{
	_texture_pool.clear();
}

size_t TextureManager::resource_count() const
{
	return _texture_pool.size();
}

}
