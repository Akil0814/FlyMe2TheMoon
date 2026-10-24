#include "content_manifest_pipeline.h"

#include "../io/loaders/animation_manifest_loader.h"
#include "../io/loaders/audio_manifest_loader.h"
#include "../io/loaders/animation_effect_manifest_loader.h"
#include "../io/loaders/fonts_manifest_loader.h"
#include "../io/loaders/texture_manifest_loader.h"
#include "../config/content/config_load_pipeline.h"
#include "animated_entity_content_loader.h"
#include "../io/path/path_manager.h"

namespace elysia::loading
{
namespace
{
ContentLoadFailure manifest_failure(
    ContentLoadError code,elysia::core::FailureDiagnostic diagnostic)
{
    if (const auto* paths = elysia::io::PathManager::instance();
        paths && paths->is_initialized())
        elysia::core::normalize_failure_diagnostic_paths(diagnostic,paths->root());
    return make_content_load_failure(code,std::move(diagnostic));
}
}

std::expected<ContentManifestResult,ContentLoadFailure> ContentManifestPipeline::load(
	const elysia::io::ContentRegistry& content_registry)
{
	ContentManifestResult result;

	const elysia::io::CoreManifestPaths& manifest_paths = content_registry.required;
	const auto config_snapshot = elysia::config::ConfigLoadPipeline{}.load(manifest_paths.configs);
	if (!config_snapshot)
	{
		const auto& config_failure = config_snapshot.error();
        auto diagnostic = config_failure.diagnostic();
        diagnostic.message = "Content manifest pipeline failed: game config load failed: "
            + config_failure.message;
		return std::unexpected(manifest_failure(
            ContentLoadError::Config,std::move(diagnostic)));
	}
	result.config_snapshot = *config_snapshot;

	elysia::io::FontsManifestLoader fonts_manifest_loader;
	auto fonts = fonts_manifest_loader.load(manifest_paths.fonts);
	if (!fonts)
	{
		return std::unexpected(manifest_failure(
			ContentLoadError::Manifest,std::move(fonts.error().diagnostic)));
	}
	result.font_manifest = std::move(*fonts);

	elysia::io::AudioManifestLoader audio_manifest_loader;
	auto audio = audio_manifest_loader.load(manifest_paths.audio);
	if (!audio)
	{
		return std::unexpected(manifest_failure(
			ContentLoadError::Manifest,std::move(audio.error().diagnostic)));
	}
	result.audio_manifest = std::move(*audio);

	elysia::io::TextureManifestLoader texture_manifest_loader;
	auto textures = texture_manifest_loader.load(manifest_paths.textures);
	if (!textures)
	{
		return std::unexpected(manifest_failure(
			ContentLoadError::Manifest,std::move(textures.error().diagnostic)));
	}
	result.texture_manifest = std::move(*textures);

	elysia::io::AnimationManifestLoader animation_manifest_loader;
	auto animations = animation_manifest_loader.load(manifest_paths.animations);
	if (!animations)
	{
		return std::unexpected(manifest_failure(
			ContentLoadError::Manifest,std::move(animations.error().diagnostic)));
	}
	result.animation_manifest = std::move(*animations);

	elysia::io::AnimationEffectManifestLoader animation_effect_manifest_loader;
	auto effects = animation_effect_manifest_loader.load(manifest_paths.effects);
	if (!effects)
	{
		return std::unexpected(manifest_failure(
			ContentLoadError::Manifest,std::move(effects.error().diagnostic)));
	}
	result.animation_effect_manifest = std::move(*effects);


	AnimatedEntityContentLoader module_loader;
	for (const auto& [module_name, module_manifest_path] : content_registry.additional_module_manifests)
	{
		auto module = module_loader.load(module_name,module_manifest_path);
		if (!module)
			return std::unexpected(std::move(module.error()));
		result.additional_modules.emplace(module_name,std::move(*module));
	}

	return result;
}

}
