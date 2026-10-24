#include "../../tools/logger.h"
#include "atlas_manager.h"

#include "atlas_builder.h"
#include "../texture/texture_loader.h"
#include "../texture/texture_manager.h"
#include <utility>

namespace elysia::resources
{
namespace
{
std::expected<TexturePtr,ResourceFailure> create_coverage_mask_texture(
	const TextureLoader& texture_loader,
	SDL_Renderer* renderer,
	const AtlasFramePreparedResult& prepared_result)
{
	if (!prepared_result.coverage_mask_surface)
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidBuildState,
			"Create atlas coverage mask failed: prepared surface is null.",
			"atlas-frame",prepared_result.task.atlas_key,
			prepared_result.task.frame_path,prepared_result.task.origin));

	TexturePtr texture = texture_loader.create_texture(
		renderer,
		*prepared_result.coverage_mask_surface);
	if (!texture
		|| !SDL_SetTextureBlendMode(texture.get(),SDL_BLENDMODE_BLEND))
	{
		return std::unexpected(make_resource_failure(
			ResourceError::CreateFailed,"Create atlas coverage mask texture failed.",
			"atlas-frame",prepared_result.task.atlas_key,
			prepared_result.task.frame_path,prepared_result.task.origin));
	}
	return texture;
}
}

AtlasManager::AtlasManager(TextureManager& texture_manager)
	: _texture_manager(texture_manager)
{
}

std::expected<void,ResourceFailure> AtlasManager::begin_build(
	const AtlasBuildRequest& request)
{
	if (!request.is_valid())
	{
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Begin atlas build failed: request is invalid.",
			"atlas",request.atlas_key,request.source_path,request.origin));
	}

	if (_atlas_pool.contains(request.atlas_key))
	{
		return std::unexpected(make_resource_failure(
			ResourceError::DuplicateResource,"Begin atlas build failed: atlas already exists.",
			"atlas",request.atlas_key,request.source_path,request.origin));
	}

	if (_assembly_states.contains(request.atlas_key))
	{
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidBuildState,"Begin atlas build failed: build is already in progress.",
			"atlas",request.atlas_key,request.source_path,request.origin));
	}

	AtlasAssemblyState state;
	state.request = request;
	state.committed_frames.resize(request.frame_count);
	state.committed_frame_count = 0;
	state.finalized = false;
	_assembly_states.emplace(request.atlas_key, std::move(state));
	return {};
}

