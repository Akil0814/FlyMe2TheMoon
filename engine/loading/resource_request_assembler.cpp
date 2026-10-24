#include "resource_request_assembler.h"

#include "content_manifest_pipeline.h"
#include "resource_load_plan.h"
#include "resource_load_plan_preflight.h"
#include "resource_load_plan_validator.h"
#include "../io/path/path_manager.h"
#include "../resources/pipeline/resource_request_builder.h"

#include <optional>

namespace elysia::loading
{
namespace
{
using BuildResult = std::expected<void,elysia::resources::ResourceRequestBuildFailure>;

std::optional<ContentLoadFailure> consume_build_result(
    BuildResult result,
    std::vector<elysia::core::FailureDiagnosticEntry>& missing)
{
    if (result) return std::nullopt;
    auto failure = std::move(result.error());
    if (failure.code == elysia::resources::ResourceRequestBuildError::MissingSource)
    {
        missing.insert(missing.end(),
            std::make_move_iterator(failure.diagnostic.entries.begin()),
            std::make_move_iterator(failure.diagnostic.entries.end()));
        return std::nullopt;
    }
    return make_content_load_failure(ContentLoadError::Plan,std::move(failure.diagnostic));
}
}

std::expected<ResourceLoadPlan,ContentLoadFailure> ResourceRequestAssembler::assemble(
    const ContentManifestResult& config,
    std::span<const int> project_font_point_sizes) const
{
    ResourceLoadPlan plan;
    elysia::resources::ResourceRequestBuilder builder;
    std::vector<elysia::core::FailureDiagnosticEntry> missing;
    const auto* paths = elysia::io::PathManager::instance();
    if (!paths || !paths->is_initialized())
        return std::unexpected(make_content_load_failure(
            ContentLoadError::Plan,
            "Resource request assembly failed: path manager is not initialized."));
    const auto textures_root = paths->textures();
    const auto consume = [&missing,paths](BuildResult result)
    {
        auto failure = consume_build_result(std::move(result),missing);
        if (failure)
            elysia::core::normalize_failure_diagnostic_paths(
                failure->diagnostic,paths->root());
        return failure;
    };

    if (auto failure = consume(builder.append_animation_manifest_requests(
            config.animation_manifest,textures_root,
            plan.atlas_build_requests(),plan.animation_build_requests())))
        return std::unexpected(std::move(*failure));
    for (const auto& [name,module] : config.additional_modules)
        for (const auto& entry : module.animation_entries)
            if (auto failure = consume(builder.append_entity_animation_requests(
                    module.key_namespace,entry,plan.atlas_build_requests(),
                    plan.animation_build_requests())))
                return std::unexpected(std::move(*failure));

    if (auto failure = consume(builder.append_animation_effect_manifest_requests(
            config.animation_effect_manifest,plan.animation_effect_build_requests())))
        return std::unexpected(std::move(*failure));
    for (const auto& [name,module] : config.additional_modules)
        for (const auto& entry : module.effect_entries)
            if (auto failure = consume(builder.append_entity_effect_requests(
                    module.key_namespace,entry,plan.animation_build_requests(),
                    plan.animation_effect_build_requests())))
                return std::unexpected(std::move(*failure));

    if (auto failure = consume(builder.append_texture_manifest_requests(
            config.texture_manifest,textures_root,plan.texture_requests())))
        return std::unexpected(std::move(*failure));
    for (const auto& [name,module] : config.additional_modules)
        for (const auto& entry : module.texture_entries)
            if (auto failure = consume(builder.append_entity_texture_requests(
                    module.key_namespace,entry,plan.texture_requests())))
                return std::unexpected(std::move(*failure));

    if (auto failure = consume(builder.append_font_requests(
            config.font_manifest,paths->fonts(),project_font_point_sizes,plan.font_requests())))
        return std::unexpected(std::move(*failure));
    if (auto failure = consume(builder.append_audio_requests(
            config.audio_manifest,paths->audio(),plan.sound_requests(),plan.music_requests())))
        return std::unexpected(std::move(*failure));
    for (const auto& [name,module] : config.additional_modules)
        for (const auto& entry : module.audio_entries)
            if (auto failure = consume(builder.append_entity_audio_requests(
                    module.key_namespace,entry,plan.sound_requests())))
                return std::unexpected(std::move(*failure));

    auto validation = ResourceLoadPlanValidator{}.validate(plan);
    if (!validation)
        return std::unexpected(make_content_load_failure(
            ContentLoadError::Plan,validation.error().diagnostic()));

    auto preflight = ResourceLoadPlanPreflight{}.validate(plan,std::move(missing));
    if (!preflight)
        return std::unexpected(std::move(preflight.error()));
    return plan;
}
}
