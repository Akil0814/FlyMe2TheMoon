#pragma once
#include "atlas.h"
#include "atlas_build_preparer.h"
#include "../resource_sub_manager.h"
#include "../resource_types.h"
#include "../texture/texture_manager.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace elysia::resources
{
using AtlasPool = std::unordered_map<std::string, std::unique_ptr<Atlas>>;

class AtlasManager : public ResourceSubManager
{
public:
	explicit AtlasManager(TextureManager& texture_manager);

	[[nodiscard]] std::expected<void,ResourceFailure> begin_build(
		const AtlasBuildRequest& request);
	[[nodiscard]] std::expected<void,ResourceFailure> commit_prepared_frame(SDL_Renderer* renderer,
		const AtlasFramePreparedResult& prepared_result);

	Atlas* find_atlas(const std::string_view& key) const;
	bool has_in_progress_build(const std::string_view& key) const;
	size_t in_progress_build_count() const;

	void clear() override;
	size_t resource_count() const override;

private:
	struct AtlasAssemblyFrame
	{
		std::filesystem::path frame_path;
		SDL_Texture* texture = nullptr;
		SDL_Texture* coverage_mask = nullptr;
		bool committed = false;
		std::optional<elysia::core::Rect> source_rect;
	};

	struct AtlasAssemblyState
	{
		AtlasBuildRequest request;
		std::vector<AtlasAssemblyFrame> committed_frames;
		size_t committed_frame_count = 0;
		bool finalized = false;
		std::vector<AnimationTextureResource> pending_textures;
	};

	[[nodiscard]] std::expected<void,ResourceFailure> finalize_build(
		const std::string& atlas_key);
	std::string make_texture_key(
		const std::string& atlas_key,
		size_t frame_index
	) const;
	std::string make_strip_texture_key(const std::string& atlas_key) const;

private:
	TextureManager& _texture_manager;
	AtlasPool _atlas_pool;
	std::unordered_map<std::string, AtlasAssemblyState> _assembly_states;
};

}
