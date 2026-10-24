#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include "../io/persistence/persistence_failure.h"

namespace elysia::save
{
enum class SaveError
{
    NotInitialized,
    AlreadyInitialized,
    InvalidSaveName,
    InvalidKey,
    InvalidValue,
    NotOpen,
    AlreadyOpen,
    NotFound,
    AlreadyExists,
    KeyNotFound,
    TypeMismatch,
    InvalidDocument,
    UnsupportedFormatVersion,
    IoFailure,
    DirtySave
};

struct SaveFailure
{
    SaveError error = SaveError::InvalidDocument;
    std::string save_name;
    std::string key;
    core::FailureDiagnostic diagnostic;
    std::optional<io::PersistenceFailureContext> persistence;
};

[[nodiscard]] inline SaveFailure make_save_failure(
    SaveError error,std::string save_name,std::string key,core::FailureDiagnostic diagnostic,
    std::optional<io::PersistenceFailureContext> persistence = {})
{
    return {error,std::move(save_name),std::move(key),std::move(diagnostic),std::move(persistence)};
}

[[nodiscard]] inline SaveFailure make_save_failure(
    SaveError error,std::string save_name,std::string key,std::string message,
    std::source_location origin = std::source_location::current())
{
    auto diagnostic = core::make_failure_diagnostic(message,
        {core::make_failure_diagnostic_entry("save",save_name,{},{},{},message,origin)},origin);
    if (!key.empty()) diagnostic.entries.push_back(core::make_failure_diagnostic_entry(
        "save-key",key,{},{},{},message,origin));
    return {error,std::move(save_name),std::move(key),std::move(diagnostic)};
}

inline void bind_save_name(SaveFailure& failure,std::string_view name)
{
    failure.save_name = name;
    for (auto& entry : failure.diagnostic.entries)
        if (entry.subject_type == "save" && entry.subject_key.empty()) entry.subject_key = name;
}

struct SaveOpenResult
{
    bool recovered = false;
    std::optional<SaveFailure> warning;
};

enum class SaveCreateMode
{
    FailIfExists,
    OverwriteExisting
};

enum class SaveClosePolicy
{
    RejectIfDirty,
    DiscardChanges
};
}
