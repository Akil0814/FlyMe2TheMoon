#include "entity_manifest_loader.h"

#include "../json/json_loader.h"
#include "../../resources/pipeline/resource_key_builder.h"

#include <unordered_map>

namespace elysia::io
{
std::expected<EntityManifest,ManifestLoadFailure> EntityManifestLoader::load(
	const std::filesystem::path& manifest_path) const
{
	const auto fail = [&manifest_path](ManifestLoadError code,std::string message,
		std::string key = {},std::string pointer = {},
		std::source_location origin = std::source_location::current())
		-> std::expected<EntityManifest,ManifestLoadFailure>
	{
		return std::unexpected(make_manifest_load_failure(
			code,std::move(message),"entity-manifest",std::move(key),manifest_path,
			manifest_path,std::move(pointer),origin));
	};
	if (auto source = validate_manifest_source(manifest_path,"entity-manifest");
		!source)
		return std::unexpected(std::move(source.error()));
	JsonLoader loader;
	const auto read = loader.open_file(manifest_path);
	if (!read) return std::unexpected(manifest_failure_from_json(
		read.error(),"entity-manifest","Load entity manifest failed: "));
	if (!loader.root().is_object() || loader.root().size() != 1
		|| !loader.root().contains("entities") || !loader.root().at("entities").is_array())
		return fail(ManifestLoadError::InvalidSchema,
			"Load entity manifest failed: entities is missing or invalid.",{},"/entities");

	EntityManifest parsed;
	std::unordered_map<std::string, elysia::resources::ResourceOrigin> entity_origins;
	size_t entity_index = 0;
	for (const json& node : loader.root().at("entities"))
	{
		const std::string pointer = "/entities/" + std::to_string(entity_index++);
		if (!node.is_object() || !node.contains("id") || !node.at("id").is_string())
			return fail(ManifestLoadError::MissingField,
				"Load entity manifest failed: entity id is missing or invalid.",{},pointer + "/id");
		if (node.contains("enabled") && !node.at("enabled").is_boolean())
			return fail(ManifestLoadError::InvalidField,
				"Load entity manifest failed: enabled is invalid.",
				node.at("id").get<std::string>(),pointer + "/enabled");
		for (auto field = node.begin(); field != node.end(); ++field)
			if (field.key() != "id" && field.key() != "enabled"
				&& field.key() != "animation_layout")
				return fail(ManifestLoadError::UnknownField,
					"Load entity manifest failed: unknown field: " + field.key(),{},pointer + "/" + field.key());
		EntityManifestEntry entry;
		entry.id = node.at("id").get<std::string>();
		if (node.contains("animation_layout"))
		{
			if (!node.at("animation_layout").is_string())
				return fail(ManifestLoadError::InvalidField,
					"Load entity manifest failed: animation_layout is invalid.",entry.id,
					pointer + "/animation_layout");
			entry.animation_layout = node.at("animation_layout").get<std::string>();
		}
		if (auto key_result = elysia::resources::ResourceKeyBuilder::validate_component(entry.id);
			!key_result)
			return fail(ManifestLoadError::InvalidResourceKey,
				"Load entity manifest failed: " + key_result.error().message,
				entry.id,pointer + "/id",key_result.error().origin);
		if (!entry.animation_layout.empty())
			if (auto key_result = elysia::resources::ResourceKeyBuilder::validate_component(entry.animation_layout);
				!key_result)
				return fail(ManifestLoadError::InvalidResourceKey,
					"Load entity manifest failed: " + key_result.error().message,
					entry.id,pointer + "/animation_layout",key_result.error().origin);
		entry.origin = elysia::resources::make_resource_origin(
			manifest_path, pointer, {}, "entities", entry.id, entry.id);
		const auto [first, inserted] = entity_origins.emplace(entry.id, entry.origin);
		if (!inserted)
		{
			const std::string message = "Load entity manifest failed: duplicate entity id: " + entry.id;
			return std::unexpected(ManifestLoadFailure{
				ManifestLoadError::DuplicateKey,
				elysia::core::make_failure_diagnostic(message,{
					elysia::core::make_failure_diagnostic_entry(
						"entity",entry.id,manifest_path,first->second.config_path,
						first->second.json_pointer,"first declaration"),
					elysia::core::make_failure_diagnostic_entry(
						"entity",entry.id,manifest_path,entry.origin.config_path,
						entry.origin.json_pointer,"duplicate declaration")})});
		}
		if (node.value("enabled", true) == false)
			continue;
		parsed.entities.push_back(std::move(entry));
	}

	return parsed;
}
}
