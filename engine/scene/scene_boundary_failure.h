#pragma once

#include "routing/scene_key.h"
#include "../core/diagnostics/failure_diagnostic.h"

#include <stdexcept>
#include <string>

namespace elysia::scene
{
enum class SceneBoundary
{
    Enter,
    Exit,
    Reset,
    Attach,
    Detach,
    Input,
    Update,
    Render,
    ObjectRegistration,
    ObjectRemoval
};

class SceneBoundaryTagged
{
public:
    explicit SceneBoundaryTagged(SceneBoundary boundary,
        std::source_location origin = std::source_location::current()) noexcept
        : _boundary(boundary),_origin(origin) {}
    virtual ~SceneBoundaryTagged() = default;

    [[nodiscard]] SceneBoundary scene_boundary() const noexcept { return _boundary; }
    [[nodiscard]] std::source_location origin() const noexcept { return _origin; }

private:
    SceneBoundary _boundary;
    std::source_location _origin;
};

class SceneBoundaryLogicError final : public std::logic_error,
                                      public SceneBoundaryTagged
{
public:
    SceneBoundaryLogicError(SceneBoundary boundary, const std::string& message,
        std::source_location origin = std::source_location::current())
        : std::logic_error(message), SceneBoundaryTagged(boundary,origin)
    {
    }
};

class SceneBoundaryRuntimeError final : public std::runtime_error,
                                        public SceneBoundaryTagged
{
public:
    SceneBoundaryRuntimeError(SceneBoundary boundary, const std::string& message,
        std::source_location origin = std::source_location::current())
        : std::runtime_error(message), SceneBoundaryTagged(boundary,origin)
    {
    }
};

struct SceneBoundaryFailure
{
    SceneKey scene = SceneKeys::Invalid;
    SceneBoundary boundary = SceneBoundary::Update;
    elysia::core::FailureDiagnostic diagnostic;
    // A cleanup attempted after the primary failure also failed.
    bool cleanup_failed = false;
};

[[nodiscard]] inline std::string_view scene_boundary_name(SceneBoundary boundary) noexcept
{
    switch (boundary)
    {
    case SceneBoundary::Enter: return "Enter";
    case SceneBoundary::Exit: return "Exit";
    case SceneBoundary::Reset: return "Reset";
    case SceneBoundary::Attach: return "Attach";
    case SceneBoundary::Detach: return "Detach";
    case SceneBoundary::Input: return "Input";
    case SceneBoundary::Update: return "Update";
    case SceneBoundary::Render: return "Render";
    case SceneBoundary::ObjectRegistration: return "ObjectRegistration";
    case SceneBoundary::ObjectRemoval: return "ObjectRemoval";
    }
    return "Unknown";
}

[[nodiscard]] inline SceneBoundaryFailure make_scene_boundary_failure(
    SceneKey scene,SceneBoundary boundary,std::string message,
    std::source_location origin = std::source_location::current())
{
    return {scene,boundary,elysia::core::make_failure_diagnostic(std::move(message),std::vector<elysia::core::FailureDiagnosticEntry>{},origin)};
}

[[nodiscard]] inline elysia::core::FailureDiagnostic to_failure_diagnostic(const SceneBoundaryFailure& failure)
{
    auto diagnostic = failure.diagnostic;
    diagnostic.entries.insert(diagnostic.entries.begin(),elysia::core::make_failure_diagnostic_entry(
        "scene",std::to_string(failure.scene),{},{},{},
        std::string(scene_boundary_name(failure.boundary)),diagnostic.origin));
    return diagnostic;
}
} // namespace elysia::scene
