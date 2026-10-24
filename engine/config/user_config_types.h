#pragma once

#include "user_config_data.h"
#include "../core/diagnostics/failure_diagnostic.h"
#include "../io/persistence/persistence_failure.h"

#include <optional>
#include <string>

namespace elysia::config
{
enum class UserConfigError
{
    InvalidValue,
    ChangeHandlerUnavailable,
    RuntimeApplyFailed,
    SaveFailed,
    LoadFailed
};

struct UserConfigFailure
{
    UserConfigError error = UserConfigError::InvalidValue;
    std::string setting_name;
    elysia::core::FailureDiagnostic diagnostic;
    std::optional<io::PersistenceFailureContext> persistence;
};

[[nodiscard]] inline UserConfigFailure make_user_config_failure(
    UserConfigError error,std::string setting,core::FailureDiagnostic diagnostic,
    std::optional<io::PersistenceFailureContext> persistence = {})
{
    return {error,std::move(setting),std::move(diagnostic),std::move(persistence)};
}

[[nodiscard]] inline UserConfigFailure make_user_config_failure(
    UserConfigError error,std::string setting,std::string message,
    std::source_location origin = std::source_location::current())
{
    auto diagnostic = core::make_failure_diagnostic(message,
        {core::make_failure_diagnostic_entry("user-config",setting,{},{},{},message,origin)},origin);
    return {error,std::move(setting),std::move(diagnostic)};
}

struct UserConfigCommitFailure
{
    UserConfigFailure cause;
    std::optional<UserConfigFailure> rollback_failure;
};

[[nodiscard]] inline core::FailureDiagnostic to_failure_diagnostic(const UserConfigCommitFailure& failure)
{
    auto diagnostic = failure.cause.diagnostic;
    if (failure.rollback_failure)
    {
        diagnostic.entries.push_back(core::make_failure_diagnostic_entry(
            "settings-runtime-rollback",failure.rollback_failure->setting_name,{},{},{},
            failure.rollback_failure->diagnostic.message,failure.rollback_failure->diagnostic.origin));
        io::append_failure_context(diagnostic,failure.rollback_failure->diagnostic);
    }
    return diagnostic;
}

struct UserConfigRuntimeState
{
    UserConfigData settings;
    bool restart_required = false;
};

enum class UserConfigApplyStatus
{
    Applied,
    PendingRestart
};

struct UserConfigLoadResult
{
    UserConfigData settings;
    std::optional<UserConfigFailure> warning;
    bool recovered = false;
    bool rebuilt = false;
};
}
