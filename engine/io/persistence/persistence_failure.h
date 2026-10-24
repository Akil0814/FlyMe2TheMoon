#pragma once

#include "../../core/diagnostics/failure_diagnostic.h"

#include <optional>

namespace elysia::io
{
enum class PersistenceRecovery { NotRequired, Succeeded, Failed, Unknown };
enum class PersistenceFileState { Unknown, Missing, Present };

struct PersistenceArtifact
{
    std::filesystem::path path;
    PersistenceFileState state = PersistenceFileState::Unknown;
};

struct PersistenceFailureContext
{
    std::string stage;
    PersistenceRecovery recovery = PersistenceRecovery::NotRequired;
    PersistenceArtifact primary;
    PersistenceArtifact temporary;
    PersistenceArtifact backup;
};

inline void append_failure_context(core::FailureDiagnostic& primary,
    const core::FailureDiagnostic& additional)
{
    // Keep the original message and origin; secondary failures remain independent entries.
    primary.entries.push_back(core::make_failure_diagnostic_entry(
        "failure-context",{},{},{},{},additional.message,additional.origin));
    primary.entries.insert(primary.entries.end(),additional.entries.begin(),additional.entries.end());
}
}
