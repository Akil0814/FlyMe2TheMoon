#include "../../resources/pipeline/resource_key_builder.h"
#include "entity_audio_layout_loader.h"
#include "../json/json_loader.h"

namespace elysia::io
{
std::expected<EntityAudioLayout,ManifestLoadFailure>
EntityAudioLayoutLoader::load(const std::filesystem::path& path) const
{
	const auto fail = [&path](ManifestLoadError code,std::string message,
		std::string key = {},std::string pointer = {},
		std::source_location origin = std::source_location::current())
		-> std::expected<EntityAudioLayout,ManifestLoadFailure>
	{
		return std::unexpected(make_manifest_load_failure(
			code,std::move(message),"entity-audio-layout",std::move(key),
			path,path,std::move(pointer),origin));
	};
	if (auto source = validate_manifest_source(path,"entity-audio-layout");
		!source)
		return std::unexpected(std::move(source.error()));
	JsonLoader loader;
	const auto read = loader.open_file(path);
	if (!read) return std::unexpected(manifest_failure_from_json(
		read.error(),"entity-audio-layout","Load entity audio layout failed: "));
	if (!loader.root().is_object()) return fail(ManifestLoadError::InvalidDocument,
		"Load entity audio layout failed: root is not an object.");
	EntityAudioLayout layout;
	for (auto item = loader.root().begin(); item != loader.root().end(); ++item)
	{
		const std::string pointer = "/" + item.key();
		if (auto key_result = elysia::resources::ResourceKeyBuilder::validate_component(item.key());
			!key_result)
			return fail(ManifestLoadError::InvalidResourceKey,
				"Load entity audio layout failed: " + key_result.error().message,
				item.key(),pointer,key_result.error().origin);
		if (!item.value().is_string() || item.value().get<std::string>().empty())
			return fail(ManifestLoadError::InvalidValue,
				"Load entity audio layout failed: path must be a non-empty string.",
				item.key(),pointer);
		layout.sounds.push_back({item.key(), item.value().get<std::string>(),
			elysia::resources::make_resource_origin(path,pointer,{},"audio",{},item.key())});
	}
	return layout;
}
}
