#pragma once

#include "../../resources/resource_origin.h"
#include "../json/json_loader.h"

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace elysia::io
{
struct CoreManifestPaths
{
	std::filesystem::path configs;
	std::filesystem::path audio;
	std::filesystem::path fonts;
	std::filesystem::path i18n;
	std::filesystem::path textures;
	std::filesystem::path animations;
	std::filesystem::path effects;
};

struct BootstrapPaths
{
	std::filesystem::path app_config;
	std::filesystem::path preload_manifest;
};

struct ContentRegistry
{
	BootstrapPaths bootstrap;
	CoreManifestPaths required;
	std::map<std::string, std::filesystem::path> additional_module_manifests;
};

struct FontManifestEntry
{
	std::string key;
	std::filesystem::path file_path;
	elysia::resources::ResourceOrigin origin;
};

struct FontManifest
{
	std::vector<FontManifestEntry> fonts;
};

struct I18nManifestFile
{
	std::filesystem::path path;
	elysia::resources::ResourceOrigin origin;
};

struct I18nManifest
{
	std::string default_language;
	std::vector<std::string> languages;
	std::vector<I18nManifestFile> files;
};

struct AudioManifestEntry
{
	std::string key;
	std::filesystem::path file_path;
	elysia::resources::ResourceOrigin origin;
};

struct AudioManifest
{
	std::vector<AudioManifestEntry> sounds;
	std::vector<AudioManifestEntry> music;
};

struct TextureManifestEntry
{
	std::string key;
	std::filesystem::path file_path;
	elysia::resources::ResourceOrigin origin;
};

struct TextureManifest
{
	std::vector<TextureManifestEntry> textures;
};

struct AnimationManifestEntry
{
	std::string key;
	std::filesystem::path source_path;
	std::string frame_prefix;
	size_t frame_count = 0;
	double fps = 10.0;
	bool loop = true;
	bool horizontal_strip = false;
	elysia::resources::ResourceOrigin origin;
};

struct AnimationManifest
{
	std::vector<AnimationManifestEntry> animations;
};

struct AnimationEffectManifestEntry
{
	std::string key;
	std::string animation_key;
	float default_width = 0.0f;
	float default_height = 0.0f;
	double default_angle_degrees = 0.0;
	elysia::resources::ResourceOrigin origin;
};

struct AnimationEffectManifest
{
	std::vector<AnimationEffectManifestEntry> effects;
};

struct EntityManifestEntry
{
	std::string id;
	std::string animation_layout;
	elysia::resources::ResourceOrigin origin;
};

struct EntityManifest
{
	std::vector<EntityManifestEntry> entities;
};

struct AnimationLayoutEntry
{
	std::filesystem::path path;
	std::filesystem::path segment_path;
	bool has_path = false;
	bool has_segment_path = false;
	elysia::resources::ResourceOrigin origin;
};

struct AnimationLayout
{
	std::unordered_map<std::string, AnimationLayoutEntry> animations;
};

struct EntityTextureLayoutEntry
{
	std::string key;
	std::filesystem::path path;
	elysia::resources::ResourceOrigin origin;
};

struct EntityTextureLayout
{
	std::vector<EntityTextureLayoutEntry> textures;
};

struct EntityAudioLayoutEntry
{
	std::string key;
	std::filesystem::path path;
	elysia::resources::ResourceOrigin origin;
};

struct EntityAudioLayout
{
	std::vector<EntityAudioLayoutEntry> sounds;
};

enum class AnimationSourceType
{
	FrameDirectory,
	HorizontalStrip
};

struct AnimationClipConfig
{
	std::string animation_name;
	std::filesystem::path path;
	size_t frame_count = 0;
	double fps = 10.0;
	bool loop = true;
	bool is_segment = false;
	size_t segment_index = 0;
	elysia::resources::ResourceOrigin origin;
};

struct AnimationConfig
{
	AnimationSourceType source_type = AnimationSourceType::FrameDirectory;
	std::vector<AnimationClipConfig> clips;
};

struct EffectDefinitionConfigEntry
{
	std::string effect_name;
	std::string animation_name;
	bool is_segment = false;
	size_t segment_index = 0;
	float default_width = 0.0f;
	float default_height = 0.0f;
	double default_angle_degrees = 0.0;
	elysia::resources::ResourceOrigin origin;
};

struct EffectDefinitionConfig
{
	std::vector<EffectDefinitionConfigEntry> effects;
};

struct EntityResourceIdentity
{
	std::string id;
	std::string animation_layout;
	elysia::resources::ResourceOrigin origin;
};

struct EntityAnimationContentEntry
{
	EntityResourceIdentity entity;
	std::filesystem::path texture_root;
	std::string frame_prefix_template;
	AnimationConfig animation_config;
};

struct EntityEffectContentEntry
{
	EntityResourceIdentity entity;
	EffectDefinitionConfig effect_config;
};

struct EntityTextureContentEntry
{
	EntityResourceIdentity entity;
	std::filesystem::path texture_root;
	EntityTextureLayout layout;
};

struct EntityAudioContentEntry
{
	EntityResourceIdentity entity;
	std::filesystem::path audio_root;
	EntityAudioLayout layout;
};

struct EntityContentModule
{
	std::string name;
	std::string key_namespace;
	std::vector<EntityResourceIdentity> entities;
	std::vector<EntityAnimationContentEntry> animation_entries;
	std::vector<EntityEffectContentEntry> effect_entries;
	std::vector<EntityTextureContentEntry> texture_entries;
	std::vector<EntityAudioContentEntry> audio_entries;
};
}