std::expected<void,ResourceFailure> AtlasManager::commit_prepared_frame(
	SDL_Renderer* renderer,
	const AtlasFramePreparedResult& prepared_result
)
{
	const auto fail = [&](ResourceError code,std::string message,
		std::filesystem::path path = {}) -> std::expected<void,ResourceFailure>
	{
		return std::unexpected(make_resource_failure(
			code,std::move(message),"atlas-frame",prepared_result.task.atlas_key,
			path.empty() ? prepared_result.task.frame_path : std::move(path),
			prepared_result.task.origin));
	};
	if (!renderer)
		return fail(ResourceError::InvalidRequest,
			"Commit atlas frame failed: renderer is null.");

	if (prepared_result.task.atlas_key.empty())
	{
		return fail(ResourceError::InvalidRequest,
			"Commit atlas frame failed: atlas key is empty.");
	}

	std::unordered_map<std::string, AtlasAssemblyState>::iterator iterator =
		_assembly_states.find(prepared_result.task.atlas_key);
	if (iterator == _assembly_states.end())
	{
		return fail(ResourceError::MissingBuildState,
			"Commit atlas frame failed: build state does not exist.");
	}

	AtlasAssemblyState& state = iterator->second;
	if (state.finalized)
	{
		return fail(ResourceError::InvalidBuildState,
			"Commit atlas frame failed: build is already finalized.");
	}

	if (prepared_result.task.expected_frame_count != state.request.frame_count)
	{
		return fail(ResourceError::InvalidBuildState,
			"Commit atlas frame failed: frame count mismatch.");
	}
	if (prepared_result.task.source_type != state.request.source_type)
	{
		return fail(ResourceError::InvalidBuildState,
			"Commit atlas frame failed: source type mismatch.");
	}

	if (prepared_result.task.frame_index >= state.committed_frames.size())
	{
		return fail(ResourceError::InvalidBuildState,
			"Commit atlas frame failed: frame index is out of range.");
	}

	const SurfaceLoadResult& surface_result = prepared_result.surface_result;
	if (!surface_result._surface)
	{
		return fail(ResourceError::InvalidBuildState,
			"Commit atlas frame failed: decoded surface is null.");
	}

	if (surface_result._asset_key != prepared_result.task.atlas_key)
	{
		return fail(ResourceError::InvalidBuildState,
			"Commit atlas frame failed: decoded asset key mismatch.");
	}

	if (surface_result._frame_index != prepared_result.task.frame_index)
	{
		return fail(ResourceError::InvalidBuildState,
			"Commit atlas frame failed: decoded frame index mismatch.");
	}

	if (state.request.source_type == AtlasSourceType::HorizontalStrip)
	{
		if (prepared_result.task.frame_index != 0)
		{
			return fail(ResourceError::InvalidBuildState,
				"Commit atlas strip failed: frame index must be zero.");
		}

		const int texture_width = surface_result._surface->w;
		const int texture_height = surface_result._surface->h;
		if (texture_width <= 0 || texture_height <= 0
			|| state.request.frame_count > static_cast<size_t>(texture_width)
			|| static_cast<size_t>(texture_width) % state.request.frame_count != 0)
		{
			return fail(ResourceError::InvalidBuildState,
				"Commit atlas strip failed: dimensions do not match frame count.");
		}

		const int frame_width = static_cast<int>(
			static_cast<size_t>(texture_width) / state.request.frame_count
		);
		if (frame_width <= 0)
		{
			return fail(ResourceError::InvalidBuildState,
				"Commit atlas strip failed: frame width is invalid.");
		}

		TextureLoader texture_loader;
		auto texture_result = texture_loader.load_texture(renderer, surface_result);
		if (!texture_result) return std::unexpected(std::move(texture_result.error()));
		if (!texture_result->_texture) return fail(ResourceError::CreateFailed,
			"Commit atlas strip failed: texture is null.");
		auto coverage_mask_result =
			create_coverage_mask_texture(texture_loader,renderer,prepared_result);
		if (!coverage_mask_result) return std::unexpected(std::move(coverage_mask_result.error()));
		TexturePtr coverage_mask = std::move(*coverage_mask_result);

		const std::string texture_key =
			make_strip_texture_key(prepared_result.task.atlas_key);
		SDL_Texture* shared_texture = texture_result->_texture.get();
		SDL_Texture* shared_coverage_mask = coverage_mask.get();
		state.pending_textures.push_back(AnimationTextureResource{
			.key = texture_key,
			.texture = std::move(texture_result->_texture),
			.coverage_mask = std::move(coverage_mask)
		});

		for (size_t index = 0; index < state.committed_frames.size(); ++index)
		{
			AtlasAssemblyFrame& strip_frame = state.committed_frames[index];
			strip_frame.frame_path = surface_result._frame_path;
			strip_frame.texture = shared_texture;
			strip_frame.coverage_mask = shared_coverage_mask;
			strip_frame.source_rect = elysia::core::Rect(
				static_cast<float>(index * static_cast<size_t>(frame_width)),
				0.0f,
				static_cast<float>(frame_width),
				static_cast<float>(texture_height)
			);
			strip_frame.committed = true;
		}
		state.committed_frame_count = state.request.frame_count;
		return finalize_build(state.request.atlas_key);
	}

	AtlasAssemblyFrame& frame_state =
		state.committed_frames[prepared_result.task.frame_index];
	if (frame_state.committed)
		return fail(ResourceError::InvalidBuildState,
			"Commit atlas frame failed: frame was already committed.");

	TextureLoader texture_loader;
	auto texture_result =
		texture_loader.load_texture(renderer, surface_result);
	if (!texture_result) return std::unexpected(std::move(texture_result.error()));
	if (!texture_result->_texture) return fail(ResourceError::CreateFailed,
		"Commit atlas frame failed: texture is null.");
	auto coverage_mask_result =
		create_coverage_mask_texture(texture_loader,renderer,prepared_result);
	if (!coverage_mask_result) return std::unexpected(std::move(coverage_mask_result.error()));
	TexturePtr coverage_mask = std::move(*coverage_mask_result);

	const std::string texture_key = make_texture_key(
		prepared_result.task.atlas_key,
		prepared_result.task.frame_index
	);
	frame_state.frame_path = surface_result._frame_path;
	frame_state.texture = texture_result->_texture.get();
	frame_state.coverage_mask = coverage_mask.get();
	frame_state.committed = true;
	state.pending_textures.push_back(AnimationTextureResource{
		.key = texture_key,
		.texture = std::move(texture_result->_texture),
		.coverage_mask = std::move(coverage_mask)
	});
	++state.committed_frame_count;

	if (!frame_state.texture || !frame_state.coverage_mask)
	{
		return fail(ResourceError::InvalidBuildState,
			"Commit atlas frame failed: committed textures are null.");
	}

	if (state.committed_frame_count == state.request.frame_count)
		return finalize_build(state.request.atlas_key);

	return {};
}

