#pragma once

#include "../resources/resource_types.h"

#include <cstddef>
#include <vector>

namespace elysia::loading
{
class ResourceLoadPlan
{
public:
	std::vector<elysia::resources::FontLoadRequest>& font_requests()
	{
		return _font_requests;
	}

	const std::vector<elysia::resources::FontLoadRequest>& font_requests() const
	{
		return _font_requests;
	}

	std::vector<elysia::resources::SoundLoadRequest>& sound_requests()
	{
		return _sound_requests;
	}

	const std::vector<elysia::resources::SoundLoadRequest>& sound_requests() const
	{
		return _sound_requests;
	}

	std::vector<elysia::resources::MusicLoadRequest>& music_requests()
	{
		return _music_requests;
	}

	const std::vector<elysia::resources::MusicLoadRequest>& music_requests() const
	{
		return _music_requests;
	}

	std::vector<elysia::resources::TextureLoadRequest>& texture_requests()
	{
		return _texture_requests;
	}

	const std::vector<elysia::resources::TextureLoadRequest>& texture_requests() const
	{
		return _texture_requests;
	}

	std::vector<elysia::resources::AtlasBuildRequest>& atlas_build_requests()
	{
		return _atlas_build_requests;
	}

	const std::vector<elysia::resources::AtlasBuildRequest>& atlas_build_requests() const
	{
		return _atlas_build_requests;
	}

	std::vector<elysia::resources::AnimationBuildRequest>& animation_build_requests()
	{
		return _animation_build_requests;
	}

	const std::vector<elysia::resources::AnimationBuildRequest>& animation_build_requests() const
	{
		return _animation_build_requests;
	}

	std::vector<elysia::resources::AnimationEffectBuildRequest>& animation_effect_build_requests()
	{
		return _animation_effect_build_requests;
	}

	const std::vector<elysia::resources::AnimationEffectBuildRequest>& animation_effect_build_requests() const
	{
		return _animation_effect_build_requests;
	}

	void clear()
	{
		_font_requests.clear();
		_sound_requests.clear();
		_music_requests.clear();
		_texture_requests.clear();
		_atlas_build_requests.clear();
		_animation_build_requests.clear();
		_animation_effect_build_requests.clear();
	}

	[[nodiscard]] bool empty() const
	{
		return _font_requests.empty()
			&& _sound_requests.empty()
			&& _music_requests.empty()
			&& _texture_requests.empty()
			&& _atlas_build_requests.empty()
			&& _animation_build_requests.empty()
			&& _animation_effect_build_requests.empty();
	}

	[[nodiscard]] std::size_t total_request_count() const
	{
		return _font_requests.size()
			+ _sound_requests.size()
			+ _music_requests.size()
			+ _texture_requests.size()
			+ _atlas_build_requests.size()
			+ _animation_build_requests.size()
			+ _animation_effect_build_requests.size();
	}

private:
	std::vector<elysia::resources::FontLoadRequest> _font_requests;
	std::vector<elysia::resources::SoundLoadRequest> _sound_requests;
	std::vector<elysia::resources::MusicLoadRequest> _music_requests;
	std::vector<elysia::resources::TextureLoadRequest> _texture_requests;
	std::vector<elysia::resources::AtlasBuildRequest> _atlas_build_requests;
	std::vector<elysia::resources::AnimationBuildRequest> _animation_build_requests;
	std::vector<elysia::resources::AnimationEffectBuildRequest> _animation_effect_build_requests;
};

}
