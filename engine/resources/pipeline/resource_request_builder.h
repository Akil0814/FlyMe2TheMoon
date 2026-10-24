#pragma once

#include "../../io/loaders/asset_config_types.h"
#include "../resource_types.h"
#include "resource_request_build_failure.h"

#include <expected>
#include <string>
#include <span>
#include <vector>

namespace elysia::resources
{
class ResourceRequestBuilder
{
public:
	[[nodiscard]] std::expected<void,ResourceRequestBuildFailure> append_font_requests(
		const elysia::io::FontManifest&,
		const std::filesystem::path& root,
		std::span<const int> point_sizes,
		std::vector<FontLoadRequest>&) const;
	[[nodiscard]] std::expected<void,ResourceRequestBuildFailure> append_audio_requests(
		const elysia::io::AudioManifest&,const std::filesystem::path& root,
		std::vector<SoundLoadRequest>&,std::vector<MusicLoadRequest>&) const;
	[[nodiscard]] std::expected<void,ResourceRequestBuildFailure> append_texture_manifest_requests(const elysia::io::TextureManifest&, const std::filesystem::path&, std::vector<TextureLoadRequest>&) const;
	[[nodiscard]] std::expected<void,ResourceRequestBuildFailure> append_animation_manifest_requests(const elysia::io::AnimationManifest&, const std::filesystem::path&,
		std::vector<AtlasBuildRequest>&, std::vector<AnimationBuildRequest>&) const;
	[[nodiscard]] std::expected<void,ResourceRequestBuildFailure> append_animation_effect_manifest_requests(const elysia::io::AnimationEffectManifest&,
		std::vector<AnimationEffectBuildRequest>&) const;

	[[nodiscard]] std::expected<void,ResourceRequestBuildFailure> append_entity_animation_requests(
		const std::string& key_namespace,
		const elysia::io::EntityAnimationContentEntry& entry,
		std::vector<AtlasBuildRequest>&,
		std::vector<AnimationBuildRequest>&) const;
	[[nodiscard]] std::expected<void,ResourceRequestBuildFailure> append_entity_effect_requests(
		const std::string& key_namespace,
		const elysia::io::EntityEffectContentEntry& entry,
		const std::vector<AnimationBuildRequest>&,
		std::vector<AnimationEffectBuildRequest>&) const;
	[[nodiscard]] std::expected<void,ResourceRequestBuildFailure> append_entity_texture_requests(
		const std::string& key_namespace,
		const elysia::io::EntityTextureContentEntry& entry,
		std::vector<TextureLoadRequest>&) const;
	[[nodiscard]] std::expected<void,ResourceRequestBuildFailure> append_entity_audio_requests(
		const std::string& key_namespace,
		const elysia::io::EntityAudioContentEntry& entry,
		std::vector<SoundLoadRequest>&) const;
};
}
