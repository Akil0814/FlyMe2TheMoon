#pragma once
#include "../atlas/atlas_manager.h"
#include "../audio/audio_manager.h"
#include "../font/font_manager.h"
#include "../texture/texture_manager.h"
#include "../resource_failure.h"

#include "../../tools/singleton.h"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <filesystem>
#include <expected>
#include <string_view>
#include <vector>

namespace elysia::resources
{
class ResourceService;

class ResourceManager : public elysia::tools::Singleton<ResourceManager>
{
	friend elysia::tools::Singleton<ResourceManager>;
	friend class ResourceService;

public:
	[[nodiscard]] std::expected<void,ResourceFailure> begin_atlas_build(
		const AtlasBuildRequest& request);
	[[nodiscard]] std::expected<void,ResourceFailure> begin_atlas_builds(
		const std::vector<AtlasBuildRequest>& requests);
	[[nodiscard]] std::expected<void,ResourceFailure> commit_prepared_atlas_frame(SDL_Renderer* renderer,
		const AtlasFramePreparedResult& result);
	[[nodiscard]] std::expected<void,ResourceFailure> store_texture(
		const std::string& key,TexturePtr texture,
		const ResourceOrigin& origin = {},const std::filesystem::path& path = {});
	[[nodiscard]] std::expected<void,ResourceFailure> load_font(const std::string& key,
		const std::filesystem::path& file_path,int point_size);
	[[nodiscard]] std::expected<void,ResourceFailure> load_font(
		const FontLoadRequest& request);
	[[nodiscard]] std::expected<void,ResourceFailure> load_sound(
		const SoundLoadRequest& request);
	[[nodiscard]] std::expected<void,ResourceFailure> load_sounds(
		const std::vector<SoundLoadRequest>& requests);
	[[nodiscard]] std::expected<void,ResourceFailure> load_music(
		const MusicLoadRequest& request);
	[[nodiscard]] std::expected<void,ResourceFailure> load_music(
		const std::vector<MusicLoadRequest>& requests);

	void clear();
	[[nodiscard]] bool has_in_progress_atlas_builds() const;
	[[nodiscard]] size_t atlas_resource_count() const;
	[[nodiscard]] size_t texture_resource_count() const;
	[[nodiscard]] size_t resource_count() const;

private:
	ResourceManager();

	[[nodiscard]] Atlas* find_atlas(std::string_view key) const;
	[[nodiscard]] bool has_font(std::string_view key) const noexcept;
	[[nodiscard]] TTF_Font* find_font(std::string_view key) const;
	[[nodiscard]] MIX_Audio* find_sound(std::string_view key) const;
	[[nodiscard]] MIX_Audio* find_music(std::string_view key) const;
	[[nodiscard]] SDL_Texture* find_texture(std::string_view key) const;

	TextureManager _texture_manager;
	AtlasManager _atlas_manager;
	FontManager _font_manager;
	AudioManager _audio_manager;
};

}
