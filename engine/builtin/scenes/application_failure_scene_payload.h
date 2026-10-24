#pragma once

#include "../builtin_scene_keys.h"
#include "../../scene/routing/scene_route.h"
#include "../../scene/scene_boundary_failure.h"
#include "../../core/diagnostics/failure_diagnostic.h"
#include "../../loading/content_load_failure.h"

#include <string>
#include <utility>

namespace elysia::builtin
{
enum class ApplicationFailurePresentation
{
    StartupLoading,
    RuntimeFatal
};

enum class ApplicationFailureReason
{
    Startup,
    Config,
    Manifest,
    Plan,
    MissingResource,
    Texture,
    Atlas,
    Font,
    Audio,
    Animation,
    Effect,
    RuntimeFatal
};

struct ApplicationFailureScenePayload
{
    ApplicationFailurePresentation presentation =
        ApplicationFailurePresentation::RuntimeFatal;
    ApplicationFailureReason reason = ApplicationFailureReason::RuntimeFatal;
    std::string error_code;
    std::string category;
    elysia::core::FailureDiagnostic diagnostic;
};

[[nodiscard]] inline ApplicationFailureReason application_failure_reason(
    elysia::loading::ContentLoadError error) noexcept
{
    using enum elysia::loading::ContentLoadError;
    switch (error)
    {
    case Config: return ApplicationFailureReason::Config;
    case Manifest: return ApplicationFailureReason::Manifest;
    case Plan: return ApplicationFailureReason::Plan;
    case MissingResource: return ApplicationFailureReason::MissingResource;
    case Texture: return ApplicationFailureReason::Texture;
    case Atlas: return ApplicationFailureReason::Atlas;
    case Font: return ApplicationFailureReason::Font;
    case Audio: return ApplicationFailureReason::Audio;
    case Animation: return ApplicationFailureReason::Animation;
    case Effect: return ApplicationFailureReason::Effect;
    }
    return ApplicationFailureReason::Manifest;
}

[[nodiscard]] inline elysia::scene::SceneRoute make_application_failure_route(
    const elysia::loading::ContentLoadFailure& failure,
    std::string category = "startup")
{
    return elysia::scene::SceneRoute{
        .target = SceneKeys::ApplicationFailure,
        .payload = ApplicationFailureScenePayload{
            .presentation = ApplicationFailurePresentation::StartupLoading,
            .reason = application_failure_reason(failure.code),
            .error_code = std::string(failure.error_code()),
            .category = std::move(category),
            .diagnostic = failure.diagnostic
        },
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    };
}

[[nodiscard]] inline elysia::scene::SceneRoute make_application_failure_route(
    ApplicationFailurePresentation presentation,
    std::string category,
    elysia::core::FailureDiagnostic diagnostic)
{
    return elysia::scene::SceneRoute{
        .target = SceneKeys::ApplicationFailure,
        .payload = ApplicationFailureScenePayload{
            .presentation = presentation,
            .reason = presentation == ApplicationFailurePresentation::StartupLoading
                ? ApplicationFailureReason::Startup
                : ApplicationFailureReason::RuntimeFatal,
            .error_code = presentation == ApplicationFailurePresentation::StartupLoading
                ? "STARTUP-FAILURE" : "APPLICATION-FATAL",
            .category = std::move(category),
            .diagnostic = std::move(diagnostic)
        },
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    };
}
[[nodiscard]] inline elysia::scene::SceneRoute make_application_failure_route(
    const elysia::scene::SceneBoundaryFailure& failure)
{
    return make_application_failure_route(ApplicationFailurePresentation::RuntimeFatal,
        "scene",elysia::scene::to_failure_diagnostic(failure));
}

}
