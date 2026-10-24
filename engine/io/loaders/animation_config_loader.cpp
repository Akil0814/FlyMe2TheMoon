#include "../../resources/pipeline/filesystem_segment_formatter.h"
#include "../../resources/pipeline/resource_key_builder.h"
#include "animation_config_loader.h"

#include <string>
#include <optional>
#include <utility>

namespace elysia::io
{
namespace
{
std::optional<std::string> unknown_field(
	const json& node,std::initializer_list<const char*> fields)
{
	for (auto item = node.begin(); item != node.end(); ++item)
	{
		bool known = false;
		for (const char* field : fields) known = known || item.key() == field;
		if (!known) return item.key();
	}
	return std::nullopt;
}

ManifestLoadFailure animation_failure(
	const std::filesystem::path& path,ManifestLoadError code,std::string message,
	std::string key = {},std::string pointer = {},
	std::source_location origin = std::source_location::current())
{
	return make_manifest_load_failure(
		code,std::move(message),"animation-config",std::move(key),path,path,
		std::move(pointer),origin);
}

std::string escape_json_pointer(std::string value)
{
	size_t position = 0;
	while ((position = value.find('~', position)) != std::string::npos) { value.replace(position, 1, "~0"); position += 2; }
	position = 0;
	while ((position = value.find('/', position)) != std::string::npos) { value.replace(position, 1, "~1"); position += 2; }
	return value;
}
}

std::expected<AnimationConfig,ManifestLoadFailure> AnimationConfigLoader::load(
	const std::filesystem::path& animation_config_path,
	const AnimationLayout& layout) const
{
	if (auto source = validate_manifest_source(
		animation_config_path,"animation-config"); !source)
		return std::unexpected(std::move(source.error()));
	JsonLoader loader;
	const auto result = loader.open_file(animation_config_path);
	if (!result)
		return std::unexpected(manifest_failure_from_json(
			result.error(),"animation-config","Load animation config failed: "));
	if (!loader.root().is_object())
		return std::unexpected(animation_failure(
			animation_config_path,ManifestLoadError::InvalidDocument,
			"Load animation config failed: root is not an object."));
	const json& root = loader.root();
	if (const auto field = unknown_field(root,{"defaults","animations"}))
		return std::unexpected(animation_failure(
			animation_config_path,ManifestLoadError::UnknownField,
			"Load animation config failed: unknown root field: " + *field,
			{},"/" + *field));
	if (root.size() != 2
		|| !root.contains("defaults") || !root.at("defaults").is_object()
		|| !root.contains("animations") || !root.at("animations").is_object())
		return std::unexpected(animation_failure(
			animation_config_path,ManifestLoadError::InvalidSchema,
			"Load animation config failed: defaults and animations are required."));
	const json& defaults = root.at("defaults");
	if (const auto field = unknown_field(defaults,{"source_type"}))
		return std::unexpected(animation_failure(
			animation_config_path,ManifestLoadError::UnknownField,
			"Load animation config failed: unknown defaults field: " + *field,
			{},"/defaults/" + *field));
	if (defaults.size() != 1 || !defaults.contains("source_type")
		|| !defaults.at("source_type").is_string())
		return std::unexpected(animation_failure(
			animation_config_path,ManifestLoadError::MissingField,
			"Load animation config failed: defaults.source_type is required.",
			{},"/defaults/source_type"));
	AnimationConfig config;
	const std::string source_type = defaults.at("source_type").get<std::string>();
	if (source_type == "frame_directory") config.source_type = AnimationSourceType::FrameDirectory;
	else if (source_type == "horizontal_strip") config.source_type = AnimationSourceType::HorizontalStrip;
	else
		return std::unexpected(animation_failure(
			animation_config_path,ManifestLoadError::InvalidValue,
			"Load animation config failed: unsupported source_type: " + source_type,
			{},"/defaults/source_type"));

	for (auto animation = root.at("animations").begin(); animation != root.at("animations").end(); ++animation)
	{
		const std::string pointer = "/animations/" + escape_json_pointer(animation.key());
		if (auto key_result = elysia::resources::ResourceKeyBuilder::validate_component(animation.key());
			!key_result)
			return std::unexpected(animation_failure(
				animation_config_path,ManifestLoadError::InvalidResourceKey,
				"Load animation config failed: " + key_result.error().message,
				animation.key(),pointer,key_result.error().origin));
		if (!animation.value().is_object())
			return std::unexpected(animation_failure(
				animation_config_path,ManifestLoadError::InvalidField,
				"Load animation config failed: animation entry is not an object.",
				animation.key(),pointer));
		const json& node = animation.value();
		if (node.contains("segments"))
		{
			if (const auto field = unknown_field(node,{"segments"}))
				return std::unexpected(animation_failure(
					animation_config_path,ManifestLoadError::UnknownField,
					"Load animation config failed: unknown segmented animation field: " + *field,
					animation.key(),pointer + "/" + *field));
			if (!node.at("segments").is_array() || node.at("segments").empty()
				|| node.at("segments").size() > 100)
				return std::unexpected(animation_failure(
					animation_config_path,ManifestLoadError::InvalidValue,
					"Load animation config failed: segments must contain 1-100 entries.",
					animation.key(),pointer + "/segments"));
			for (size_t index = 0; index < node.at("segments").size(); ++index)
			{
				const json& segment = node.at("segments").at(index);
				const std::string segment_pointer = pointer + "/segments/" + std::to_string(index);
				if (!segment.is_object())
					return std::unexpected(animation_failure(
						animation_config_path,ManifestLoadError::InvalidField,
						"Load animation config failed: segment is not an object.",
						animation.key(),segment_pointer));
				if (auto appended = append_clip(animation_config_path,segment_pointer,
					animation.key(),true,index,segment,layout,config); !appended)
					return std::unexpected(std::move(appended.error()));
			}
		}
		else if (auto appended = append_clip(animation_config_path,pointer,
			animation.key(),false,0,node,layout,config); !appended)
			return std::unexpected(std::move(appended.error()));
	}
	return config;
}

std::expected<std::filesystem::path,ManifestLoadFailure>
AnimationConfigLoader::resolve_clip_path(
	const std::filesystem::path& config_path,
	const std::string& json_pointer,
	const std::string& animation_name,
	bool is_segment,
	size_t segment_index,
	const AnimationLayout& layout) const
{
	const auto iterator = layout.animations.find(animation_name);
	if (iterator == layout.animations.end())
		return std::unexpected(animation_failure(
			config_path,ManifestLoadError::MissingContent,
			"Load animation clip failed: layout entry does not exist: " + animation_name,
			animation_name,json_pointer));
	const AnimationLayoutEntry& entry = iterator->second;
	if (!is_segment)
	{
		if (!entry.has_path)
			return std::unexpected(animation_failure(
				config_path,ManifestLoadError::MissingContent,
				"Load animation clip failed: layout path is unavailable.",
				animation_name,json_pointer));
		return entry.path;
	}
	if (!entry.has_segment_path)
		return std::unexpected(animation_failure(
			config_path,ManifestLoadError::MissingContent,
			"Load animation clip failed: segmented layout path is unavailable.",
			animation_name,json_pointer));
	std::string segment;
	if (!elysia::resources::format_filesystem_segment(segment_index, segment))
		return std::unexpected(animation_failure(
			config_path,ManifestLoadError::InvalidValue,
			"Load animation clip failed: segment index is invalid.",
			animation_name,json_pointer));
	std::string path = entry.segment_path.generic_string();
	const size_t marker = path.find("{segment}");
	if (marker == std::string::npos) return (entry.segment_path / segment).lexically_normal();
	path.replace(marker, std::string("{segment}").size(), segment);
	return std::filesystem::path(path).lexically_normal();
}

std::expected<void,ManifestLoadFailure> AnimationConfigLoader::append_clip(
	const std::filesystem::path& config_path,
	const std::string& json_pointer,
	const std::string& animation_name,
	bool is_segment,
	size_t segment_index,
	const json& clip_node,
	const AnimationLayout& layout,
	AnimationConfig& config) const
{
	if (const auto field = unknown_field(clip_node,{"frame_count","fps","loop"}))
		return std::unexpected(animation_failure(
			config_path,ManifestLoadError::UnknownField,
			"Load animation clip failed: unknown field: " + *field,
			animation_name,json_pointer + "/" + *field));
	if (clip_node.size() != 3
		|| !clip_node.contains("frame_count") || !clip_node.at("frame_count").is_number_integer()
		|| !clip_node.contains("fps") || !clip_node.at("fps").is_number()
		|| !clip_node.contains("loop") || !clip_node.at("loop").is_boolean())
		return std::unexpected(animation_failure(
			config_path,ManifestLoadError::InvalidSchema,
			"Load animation clip failed: frame_count, fps and loop are required.",
			animation_name,json_pointer));
	const int frame_count = clip_node.at("frame_count").get<int>();
	const double fps = clip_node.at("fps").get<double>();
	if (frame_count <= 0 || fps <= 0.0)
		return std::unexpected(animation_failure(
			config_path,ManifestLoadError::InvalidValue,
			"Load animation clip failed: frame_count and fps must be positive.",
			animation_name,json_pointer));
	auto path = resolve_clip_path(
		config_path,json_pointer,animation_name,is_segment,segment_index,layout);
	if (!path) return std::unexpected(std::move(path.error()));
	AnimationClipConfig clip;
	clip.animation_name = animation_name;
	clip.path = std::move(*path);
	clip.frame_count = static_cast<size_t>(frame_count);
	clip.fps = fps;
	clip.loop = clip_node.at("loop").get<bool>();
	clip.is_segment = is_segment;
	clip.segment_index = segment_index;
	clip.origin = elysia::resources::make_resource_origin(
		config_path, json_pointer, {}, "animations", {}, animation_name,
		is_segment ? std::optional<size_t>(segment_index) : std::nullopt);
	config.clips.push_back(std::move(clip));
	return {};
}
}
