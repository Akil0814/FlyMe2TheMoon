#include "../../resources/pipeline/resource_key_builder.h"
#include "effect_definition_config_loader.h"

#include "../json/json_loader.h"

#include <algorithm>
#include <utility>

namespace elysia::io
{
std::expected<EffectDefinitionConfig,ManifestLoadFailure>
EffectDefinitionConfigLoader::load(
	const std::filesystem::path& config_path,
	const AnimationConfig& animation_config) const
{
	const auto fail = [&config_path](ManifestLoadError code,std::string message,
		std::string key = {},std::string pointer = {},
		std::source_location origin = std::source_location::current())
		-> std::expected<EffectDefinitionConfig,ManifestLoadFailure>
	{
		return std::unexpected(make_manifest_load_failure(
			code,std::move(message),"effect-config",std::move(key),config_path,
			config_path,std::move(pointer),origin));
	};
	if (auto source = validate_manifest_source(config_path,"effect-config");
		!source)
		return std::unexpected(std::move(source.error()));
	JsonLoader loader;
	const auto read = loader.open_file(config_path);
	if (!read) return std::unexpected(manifest_failure_from_json(
		read.error(),"effect-config","Load effect definition config failed: "));
	if (!loader.root().is_object() || loader.root().size() != 1
		|| !loader.root().contains("effects") || !loader.root().at("effects").is_object())
		return fail(ManifestLoadError::InvalidSchema,
			"Load effect definition config failed: effects is missing or invalid.",{},"/effects");
	EffectDefinitionConfig parsed;
	for (auto item = loader.root().at("effects").begin(); item != loader.root().at("effects").end(); ++item)
	{
		const std::string pointer = "/effects/" + item.key();
		if (auto key_result = elysia::resources::ResourceKeyBuilder::validate_component(item.key());
			!key_result)
			return fail(ManifestLoadError::InvalidResourceKey,
				"Load effect definition config failed: " + key_result.error().message,
				item.key(),pointer,key_result.error().origin);
		if (!item.value().is_object())
			return fail(ManifestLoadError::InvalidField,
				"Load effect definition config failed: effect entry is not an object.",
				item.key(),pointer);
		const json& node = item.value();
		for (auto field = node.begin(); field != node.end(); ++field)
			if (field.key() != "animation" && field.key() != "default_width"
				&& field.key() != "default_height" && field.key() != "default_angle_degrees")
				return fail(ManifestLoadError::UnknownField,
					"Load effect definition config failed: unknown field: " + field.key(),
					item.key(),pointer + "/" + field.key());
		if (!node.contains("animation") || !node.at("animation").is_string())
			return fail(ManifestLoadError::MissingField,
				"Load effect definition config failed: animation is missing or invalid.",
				item.key(),pointer + "/animation");
		const std::string animation_name = node.at("animation").get<std::string>();
		if (auto key_result = elysia::resources::ResourceKeyBuilder::validate_component(animation_name);
			!key_result)
			return fail(ManifestLoadError::InvalidResourceKey,
				"Load effect definition config failed: " + key_result.error().message,
				item.key(),pointer + "/animation",key_result.error().origin);
		float width = 0.0f;
		float height = 0.0f;
		double angle = 0.0;
		if (node.contains("default_width"))
		{
			if (!node.at("default_width").is_number())
				return fail(ManifestLoadError::InvalidField,
					"Load effect definition config failed: default_width is not numeric.",
					item.key(),pointer + "/default_width");
			width = node.at("default_width").get<float>();
		}
		if (node.contains("default_height"))
		{
			if (!node.at("default_height").is_number())
				return fail(ManifestLoadError::InvalidField,
					"Load effect definition config failed: default_height is not numeric.",
					item.key(),pointer + "/default_height");
			height = node.at("default_height").get<float>();
		}
		if (node.contains("default_angle_degrees"))
		{
			if (!node.at("default_angle_degrees").is_number())
				return fail(ManifestLoadError::InvalidField,
					"Load effect definition config failed: default_angle_degrees is not numeric.",
					item.key(),pointer + "/default_angle_degrees");
			angle = node.at("default_angle_degrees").get<double>();
		}
		if ((width == 0.0f) != (height == 0.0f) || width < 0.0f || height < 0.0f)
			return fail(ManifestLoadError::InvalidValue,
				"Load effect definition config failed: width and height must both be zero or positive.",
				item.key(),pointer);
		bool matched = false;
		for (const AnimationClipConfig& clip : animation_config.clips)
		{
			if (clip.animation_name != animation_name) continue;
			matched = true;
			EffectDefinitionConfigEntry entry;
			entry.effect_name = item.key();
			entry.animation_name = animation_name;
			entry.is_segment = clip.is_segment;
			entry.segment_index = clip.segment_index;
			entry.default_width = width;
			entry.default_height = height;
			entry.default_angle_degrees = angle;
			entry.origin = elysia::resources::make_resource_origin(
				config_path,pointer,{},"effects",{},item.key(),
				clip.is_segment ? std::optional<size_t>(clip.segment_index) : std::nullopt);
			parsed.effects.push_back(std::move(entry));
		}
		if (!matched)
			return fail(ManifestLoadError::MissingContent,
				"Load effect definition config failed: animation does not exist: " + animation_name,
				item.key(),pointer + "/animation");
	}
	return parsed;
}
}