Atlas* AtlasManager::find_atlas(const std::string_view& key) const
{
	if (key.empty())
	{
		ELYSIA_LOG_WARN("resource","Find atlas failed: key is empty.");
		return nullptr;
	}

	AtlasPool::const_iterator iterator = _atlas_pool.find(std::string(key));
	if (iterator == _atlas_pool.end())
	{
		ELYSIA_LOG_WARN("resource","Find atlas failed: resource does not exist: "
			<< key);
		return nullptr;
	}

	return iterator->second.get();
}

bool AtlasManager::has_in_progress_build(const std::string_view& key) const
{
	return _assembly_states.contains(std::string(key));
}

size_t AtlasManager::in_progress_build_count() const
{
	return _assembly_states.size();
}

void AtlasManager::clear()
{
	_assembly_states.clear();
	_atlas_pool.clear();
}

size_t AtlasManager::resource_count() const
{
	return _atlas_pool.size();
}

std::expected<void,ResourceFailure> AtlasManager::finalize_build(
	const std::string& atlas_key)
{
	std::unordered_map<std::string, AtlasAssemblyState>::iterator iterator =
		_assembly_states.find(atlas_key);
	if (iterator == _assembly_states.end())
		return std::unexpected(make_resource_failure(
			ResourceError::MissingBuildState,
			"Finalize atlas build failed: build state does not exist.",
			"atlas",atlas_key,{},{}));

	AtlasAssemblyState& state = iterator->second;
	const auto fail = [&](ResourceError code,std::string message,
		std::filesystem::path path = {}) -> std::expected<void,ResourceFailure>
	{
		return std::unexpected(make_resource_failure(
			code,std::move(message),path.empty() ? "atlas" : "atlas-frame",
			atlas_key,path.empty() ? state.request.source_path : std::move(path),
			state.request.origin));
	};
	if (state.finalized)
		return fail(ResourceError::InvalidBuildState,
			"Finalize atlas build failed: build is already finalized.");

	if (state.committed_frame_count != state.request.frame_count)
	{
		return fail(ResourceError::InvalidBuildState,
			"Finalize atlas build failed: committed frame count mismatch.");
	}

	std::vector<AtlasCommittedFrame> committed_frames;
	committed_frames.reserve(state.committed_frames.size());
	for (size_t index = 0; index < state.committed_frames.size(); ++index)
	{
		const AtlasAssemblyFrame& frame_state = state.committed_frames[index];
		if (!frame_state.committed || !frame_state.texture)
		{
			return fail(ResourceError::InvalidBuildState,
				"Finalize atlas build failed: frame texture is incomplete.",
				frame_state.frame_path);
		}
		if (!frame_state.coverage_mask)
		{
			return fail(ResourceError::InvalidBuildState,
				"Finalize atlas build failed: coverage mask is incomplete.",
				frame_state.frame_path);
		}

		AtlasCommittedFrame committed_frame;
		committed_frame.frame_path = frame_state.frame_path;
		committed_frame.texture = frame_state.texture;
		committed_frame.coverage_mask = frame_state.coverage_mask;
		committed_frame.frame_index = index;
		committed_frame.source_rect = frame_state.source_rect;
		committed_frames.push_back(std::move(committed_frame));
	}

	std::unique_ptr<Atlas> atlas = std::make_unique<Atlas>(atlas_key);
	AtlasBuilder atlas_builder;
	if (auto built = atlas_builder.build_atlas(
		state.request,committed_frames,*atlas); !built)
		return std::unexpected(std::move(built.error()));
	if (auto stored = _texture_manager.store_animation_textures(
		std::move(state.pending_textures)); !stored)
		return std::unexpected(make_resource_failure(
			stored.error().code,stored.error().diagnostic.message,
			"atlas",atlas_key,state.request.source_path,state.request.origin,
			stored.error().diagnostic.origin));

	state.finalized = true;
	_atlas_pool.emplace(atlas_key, std::move(atlas));
	_assembly_states.erase(iterator);
	return {};
}

std::string AtlasManager::make_texture_key(
	const std::string& atlas_key,
	size_t frame_index
) const
{
	return atlas_key + "#" + std::to_string(frame_index);
}

std::string AtlasManager::make_strip_texture_key(const std::string& atlas_key) const
{
	return atlas_key + "#strip";
}

}
