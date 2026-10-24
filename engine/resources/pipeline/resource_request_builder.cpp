#include "resource_request_builder.h"

#include "filesystem_segment_formatter.h"
#include "resource_key_builder.h"

#include <algorithm>
#include <system_error>
#include <iomanip>
#include <sstream>
#include <utility>

namespace elysia::resources
{
namespace
{
bool missing_path_error(const std::error_code& error)
{
	return error == std::errc::no_such_file_or_directory;
}

ResourceRequestBuildFailure key_error(
	const char* operation,const elysia::core::KeyValidationFailure& error,
	const ResourceOrigin& origin = {})
{
	return make_request_build_failure(
		ResourceRequestBuildError::InvalidDeclaration,
		std::string(operation) + ": " + error.message,"resource-key",error.value,{},origin,
		error.origin);
}

void append_texture_request(
	std::string key,
	std::filesystem::path file_path,
	ResourceOrigin origin,
	std::vector<TextureLoadRequest>& requests)
{
	requests.push_back({std::move(key), std::move(file_path), std::move(origin)});
}

void replace_all(std::string& value, std::string_view marker, std::string_view replacement)
{
	size_t position = 0;
	while ((position = value.find(marker, position)) != std::string::npos)
	{
		value.replace(position, marker.size(), replacement);
		position += replacement.size();
	}
}

std::optional<std::string> make_frame_prefix(
	const std::string& pattern,
	const elysia::io::EntityAnimationContentEntry& entry,
	const elysia::io::AnimationClipConfig& clip)
{
	std::string prefix = pattern;
	replace_all(prefix, "{id}", entry.entity.id);
	replace_all(prefix, "{animation}", clip.animation_name);
	std::string suffix;
	if (clip.is_segment)
	{
		std::string segment;
		if (!format_filesystem_segment(clip.segment_index, segment)) return std::nullopt;
		suffix = "_" + segment;
	}
	replace_all(prefix, "{segment_suffix}", suffix);
	if (prefix.empty() || prefix.find('{') != std::string::npos
		|| prefix.find('}') != std::string::npos)
		return std::nullopt;
	return prefix;
}

std::optional<size_t> clip_segment(const elysia::io::AnimationClipConfig& clip)
{
	return clip.is_segment ? std::optional<size_t>(clip.segment_index) : std::nullopt;
}

std::optional<size_t> effect_segment(const elysia::io::EffectDefinitionConfigEntry& effect)
{
	return effect.is_segment ? std::optional<size_t>(effect.segment_index) : std::nullopt;
}
}

std::expected<void,ResourceRequestBuildFailure> ResourceRequestBuilder::append_font_requests(
	const elysia::io::FontManifest& manifest,
	const std::filesystem::path& root,
	std::span<const int> point_sizes,
	std::vector<FontLoadRequest>& requests) const
{
	if (root.empty())
		return std::unexpected(make_request_build_failure(
			ResourceRequestBuildError::InvalidDeclaration,
			"Build font requests failed: font root is empty.","font"));
	for (const auto& entry : manifest.fonts)
	{
		if (auto key_result = ResourceKeyBuilder::validate_key(entry.key); !key_result)
			return std::unexpected(key_error("Build font requests failed",key_result.error(),entry.origin));
		for (const int size : point_sizes)
		{
			if (size <= 0) return std::unexpected(make_request_build_failure(
				ResourceRequestBuildError::InvalidDeclaration,
				"Build font requests failed: point size must be positive.","font",entry.key,
				(root / entry.file_path).lexically_normal(),entry.origin));
			auto key_result = ResourceKeyBuilder::append_component(entry.key,std::to_string(size));
			if (!key_result)
				return std::unexpected(key_error("Build font requests failed",key_result.error(),entry.origin));
			auto origin = entry.origin;
			origin.logical_name = *key_result;
			requests.push_back({std::move(*key_result), (root / entry.file_path).lexically_normal(), size, std::move(origin)});
		}
	}
	return {};
}

std::expected<void,ResourceRequestBuildFailure> ResourceRequestBuilder::append_audio_requests(
	const elysia::io::AudioManifest& manifest,
	const std::filesystem::path& root,
	std::vector<SoundLoadRequest>& sounds,
	std::vector<MusicLoadRequest>& music) const
{
	if (root.empty())
		return std::unexpected(make_request_build_failure(
			ResourceRequestBuildError::InvalidDeclaration,
			"Build audio requests failed: audio root is empty.","audio"));
	for (const auto& entry : manifest.sounds)
	{
		if (auto key_result = ResourceKeyBuilder::validate_key(entry.key); !key_result)
			return std::unexpected(key_error("Build sound requests failed",key_result.error(),entry.origin));
		sounds.push_back({entry.key, (root / entry.file_path).lexically_normal(), entry.origin});
	}
	for (const auto& entry : manifest.music)
	{
		if (auto key_result = ResourceKeyBuilder::validate_key(entry.key); !key_result)
			return std::unexpected(key_error("Build music requests failed",key_result.error(),entry.origin));
		music.push_back({entry.key, (root / entry.file_path).lexically_normal(), entry.origin});
	}
	return {};
}

std::expected<void,ResourceRequestBuildFailure> ResourceRequestBuilder::append_texture_manifest_requests(
	const elysia::io::TextureManifest& manifest,
	const std::filesystem::path& root,
	std::vector<TextureLoadRequest>& requests) const
{
	if (root.empty())
	{
		return std::unexpected(make_request_build_failure(
			ResourceRequestBuildError::InvalidDeclaration,
			"Build texture requests failed: texture root is empty.","texture"));
	}
	for (const auto& entry : manifest.textures)
	{
		if (auto key_result = ResourceKeyBuilder::validate_key(entry.key); !key_result)
			return std::unexpected(key_error("Build texture requests failed",key_result.error(),entry.origin));
		append_texture_request(entry.key, (root / entry.file_path).lexically_normal(), entry.origin, requests);
	}
	return {};
}

std::expected<void,ResourceRequestBuildFailure> ResourceRequestBuilder::append_animation_manifest_requests(
	const elysia::io::AnimationManifest& manifest,
	const std::filesystem::path& root,
	std::vector<AtlasBuildRequest>& atlases,
	std::vector<AnimationBuildRequest>& animations) const
{
	for (const auto& entry : manifest.animations)
	{
		if (auto key_result = ResourceKeyBuilder::validate_key(entry.key); !key_result)
			return std::unexpected(key_error("Build animation requests failed",key_result.error(),entry.origin));
		const auto source = (root / entry.source_path).lexically_normal();
		if (entry.frame_count == 0 || entry.fps <= 0.0)
		{
			return std::unexpected(make_request_build_failure(
				ResourceRequestBuildError::InvalidDeclaration,
				"Build animation requests failed: frame count and fps must be positive.",
				"animation",entry.key,source,entry.origin));
		}
		AtlasBuildRequest atlas;
		atlas.atlas_key = entry.key;
		atlas.source_path = source;
		atlas.frame_count = entry.frame_count;
		atlas.frame_filename_prefix = entry.frame_prefix;
		atlas.source_type = entry.horizontal_strip ? AtlasSourceType::HorizontalStrip : AtlasSourceType::FrameDirectory;
		atlas.origin = entry.origin;
		AnimationBuildRequest animation;
		animation.animation_key = entry.key;
		animation.atlas_key = entry.key;
		animation.fps = entry.fps;
		animation.loop = entry.loop;
		animation.origin = entry.origin;
		atlases.push_back(std::move(atlas));
		animations.push_back(std::move(animation));
	}
	return {};
}

std::expected<void,ResourceRequestBuildFailure> ResourceRequestBuilder::append_animation_effect_manifest_requests(
	const elysia::io::AnimationEffectManifest& manifest,
	std::vector<AnimationEffectBuildRequest>& requests) const
{
	for (const auto& entry : manifest.effects)
	{
		auto effect_key = ResourceKeyBuilder::validate_key(entry.key);
		auto animation_key = ResourceKeyBuilder::validate_key(entry.animation_key);
		if (!effect_key || !animation_key)
			return std::unexpected(key_error("Build effect requests failed",
				!effect_key ? effect_key.error() : animation_key.error(),entry.origin));
		requests.push_back({entry.key, entry.animation_key,
			elysia::core::Vector2(entry.default_width, entry.default_height),
			entry.default_angle_degrees, entry.origin});
	}
	return {};
}

std::expected<void,ResourceRequestBuildFailure> ResourceRequestBuilder::append_entity_animation_requests(
	const std::string& key_namespace,
	const elysia::io::EntityAnimationContentEntry& entry,
	std::vector<AtlasBuildRequest>& atlases,
	std::vector<AnimationBuildRequest>& animations) const
{
	for (const auto& clip : entry.animation_config.clips)
	{
		auto key_result = ResourceKeyBuilder::build(
			entry.entity.id,key_namespace,{clip.animation_name},clip_segment(clip));
		if (!key_result)
			return std::unexpected(key_error(
				"Build entity animation requests failed",key_result.error(),clip.origin));
		std::string key = std::move(*key_result);
		if (clip.frame_count == 0 || clip.fps <= 0.0)
			return std::unexpected(make_request_build_failure(
				ResourceRequestBuildError::InvalidDeclaration,
				"Build entity animation requests failed: frame count and fps must be positive.",
				"animation",key,{},clip.origin));
		AtlasBuildRequest atlas;
		atlas.atlas_key = key;
		atlas.frame_count = clip.frame_count;
		atlas.origin = clip.origin;
		const auto resolved = (entry.texture_root / clip.path).lexically_normal();
		if (entry.animation_config.source_type == elysia::io::AnimationSourceType::HorizontalStrip)
		{
			atlas.source_type = AtlasSourceType::HorizontalStrip;
			atlas.source_path = (resolved / (clip.animation_name + ".png")).lexically_normal();
		}
		else
		{
			atlas.source_type = AtlasSourceType::FrameDirectory;
			atlas.source_path = resolved;
			auto frame_prefix = make_frame_prefix(entry.frame_prefix_template,entry,clip);
			if (!frame_prefix)
			{
				return std::unexpected(make_request_build_failure(
					ResourceRequestBuildError::InvalidDeclaration,
					"Build entity animation requests failed: frame prefix is invalid.",
					"animation",key,atlas.source_path,clip.origin));
			}
			atlas.frame_filename_prefix = std::move(*frame_prefix);
		}
		AnimationBuildRequest animation;
		animation.animation_key = key;
		animation.atlas_key = key;
		animation.fps = clip.fps;
		animation.loop = clip.loop;
		animation.segment_index = clip.segment_index;
		animation.origin = clip.origin;
		atlases.push_back(std::move(atlas));
		animations.push_back(std::move(animation));
	}
	return {};
}

std::expected<void,ResourceRequestBuildFailure> ResourceRequestBuilder::append_entity_effect_requests(
	const std::string& key_namespace,
	const elysia::io::EntityEffectContentEntry& entry,
	const std::vector<AnimationBuildRequest>& animations,
	std::vector<AnimationEffectBuildRequest>& effects) const
{
	for (const auto& definition : entry.effect_config.effects)
	{
		auto effect_key_result = ResourceKeyBuilder::build(
			entry.entity.id,key_namespace,{definition.effect_name},effect_segment(definition));
		if (!effect_key_result)
			return std::unexpected(key_error(
				"Build entity effect requests failed",effect_key_result.error(),definition.origin));
		auto animation_key_result = ResourceKeyBuilder::build(
			entry.entity.id,key_namespace,{definition.animation_name},effect_segment(definition));
		if (!animation_key_result)
			return std::unexpected(key_error(
				"Build entity effect requests failed",animation_key_result.error(),definition.origin));
		std::string effect_key = std::move(*effect_key_result);
		std::string animation_key = std::move(*animation_key_result);
		const bool exists = std::any_of(animations.begin(), animations.end(),
			[&animation_key](const auto& request) { return request.animation_key == animation_key; });
		if (!exists)
		{
			return std::unexpected(make_request_build_failure(
				ResourceRequestBuildError::InvalidDeclaration,
				"Build entity effect requests failed: referenced animation does not exist.",
				"effect",effect_key,{},definition.origin));
		}
		effects.push_back({std::move(effect_key), std::move(animation_key),
			elysia::core::Vector2(definition.default_width, definition.default_height),
			definition.default_angle_degrees, definition.origin});
	}
	return {};
}

std::expected<void,ResourceRequestBuildFailure> ResourceRequestBuilder::append_entity_texture_requests(
	const std::string& key_namespace,
	const elysia::io::EntityTextureContentEntry& content,
	std::vector<TextureLoadRequest>& requests) const
{
	std::vector<elysia::core::FailureDiagnosticEntry> missing;
	for (const auto& entry : content.layout.textures)
	{
		auto base_key_result = ResourceKeyBuilder::build(
			content.entity.id,key_namespace,{entry.key},std::nullopt);
		if (!base_key_result)
			return std::unexpected(key_error(
				"Build entity texture requests failed",base_key_result.error(),entry.origin));
		std::string base_key = std::move(*base_key_result);
		const auto resolved = (content.texture_root / entry.path).lexically_normal();
		std::error_code filesystem_error;
		const bool is_file = std::filesystem::is_regular_file(resolved,filesystem_error);
		if (missing_path_error(filesystem_error)) filesystem_error.clear();
		if (filesystem_error)
			return std::unexpected(make_request_build_failure(
				ResourceRequestBuildError::FilesystemAccess,
				"Build entity texture requests failed: " + filesystem_error.message(),
				"texture",base_key,resolved,entry.origin));
		if (is_file)
		{
			append_texture_request(base_key, resolved, entry.origin, requests);
			continue;
		}
		const bool is_directory = std::filesystem::is_directory(resolved,filesystem_error);
		if (missing_path_error(filesystem_error)) filesystem_error.clear();
		if (filesystem_error)
			return std::unexpected(make_request_build_failure(
				ResourceRequestBuildError::FilesystemAccess,
				"Build entity texture requests failed: " + filesystem_error.message(),
				"texture",base_key,resolved,entry.origin));
		if (!is_directory)
		{
			missing.push_back(elysia::core::make_failure_diagnostic_entry(
				"texture",base_key,resolved,entry.origin.config_path,
				entry.origin.json_pointer,"file or directory does not exist"));
			continue;
		}
		std::vector<std::filesystem::path> files;
		std::filesystem::directory_iterator iterator(resolved,filesystem_error),end;
		if (filesystem_error)
			return std::unexpected(make_request_build_failure(
				ResourceRequestBuildError::FilesystemAccess,
				"Build entity texture requests failed: " + filesystem_error.message(),
				"texture",base_key,resolved,entry.origin));
		for (; iterator != end; iterator.increment(filesystem_error))
		{
			if (filesystem_error) break;
			const bool regular = iterator->is_regular_file(filesystem_error);
			if (filesystem_error) break;
			if (regular) files.push_back(iterator->path());
		}
		if (filesystem_error)
			return std::unexpected(make_request_build_failure(
				ResourceRequestBuildError::FilesystemAccess,
				"Build entity texture requests failed: " + filesystem_error.message(),
				"texture",base_key,resolved,entry.origin));
		std::sort(files.begin(), files.end());
		if (files.empty())
		{
			return std::unexpected(make_request_build_failure(
				ResourceRequestBuildError::InvalidDeclaration,
				"Build entity texture requests failed: texture directory is empty.",
				"texture",base_key,resolved,entry.origin));
		}
		for (const auto& file : files)
		{
			const std::string stem = file.stem().string();
			auto key_result = ResourceKeyBuilder::append_component(base_key,stem);
			if (!key_result)
				return std::unexpected(key_error(
					"Build entity texture requests failed",key_result.error(),entry.origin));
			auto origin = entry.origin;
			origin.json_pointer += "/files/" + file.filename().generic_string();
			origin.logical_name = entry.key + "." + stem;
			append_texture_request(std::move(*key_result), file, std::move(origin), requests);
		}
	}
	if (!missing.empty())
		return std::unexpected(ResourceRequestBuildFailure{
			ResourceRequestBuildError::MissingSource,
			elysia::core::make_failure_diagnostic(
				"One or more entity texture sources are missing.",std::move(missing))});
	return {};
}

std::expected<void,ResourceRequestBuildFailure> ResourceRequestBuilder::append_entity_audio_requests(
	const std::string& key_namespace,
	const elysia::io::EntityAudioContentEntry& content,
	std::vector<SoundLoadRequest>& requests) const
{
	for (const auto& entry : content.layout.sounds)
	{
		auto key_result = ResourceKeyBuilder::build(
			content.entity.id,key_namespace,{entry.key},std::nullopt);
		if (!key_result)
			return std::unexpected(key_error(
				"Build entity audio requests failed",key_result.error(),entry.origin));
		const auto path = (content.audio_root / entry.path).lexically_normal();
		requests.push_back({std::move(*key_result), path, entry.origin});
	}
	return {};
}
}
