#include "engine/audio/mixer_backend.h"
#include "audio_manager.h"
#include "../../tools/logger.h"
namespace elysia::resources
{
AudioManager::~AudioManager()
{
	clear();
}

std::expected<void,ResourceFailure> AudioManager::load_sound(
	const std::string& key,
	const std::filesystem::path& file_path
)
{
	if (key.empty())
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Load sound failed: key is empty."));

	if (file_path.empty())
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Load sound failed: file path is empty.",key));

	MIX_Audio* sound = MIX_LoadAudio(nullptr,file_path.string().c_str(),true);
	if (!sound)
		return std::unexpected(make_resource_failure(
			ResourceError::DecodeFailed,std::string("Load sound failed: ") + SDL_GetError(),
			key,file_path));

	return store_sound(key, sound);
}

std::expected<void,ResourceFailure> AudioManager::load_sound(
	const SoundLoadRequest& request)
{
	auto result = load_sound(request.key,request.file_path);
	if (result) return result;
	return std::unexpected(make_resource_failure(
		result.error().code,result.error().diagnostic.message,"sound",request.key,
		request.file_path,request.origin,result.error().diagnostic.origin));
}

std::expected<void,ResourceFailure> AudioManager::load_sounds(
	const std::vector<SoundLoadRequest>& requests)
{
	for (const SoundLoadRequest& request : requests)
	{
		if (auto result = load_sound(request); !result)
			return result;
	}

	return {};
}

std::expected<void,ResourceFailure> AudioManager::store_sound(
	const std::string& key,MIX_Audio* sound)
{
	if (key.empty())
	{
		if (sound)
			elysia::audio::detail::mixer_backend().destroy_audio(sound);
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Store sound failed: key is empty."));
	}

	if (!sound)
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Store sound failed: sound is null.",key));

	SoundPool::iterator iterator = _sound_pool.find(key);
	if (iterator != _sound_pool.end())
	{
		if (iterator->second)
			elysia::audio::detail::mixer_backend().destroy_audio(iterator->second);

		iterator->second = sound;
		return {};
	}

	_sound_pool.emplace(key, sound);
	return {};
}

MIX_Audio* AudioManager::find_sound(const std::string_view& key) const
{
	if (key.empty())
	{
		ELYSIA_LOG_WARN("resource","Find sound failed: key is empty.");
		return nullptr;
	}

	SoundPool::const_iterator iterator = _sound_pool.find(std::string(key));
	if (iterator == _sound_pool.end())
	{
		ELYSIA_LOG_WARN("resource","Find sound failed: resource does not exist: "
			<< key);
		return nullptr;
	}

	return iterator->second;
}

std::expected<void,ResourceFailure> AudioManager::load_music(
	const std::string& key,
	const std::filesystem::path& file_path
)
{
	if (key.empty())
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Load music failed: key is empty."));

	if (file_path.empty())
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Load music failed: file path is empty.",key));

	MIX_Audio* music = MIX_LoadAudio(nullptr,file_path.string().c_str(),false);
	if (!music)
		return std::unexpected(make_resource_failure(
			ResourceError::DecodeFailed,std::string("Load music failed: ") + SDL_GetError(),
			key,file_path));

	return store_music(key, music);
}

std::expected<void,ResourceFailure> AudioManager::load_music(
	const MusicLoadRequest& request)
{
	auto result = load_music(request.key,request.file_path);
	if (result) return result;
	return std::unexpected(make_resource_failure(
		result.error().code,result.error().diagnostic.message,"music",request.key,
		request.file_path,request.origin,result.error().diagnostic.origin));
}

std::expected<void,ResourceFailure> AudioManager::load_music(
	const std::vector<MusicLoadRequest>& requests)
{
	for (const MusicLoadRequest& request : requests)
	{
		if (auto result = load_music(request); !result)
			return result;
	}

	return {};
}

std::expected<void,ResourceFailure> AudioManager::store_music(
	const std::string& key,MIX_Audio* music)
{
	if (key.empty())
	{
		if (music)
			elysia::audio::detail::mixer_backend().destroy_audio(music);
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Store music failed: key is empty."));
	}

	if (!music)
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Store music failed: music is null.",key));

	MusicPool::iterator iterator = _music_pool.find(key);
	if (iterator != _music_pool.end())
	{
		if (iterator->second)
			elysia::audio::detail::mixer_backend().destroy_audio(iterator->second);

		iterator->second = music;
		return {};
	}

	_music_pool.emplace(key, music);
	return {};
}

MIX_Audio* AudioManager::find_music(const std::string_view& key) const
{
	if (key.empty())
	{
		ELYSIA_LOG_WARN("resource","Find music failed: key is empty.");
		return nullptr;
	}

	MusicPool::const_iterator iterator = _music_pool.find(std::string(key));
	if (iterator == _music_pool.end())
	{
		ELYSIA_LOG_WARN("resource","Find music failed: resource does not exist: "
			<< key);
		return nullptr;
	}

	return iterator->second;
}

void AudioManager::clear()
{
	for (SoundPool::value_type& sound : _sound_pool)
	{
		if (sound.second)
			elysia::audio::detail::mixer_backend().destroy_audio(sound.second);
	}

	for (MusicPool::value_type& music : _music_pool)
	{
		if (music.second)
			elysia::audio::detail::mixer_backend().destroy_audio(music.second);
	}

	_sound_pool.clear();
	_music_pool.clear();
}

size_t AudioManager::resource_count() const
{
	return _sound_pool.size() + _music_pool.size();
}

}
