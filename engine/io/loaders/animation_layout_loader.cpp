#include "animation_layout_loader.h"

#include "../json/json_loader.h"
#include "../../resources/pipeline/resource_key_builder.h"

namespace elysia::io
{
std::expected<AnimationLayout,ManifestLoadFailure> AnimationLayoutLoader::load(
	const std::filesystem::path& layout_path) const
{
	const auto fail = [&layout_path](ManifestLoadError code,std::string message,
		std::string key = {},std::string pointer = {},
		std::source_location origin = std::source_location::current())
		-> std::expected<AnimationLayout,ManifestLoadFailure>
	{
		return std::unexpected(make_manifest_load_failure(
			code,std::move(message),"animation-layout",std::move(key),layout_path,
			layout_path,std::move(pointer),origin));
	};
	if (auto source = validate_manifest_source(layout_path,"animation-layout");
		!source)
		return std::unexpected(std::move(source.error()));
	JsonLoader loader;
	const auto read = loader.open_file(layout_path);
	if (!read) return std::unexpected(manifest_failure_from_json(
		read.error(),"animation-layout","Load animation layout failed: "));
	if (!loader.root().is_object() || loader.root().size() != 1
		|| !loader.root().contains("animations") || !loader.root().at("animations").is_object())
		return fail(ManifestLoadError::InvalidSchema,
			"Load animation layout failed: animations is missing or invalid.",{},"/animations");

	AnimationLayout parsed;
	for (json::const_iterator item = loader.root().at("animations").begin(); item != loader.root().at("animations").end(); ++item)
	{
		const std::string pointer = "/animations/" + item.key();
		if (auto key_result = elysia::resources::ResourceKeyBuilder::validate_component(item.key());
			!key_result)
			return fail(ManifestLoadError::InvalidResourceKey,
				"Load animation layout failed: " + key_result.error().message,
				item.key(),pointer,key_result.error().origin);
		if (!item.value().is_object())
			return fail(ManifestLoadError::InvalidField,
				"Load animation layout failed: entry is not an object.",item.key(),pointer);
		const json& node = item.value();
		for (auto field = node.begin(); field != node.end(); ++field)
			if (field.key() != "path" && field.key() != "segment_path")
				return fail(ManifestLoadError::UnknownField,
					"Load animation layout failed: unknown field: " + field.key(),
					item.key(),pointer + "/" + field.key());
		const bool has_path = node.contains("path");
		const bool has_segment_path = node.contains("segment_path");
		if (has_path == has_segment_path || (has_path && !node.at("path").is_string())
			|| (has_segment_path && !node.at("segment_path").is_string()))
			return fail(ManifestLoadError::InvalidSchema,
				"Load animation layout failed: entry must contain exactly one path.",
				item.key(),pointer);
		if ((has_path && node.at("path").get<std::string>().empty())
			|| (has_segment_path && node.at("segment_path").get<std::string>().empty()))
			return fail(ManifestLoadError::InvalidValue,
				"Load animation layout failed: path is empty.",item.key(),pointer);
		AnimationLayoutEntry entry;
		entry.has_path = has_path;
		entry.has_segment_path = has_segment_path;
		if (has_path) entry.path = node.at("path").get<std::string>();
		else entry.segment_path = node.at("segment_path").get<std::string>();
		entry.origin = elysia::resources::make_resource_origin(
			layout_path,pointer,{},"animations",{},item.key());
		parsed.animations.emplace(item.key(), std::move(entry));
	}
	return parsed;
}
}
