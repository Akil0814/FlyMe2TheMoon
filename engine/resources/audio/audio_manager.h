#pragma once
#include "../resource_sub_manager.h"
#include "../resource_failure.h"
#include "../resource_types.h"

#include <SDL3_mixer/SDL_mixer.h>

#include <filesystem>
#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace elysia::resources
{
using SoundPool = std::unordered_map<std::string, MIX_Audio*>;
using MusicPool = std::unordered_map<std::string, MIX_Audio*>;

class AudioManager : public ResourceSubManager
{
public:
	~AudioManager() override;

	[[nodiscard]] std::expected<void,ResourceFailure> load_sound(
		const std::string& key,
		const std::filesystem::path& file_path
	);
	[[nodiscard]] std::expected<void,ResourceFailure> load_sound(
		const SoundLoadRequest& request);
	[[nodiscard]] std::expected<void,ResourceFailure> load_sounds(
		const std::vector<SoundLoadRequest>& requests);
	[[nodiscard]] std::expected<void,ResourceFailure> store_sound(
		const std::string& key,MIX_Audio* sound);
	MIX_Audio* find_sound(const std::string_view& key) const;

	[[nodiscard]] std::expected<void,ResourceFailure> load_music(
		const std::string& key,
		const std::filesystem::path& file_path
	);
	[[nodiscard]] std::expected<void,ResourceFailure> load_music(
		const MusicLoadRequest& request);
	[[nodiscard]] std::expected<void,ResourceFailure> load_music(
		const std::vector<MusicLoadRequest>& requests);
	[[nodiscard]] std::expected<void,ResourceFailure> store_music(
		const std::string& key,MIX_Audio* music);
	MIX_Audio* find_music(const std::string_view& key) const;

	void clear() override;
	size_t resource_count() const override;

private:
	SoundPool _sound_pool;
	MusicPool _music_pool;
};

}
