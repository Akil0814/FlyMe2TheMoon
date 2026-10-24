#include "animated_entity_content_loader.h"

#include "../io/loaders/animation_config_loader.h"
#include "../io/loaders/animation_layout_loader.h"
#include "../io/loaders/effect_definition_config_loader.h"
#include "../io/loaders/entity_audio_layout_loader.h"
#include "../io/loaders/entity_manifest_loader.h"
#include "../io/loaders/entity_texture_layout_loader.h"
#include "../io/json/json_loader.h"
#include "../io/path/path_manager.h"
#include "../resources/pipeline/resource_key_builder.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace elysia::loading
{
namespace
{
using ModuleResult = std::expected<elysia::io::EntityContentModule,
	elysia::io::ManifestLoadFailure>;

std::string escape_pointer(std::string value)
{
	size_t position = 0;
	while ((position = value.find('~',position)) != std::string::npos)
	{
		value.replace(position,1,"~0");
		position += 2;
	}
	position = 0;
	while ((position = value.find('/',position)) != std::string::npos)
	{
		value.replace(position,1,"~1");
		position += 2;
	}
	return value;
}

void replace_all(std::string& value,std::string_view marker,std::string_view replacement)
{
	size_t position = 0;
	while ((position = value.find(marker,position)) != std::string::npos)
	{
		value.replace(position,marker.size(),replacement);
		position += replacement.size();
	}
}

elysia::io::EntityResourceIdentity make_identity(
	const elysia::io::EntityManifestEntry& entity,const std::string& module_name)
{
	auto identity = elysia::io::EntityResourceIdentity{
		entity.id,entity.animation_layout,entity.origin};
	identity.origin.module = module_name;
	identity.origin.scope = elysia::resources::ResourceOriginScope::AdditionalModule;
	return identity;
}

void enrich_animation_origins(
	elysia::io::AnimationConfig& config,const std::string& module_name,
	const std::string& entity_id)
{
	for (auto& clip : config.clips)
	{
		clip.origin.module = module_name;
		clip.origin.scope = elysia::resources::ResourceOriginScope::AdditionalModule;
		clip.origin.capability = "animations";
		clip.origin.entity_id = entity_id;
	}
}

void enrich_effect_origins(
	elysia::io::EffectDefinitionConfig& config,const std::string& module_name,
	const std::string& entity_id)
{
	for (auto& effect : config.effects)
	{
		effect.origin.module = module_name;
		effect.origin.scope = elysia::resources::ResourceOriginScope::AdditionalModule;
		effect.origin.capability = "effects";
		effect.origin.entity_id = entity_id;
	}
}

class ModuleParser
{
public:
	ModuleParser(std::string module_name,std::filesystem::path manifest_path,
		const elysia::io::PathManager& paths)
		: _module_name(std::move(module_name)),
		  _manifest_path(std::move(manifest_path)),_paths(paths) {}

	ModuleResult parse() const
	{
		if (auto source = elysia::io::validate_manifest_source(
			_manifest_path,"additional-module"); !source)
		{
			auto error = std::move(source.error());
			if (!error.diagnostic.entries.empty())
				error.diagnostic.entries.front().subject_key = _module_name;
			return std::unexpected(std::move(error));
		}
		elysia::io::JsonLoader loader;
		const auto read = loader.open_file(_manifest_path);
		if (!read)
			return std::unexpected(elysia::io::manifest_failure_from_json(
				read.error(),"additional-module","Entity content module failed: "));
		if (!loader.root().is_object())
			return std::unexpected(failure(elysia::io::ManifestLoadError::InvalidDocument,
				"Entity content module failed: module manifest root is not an object."));
		const auto& root = loader.root();
		if (auto fields = require_only_fields(root,
			{"entities","key_namespace","capabilities"},"",3); !fields)
			return std::unexpected(std::move(fields.error()));

		auto entities_value = required_string(root,"entities","");
		if (!entities_value) return std::unexpected(std::move(entities_value.error()));
		auto key_namespace = required_string(root,"key_namespace","",true);
		if (!key_namespace) return std::unexpected(std::move(key_namespace.error()));
		if (!key_namespace->empty())
			if (auto key_result = elysia::resources::ResourceKeyBuilder::validate_component(*key_namespace);
				!key_result)
				return std::unexpected(failure(
					elysia::io::ManifestLoadError::InvalidResourceKey,
					"Entity content module failed: " + key_result.error().message,
					"additional-module",_module_name,_manifest_path,"/key_namespace",
					key_result.error().origin));
		if (!root.at("capabilities").is_object())
			return std::unexpected(failure(elysia::io::ManifestLoadError::InvalidField,
				"Entity content module failed: capabilities is missing or invalid.",
				"additional-module",_module_name,_manifest_path,"/capabilities"));
		const auto& capabilities = root.at("capabilities");
		for (auto item = capabilities.begin(); item != capabilities.end(); ++item)
		{
			if (item.key() != "animations" && item.key() != "effects"
				&& item.key() != "textures" && item.key() != "audio")
				return std::unexpected(failure(elysia::io::ManifestLoadError::UnknownField,
					"Entity content module failed: unknown capability: " + item.key(),
					"additional-module",_module_name,_manifest_path,
					"/capabilities/" + escape_pointer(item.key())));
			if (!item.value().is_object())
				return std::unexpected(failure(elysia::io::ManifestLoadError::InvalidField,
					"Entity content module failed: capability is not an object: " + item.key(),
					"additional-module",_module_name,_manifest_path,
					"/capabilities/" + escape_pointer(item.key())));
		}
		if (capabilities.contains("effects") && !capabilities.contains("animations"))
			return std::unexpected(failure(elysia::io::ManifestLoadError::MissingContent,
				"Entity content module failed: effects requires animations in the same module.",
				"additional-module",_module_name,_manifest_path,"/capabilities/effects"));

		const auto entities_path = _paths.to_asset_path(*entities_value);
		auto entity_manifest = elysia::io::EntityManifestLoader{}.load(entities_path);
		if (!entity_manifest) return std::unexpected(std::move(entity_manifest.error()));

		elysia::io::EntityContentModule content;
		content.name = _module_name;
		content.key_namespace = std::move(*key_namespace);
		for (const auto& entity : entity_manifest->entities)
			content.entities.push_back(make_identity(entity,_module_name));

		if (capabilities.contains("animations"))
		{
			auto result = parse_animations(
				capabilities.at("animations"),*entity_manifest,content);
			if (!result) return std::unexpected(std::move(result.error()));
		}
		if (capabilities.contains("effects"))
		{
			auto result = parse_effects(capabilities.at("effects"),content);
			if (!result) return std::unexpected(std::move(result.error()));
		}
		if (capabilities.contains("textures"))
		{
			auto result = parse_textures(
				capabilities.at("textures"),*entity_manifest,content);
			if (!result) return std::unexpected(std::move(result.error()));
		}
		if (capabilities.contains("audio"))
		{
			auto result = parse_audio(
				capabilities.at("audio"),*entity_manifest,content);
			if (!result) return std::unexpected(std::move(result.error()));
		}
		return content;
	}

private:
	elysia::io::ManifestLoadFailure failure(
		elysia::io::ManifestLoadError code,std::string message,
		std::string type = "additional-module",std::string key = {},
		std::filesystem::path expected = {},std::string pointer = {},
		std::source_location origin = std::source_location::current()) const
	{
		if (key.empty()) key = _module_name;
		if (expected.empty()) expected = _manifest_path;
		return elysia::io::make_manifest_load_failure(
			code,std::move(message),std::move(type),std::move(key),
			std::move(expected),_manifest_path,std::move(pointer),origin);
	}

	std::expected<void,elysia::io::ManifestLoadFailure> require_only_fields(
		const elysia::io::json& node,std::initializer_list<std::string_view> fields,
		std::string pointer,std::optional<std::size_t> exact_size = std::nullopt) const
	{
		for (auto item = node.begin(); item != node.end(); ++item)
		{
			bool known = false;
			for (const auto field : fields) known = known || item.key() == field;
			if (!known)
			{
				std::string label;
				if (pointer.starts_with("/capabilities/"))
					label = pointer.substr(std::string("/capabilities/").size())
						+ " capability ";
				return std::unexpected(failure(elysia::io::ManifestLoadError::UnknownField,
					"Entity content module failed: unknown " + label + "field: " + item.key(),
					"additional-module",_module_name,_manifest_path,
					pointer + "/" + escape_pointer(item.key())));
			}
		}
		if (exact_size && node.size() != *exact_size)
			return std::unexpected(failure(elysia::io::ManifestLoadError::InvalidSchema,
				"Entity content module failed: required fields are missing.",
				"additional-module",_module_name,_manifest_path,pointer));
		return {};
	}

	std::expected<std::string,elysia::io::ManifestLoadFailure> required_string(
		const elysia::io::json& node,std::string_view field,std::string pointer,
		bool allow_empty = false) const
	{
		const std::string name(field);
		const std::string field_pointer = pointer + "/" + name;
		if (!node.contains(name) || !node.at(name).is_string())
			return std::unexpected(failure(elysia::io::ManifestLoadError::MissingField,
				"Entity content module failed: " + name + " is missing or invalid.",
				"additional-module",_module_name,_manifest_path,field_pointer));
		std::string value = node.at(name).get<std::string>();
		if (!allow_empty && value.empty())
			return std::unexpected(failure(elysia::io::ManifestLoadError::InvalidValue,
				"Entity content module failed: " + name + " is empty.",
				"additional-module",_module_name,_manifest_path,field_pointer));
		return value;
	}

	std::expected<void,elysia::io::ManifestLoadFailure> validate_tokens(
		std::string_view value,std::initializer_list<std::string_view> allowed,
		std::string pointer) const
	{
		size_t position = 0;
		while (position < value.size())
		{
			const size_t begin = value.find_first_of("{}",position);
			if (begin == std::string_view::npos) break;
			if (value[begin] == '}')
				return std::unexpected(failure(elysia::io::ManifestLoadError::InvalidValue,
					"Entity content module failed: template has an unmatched brace.",
					"additional-module",_module_name,_manifest_path,pointer));
			const size_t end = value.find('}',begin + 1);
			if (end == std::string_view::npos || value.find('{',begin + 1) < end)
				return std::unexpected(failure(elysia::io::ManifestLoadError::InvalidValue,
					"Entity content module failed: template token is malformed.",
					"additional-module",_module_name,_manifest_path,pointer));
			const auto token = value.substr(begin,end - begin + 1);
			bool known = false;
			for (const auto candidate : allowed) known = known || token == candidate;
			if (!known)
				return std::unexpected(failure(elysia::io::ManifestLoadError::InvalidValue,
					"Entity content module failed: template contains unsupported token: "
						+ std::string(token),"additional-module",_module_name,
					_manifest_path,pointer));
			position = end + 1;
		}
		return {};
	}

	std::expected<std::filesystem::path,elysia::io::ManifestLoadFailure>
	resolve_entity_root(
		const std::string& pattern,const std::string& id,std::string pointer,
		std::string capability) const
	{
		if (auto result = validate_tokens(pattern,{"{id}"},pointer); !result)
			return std::unexpected(std::move(result.error()));
		std::string value = pattern;
		if (value.find("{id}") == std::string::npos)
			value = (std::filesystem::path(value) / id).generic_string();
		else replace_all(value,"{id}",id);
		auto path = _paths.to_asset_path(value);
		std::error_code error;
		const bool directory = std::filesystem::is_directory(path,error);
		if (error && error != std::errc::no_such_file_or_directory)
			return std::unexpected(failure(elysia::io::ManifestLoadError::FilesystemAccess,
				"Entity content module failed: resource root access failed for module '"
					+ _module_name + "', capability '" + capability + "', entity '"
					+ id + "': " + error.message(),
				capability,id,path,pointer));
		if (!directory)
			return std::unexpected(failure(elysia::io::ManifestLoadError::MissingContent,
				"Entity content module failed: resource root does not exist for module '"
					+ _module_name + "', capability '" + capability + "', entity '"
					+ id + "'.",
				capability,id,path,pointer));
		return path;
	}

	std::expected<std::filesystem::path,elysia::io::ManifestLoadFailure>
	resolve_config_template(
		const std::string& pattern,const std::string& id,std::string pointer,
		std::string capability) const
	{
		if (auto result = validate_tokens(pattern,{"{id}"},pointer); !result)
			return std::unexpected(std::move(result.error()));
		if (pattern.find("{id}") == std::string::npos)
			return std::unexpected(failure(elysia::io::ManifestLoadError::InvalidValue,
				"Entity content module failed: config_template must contain {id}.",
				capability,id,_manifest_path,pointer));
		std::string value = pattern;
		replace_all(value,"{id}",id);
		auto path = _paths.to_config_path(value);
		std::error_code error;
		const bool regular = std::filesystem::is_regular_file(path,error);
		if (error && error != std::errc::no_such_file_or_directory)
			return std::unexpected(failure(elysia::io::ManifestLoadError::FilesystemAccess,
				"Entity content module failed: configured file access failed: " + error.message(),
				capability,id,path,pointer));
		if (!regular)
			return std::unexpected(failure(elysia::io::ManifestLoadError::MissingContent,
				"Entity content module failed: configured file does not exist.",
				capability,id,path,pointer));
		return path;
	}

	std::expected<std::unordered_map<std::string,elysia::io::AnimationLayout>,
		elysia::io::ManifestLoadFailure> load_animation_layouts(
		const elysia::io::json& capability) const
	{
		const std::string base = "/capabilities/animations/layouts";
		if (!capability.contains("layouts") || !capability.at("layouts").is_object()
			|| capability.at("layouts").empty())
			return std::unexpected(failure(elysia::io::ManifestLoadError::MissingField,
				"Entity content module failed: animations.layouts is missing or empty.",
				"animations",_module_name,_manifest_path,base));
		std::unordered_map<std::string,elysia::io::AnimationLayout> layouts;
		for (auto item = capability.at("layouts").begin();
			item != capability.at("layouts").end(); ++item)
		{
			const std::string pointer = base + "/" + escape_pointer(item.key());
			if (auto key_result = elysia::resources::ResourceKeyBuilder::validate_component(item.key());
				!key_result)
				return std::unexpected(failure(
					elysia::io::ManifestLoadError::InvalidResourceKey,
					"Entity content module failed: " + key_result.error().message,
					"animations",item.key(),_manifest_path,pointer,key_result.error().origin));
			if (!item.value().is_string())
				return std::unexpected(failure(elysia::io::ManifestLoadError::InvalidField,
					"Entity content module failed: animation layout path is invalid.",
					"animations",item.key(),_manifest_path,pointer));
			const auto path = _paths.to_asset_path(item.value().get<std::string>());
			auto layout = elysia::io::AnimationLayoutLoader{}.load(path);
			if (!layout) return std::unexpected(std::move(layout.error()));
			layouts.emplace(item.key(),std::move(*layout));
		}
		return layouts;
	}

	std::expected<void,elysia::io::ManifestLoadFailure> validate_frame_prefix(
		const std::string& value,bool has_segments) const
	{
		const std::string pointer = "/capabilities/animations/frame_prefix_template";
		if (value.find('/') != std::string::npos || value.find('\\') != std::string::npos
			|| value.find("..") != std::string::npos)
			return std::unexpected(failure(elysia::io::ManifestLoadError::InvalidValue,
				"Entity content module failed: frame_prefix_template must be a filename prefix.",
				"animations",_module_name,_manifest_path,pointer));
		if (auto result = validate_tokens(
			value,{"{id}","{animation}","{segment_suffix}"},pointer); !result)
			return result;
		if (value.find("{id}") == std::string::npos
			|| value.find("{animation}") == std::string::npos
			|| (has_segments && value.find("{segment_suffix}") == std::string::npos))
			return std::unexpected(failure(elysia::io::ManifestLoadError::InvalidValue,
				"Entity content module failed: frame_prefix_template is missing required tokens.",
				"animations",_module_name,_manifest_path,pointer));
		return {};
	}

	std::expected<void,elysia::io::ManifestLoadFailure> parse_animations(
		const elysia::io::json& capability,const elysia::io::EntityManifest& manifest,
		elysia::io::EntityContentModule& content) const
	{
		if (auto fields = require_only_fields(capability,
			{"texture_root","config_template","frame_prefix_template","layouts"},
			"/capabilities/animations"); !fields) return fields;
		auto texture_template = required_string(
			capability,"texture_root","/capabilities/animations");
		if (!texture_template) return std::unexpected(std::move(texture_template.error()));
		auto config_template = required_string(
			capability,"config_template","/capabilities/animations");
		if (!config_template) return std::unexpected(std::move(config_template.error()));
		std::string frame_prefix;
		if (capability.contains("frame_prefix_template"))
		{
			if (!capability.at("frame_prefix_template").is_string())
				return std::unexpected(failure(elysia::io::ManifestLoadError::InvalidField,
					"Entity content module failed: frame_prefix_template is invalid.",
					"animations",_module_name,_manifest_path,
					"/capabilities/animations/frame_prefix_template"));
			frame_prefix = capability.at("frame_prefix_template").get<std::string>();
		}
		auto layouts = load_animation_layouts(capability);
		if (!layouts) return std::unexpected(std::move(layouts.error()));
		for (const auto& entity : manifest.entities)
		{
			const auto layout = layouts->find(entity.animation_layout);
			if (entity.animation_layout.empty() || layout == layouts->end())
				return std::unexpected(failure(elysia::io::ManifestLoadError::MissingContent,
					"Entity content module failed: unknown animation layout for entity: " + entity.id,
					"animations",entity.id,_manifest_path,"/capabilities/animations/layouts"));
			auto texture_root = resolve_entity_root(*texture_template,entity.id,
				"/capabilities/animations/texture_root","animations");
			if (!texture_root) return std::unexpected(std::move(texture_root.error()));
			auto config_path = resolve_config_template(*config_template,entity.id,
				"/capabilities/animations/config_template","animations");
			if (!config_path) return std::unexpected(std::move(config_path.error()));
			auto config = elysia::io::AnimationConfigLoader{}.load(*config_path,layout->second);
			if (!config) return std::unexpected(std::move(config.error()));
			const bool has_segments = std::any_of(config->clips.begin(),config->clips.end(),
				[](const auto& clip) { return clip.is_segment; });
			if (config->source_type == elysia::io::AnimationSourceType::FrameDirectory)
			{
				if (frame_prefix.empty())
					return std::unexpected(failure(elysia::io::ManifestLoadError::MissingField,
						"Entity content module failed: frame_prefix_template is required.",
						"animations",entity.id,_manifest_path,
						"/capabilities/animations/frame_prefix_template"));
				if (auto prefix = validate_frame_prefix(frame_prefix,has_segments); !prefix)
					return prefix;
			}
			enrich_animation_origins(*config,_module_name,entity.id);
			content.animation_entries.push_back({
				make_identity(entity,_module_name),std::move(*texture_root),frame_prefix,
				std::move(*config)});
		}
		return {};
	}

	std::expected<void,elysia::io::ManifestLoadFailure> parse_effects(
		const elysia::io::json& capability,elysia::io::EntityContentModule& content) const
	{
		if (auto fields = require_only_fields(capability,{"config_template"},
			"/capabilities/effects"); !fields) return fields;
		auto config_template = required_string(
			capability,"config_template","/capabilities/effects");
		if (!config_template) return std::unexpected(std::move(config_template.error()));
		for (const auto& animation_entry : content.animation_entries)
		{
			auto config_path = resolve_config_template(*config_template,
				animation_entry.entity.id,"/capabilities/effects/config_template","effects");
			if (!config_path) return std::unexpected(std::move(config_path.error()));
			auto config = elysia::io::EffectDefinitionConfigLoader{}.load(
				*config_path,animation_entry.animation_config);
			if (!config) return std::unexpected(std::move(config.error()));
			enrich_effect_origins(*config,_module_name,animation_entry.entity.id);
			content.effect_entries.push_back({animation_entry.entity,std::move(*config)});
		}
		return {};
	}

	std::expected<void,elysia::io::ManifestLoadFailure> parse_textures(
		const elysia::io::json& capability,const elysia::io::EntityManifest& manifest,
		elysia::io::EntityContentModule& content) const
	{
		if (auto fields = require_only_fields(capability,{"texture_root","layout"},
			"/capabilities/textures"); !fields) return fields;
		auto root_template = required_string(
			capability,"texture_root","/capabilities/textures");
		if (!root_template) return std::unexpected(std::move(root_template.error()));
		auto layout_value = required_string(capability,"layout","/capabilities/textures");
		if (!layout_value) return std::unexpected(std::move(layout_value.error()));
		auto layout = elysia::io::EntityTextureLayoutLoader{}.load(
			_paths.to_asset_path(*layout_value));
		if (!layout) return std::unexpected(std::move(layout.error()));
		for (const auto& entity : manifest.entities)
		{
			auto root = resolve_entity_root(*root_template,entity.id,
				"/capabilities/textures/texture_root","textures");
			if (!root) return std::unexpected(std::move(root.error()));
			auto entity_layout = *layout;
			for (auto& item : entity_layout.textures)
			{
				item.origin.module = _module_name;
				item.origin.scope = elysia::resources::ResourceOriginScope::AdditionalModule;
				item.origin.entity_id = entity.id;
			}
			content.texture_entries.push_back({
				make_identity(entity,_module_name),std::move(*root),std::move(entity_layout)});
		}
		return {};
	}

	std::expected<void,elysia::io::ManifestLoadFailure> parse_audio(
		const elysia::io::json& capability,const elysia::io::EntityManifest& manifest,
		elysia::io::EntityContentModule& content) const
	{
		if (auto fields = require_only_fields(capability,{"audio_root","layout"},
			"/capabilities/audio"); !fields) return fields;
		auto root_template = required_string(capability,"audio_root","/capabilities/audio");
		if (!root_template) return std::unexpected(std::move(root_template.error()));
		auto layout_value = required_string(capability,"layout","/capabilities/audio");
		if (!layout_value) return std::unexpected(std::move(layout_value.error()));
		auto layout = elysia::io::EntityAudioLayoutLoader{}.load(
			_paths.to_asset_path(*layout_value));
		if (!layout) return std::unexpected(std::move(layout.error()));
		for (const auto& entity : manifest.entities)
		{
			auto root = resolve_entity_root(*root_template,entity.id,
				"/capabilities/audio/audio_root","audio");
			if (!root) return std::unexpected(std::move(root.error()));
			auto entity_layout = *layout;
			for (auto& item : entity_layout.sounds)
			{
				item.origin.module = _module_name;
				item.origin.scope = elysia::resources::ResourceOriginScope::AdditionalModule;
				item.origin.entity_id = entity.id;
			}
			content.audio_entries.push_back({
				make_identity(entity,_module_name),std::move(*root),std::move(entity_layout)});
		}
		return {};
	}

	std::string _module_name;
	std::filesystem::path _manifest_path;
	const elysia::io::PathManager& _paths;
};
}

std::expected<elysia::io::EntityContentModule,ContentLoadFailure>
AnimatedEntityContentLoader::load(
	const std::string& module_name,const std::filesystem::path& manifest_path) const
{
	const auto* paths = elysia::io::PathManager::instance();
	if (!paths || !paths->is_initialized())
		return std::unexpected(make_content_load_failure(
			ContentLoadError::Config,
			"Additional module load failed: path manager is not initialized.",
			module_name,manifest_path));
	auto content = ModuleParser(module_name,manifest_path,*paths).parse();
	if (!content)
	{
		elysia::core::normalize_failure_diagnostic_paths(
			content.error().diagnostic,paths->root());
		return std::unexpected(make_content_load_failure(
			ContentLoadError::Manifest,std::move(content.error().diagnostic)));
	}
	return std::move(*content);
}
}
