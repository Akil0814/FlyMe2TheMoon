#pragma once

#include "../persistence_failure.h"
#include "../../../tools/logger.h"

#include <array>
#include <exception>
#include <expected>
#include <fstream>
#include <functional>
#include <string_view>

namespace elysia::io::detail
{
// Internal fault injection runs immediately before the real operation. Stores own no global hook.
struct PersistenceOperations
{
    std::function<std::error_code(std::string_view,const std::filesystem::path&)> before_operation;
};

struct PersistenceFailure
{
    core::FailureDiagnostic diagnostic;
    PersistenceFailureContext context;
};

inline core::FailureDiagnostic operation_failure(std::string_view stage,
    const std::filesystem::path& path,const std::error_code& error,
    std::source_location origin = std::source_location::current())
{
    const auto message = std::string(stage) + " failed: " + error.message();
    return core::make_failure_diagnostic(message,
        {core::make_failure_diagnostic_entry("persistence",std::string(stage),path,path,{},message,origin)},origin);
}

inline std::error_code before_operation(const PersistenceOperations& operations,
    std::string_view stage,const std::filesystem::path& path)
{
    return operations.before_operation ? operations.before_operation(stage,path) : std::error_code{};
}

inline std::expected<bool,core::FailureDiagnostic> file_exists(
    const PersistenceOperations& operations,const std::filesystem::path& path,
    std::string_view stage = "status")
{
    auto error = before_operation(operations,stage,path);
    bool exists = false;
    if (!error) exists = std::filesystem::exists(path,error);
    if (error) return std::unexpected(operation_failure(stage,path,error));
    return exists;
}

inline std::expected<void,core::FailureDiagnostic> rename_file(
    const PersistenceOperations& operations,const std::filesystem::path& from,
    const std::filesystem::path& to,std::string_view stage)
{
    auto error = before_operation(operations,stage,from);
    if (!error) std::filesystem::rename(from,to,error);
    if (error)
    {
        auto diagnostic = operation_failure(stage,from,error);
        diagnostic.entries.front().expected_path = to;
        return std::unexpected(std::move(diagnostic));
    }
    return {};
}

inline void observe_files(const PersistenceOperations& operations,PersistenceFailure& failure)
{
    for (auto* artifact : {&failure.context.primary,&failure.context.temporary,&failure.context.backup})
    {
        const auto status = file_exists(operations,artifact->path,"observe");
        artifact->state = !status ? PersistenceFileState::Unknown
            : (*status ? PersistenceFileState::Present : PersistenceFileState::Missing);
        if (!status) append_failure_context(failure.diagnostic,status.error());
        const auto state = artifact->state == PersistenceFileState::Unknown ? "unknown"
            : (artifact->state == PersistenceFileState::Present ? "present" : "missing");
        failure.diagnostic.entries.push_back(core::make_failure_diagnostic_entry(
            "persistence-state",{},artifact->path,{},{},state));
    }
    const auto state = failure.context.recovery;
    failure.diagnostic.entries.push_back(core::make_failure_diagnostic_entry(
        "persistence-recovery",failure.context.stage,{},{},{},
        state == PersistenceRecovery::NotRequired ? "not required"
        : state == PersistenceRecovery::Succeeded ? "succeeded"
        : state == PersistenceRecovery::Failed ? "failed" : "unknown"));
}

// Raw restoration results survive allocation failures while building a diagnostic.
struct RestoreAttempt
{
    std::error_code backup_error,primary_error,restore_error;
    std::exception_ptr backup_exception,primary_exception,restore_exception;
    bool backup_exists = false,primary_exists = false,can_restore = false;
    PersistenceRecovery recovery = PersistenceRecovery::Unknown;

    std::exception_ptr exception() const noexcept
    {
        return backup_exception ? backup_exception : primary_exception ? primary_exception : restore_exception;
    }
};

inline RestoreAttempt restore_primary(const PersistenceOperations& operations,
    const PersistenceFailureContext& context) noexcept
{
    RestoreAttempt attempt;
    try
    {
        attempt.backup_error = before_operation(operations,"restore-backup-status",context.backup.path);
        if (!attempt.backup_error) attempt.backup_exists = std::filesystem::exists(context.backup.path,attempt.backup_error);
    }
    catch (...) { attempt.backup_exception = std::current_exception(); }
    try
    {
        attempt.primary_error = before_operation(operations,"restore-primary-status",context.primary.path);
        if (!attempt.primary_error) attempt.primary_exists = std::filesystem::exists(context.primary.path,attempt.primary_error);
    }
    catch (...) { attempt.primary_exception = std::current_exception(); }
    attempt.can_restore = !attempt.backup_error && !attempt.primary_error
        && !attempt.backup_exception && !attempt.primary_exception
        && attempt.backup_exists && !attempt.primary_exists;
    if (attempt.can_restore)
    {
        try
        {
            attempt.restore_error = before_operation(operations,"restore-primary",context.backup.path);
            if (!attempt.restore_error) std::filesystem::rename(context.backup.path,context.primary.path,attempt.restore_error);
        }
        catch (...) { attempt.restore_exception = std::current_exception(); }
    }
    attempt.recovery = attempt.exception() || attempt.backup_error || attempt.primary_error
        ? PersistenceRecovery::Unknown
        : (attempt.can_restore && !attempt.restore_error ? PersistenceRecovery::Succeeded : PersistenceRecovery::Failed);
    return attempt;
}

inline core::FailureDiagnostic exception_diagnostic(const std::exception_ptr& exception,
    std::source_location capture = std::source_location::current())
{
    std::string message = "Unknown non-standard persistence exception.";
    try { std::rethrow_exception(exception); }
    catch (const std::exception& error) { message = error.what(); }
    catch (...) {}
    return core::make_failure_diagnostic(std::move(message),{},capture);
}

inline void append_restore_attempt(core::FailureDiagnostic& diagnostic,
    const PersistenceFailureContext& context,const RestoreAttempt& attempt)
{
    const auto append = [&](std::string_view stage,const std::filesystem::path& path,
        const std::error_code& error,const std::exception_ptr& exception)
    {
        if (error) append_failure_context(diagnostic,operation_failure(stage,path,error));
        if (exception)
        {
            auto captured = exception_diagnostic(exception);
            captured.entries.push_back(core::make_failure_diagnostic_entry(
                "persistence",std::string(stage),path,{},{},captured.message,captured.origin));
            append_failure_context(diagnostic,captured);
        }
    };
    append("restore-backup-status",context.backup.path,attempt.backup_error,attempt.backup_exception);
    append("restore-primary-status",context.primary.path,attempt.primary_error,attempt.primary_exception);
    append("restore-primary",context.backup.path,attempt.restore_error,attempt.restore_exception);
    if (!attempt.can_restore && !attempt.backup_error && !attempt.primary_error && !attempt.exception())
        append_failure_context(diagnostic,core::make_failure_diagnostic("Restore refused: backup missing or primary already exists."));
}

// Concrete temporary/backup replacement used by the two JSON stores, not a general transaction API.
template<typename Verify,typename Inspect>
std::expected<void,PersistenceFailure> replace_json_file(
    const std::filesystem::path& primary,std::string_view content,
    const PersistenceOperations& operations,Verify&& verify,Inspect&& inspect)
{
    PersistenceFailure failure{{},{.primary = {primary},
        .temporary = {primary.string()+".tmp"},.backup = {primary.string()+".bak"}}};
    auto& context = failure.context;
    context.stage.reserve(32);
    bool primary_moved = false,has_failure = false;
    std::string_view current_stage = "prepare";
    // At most one normal restoration and one protected retry in the exception boundary.
    std::array<RestoreAttempt,2> attempts;
    std::size_t attempt_count = 0,reported_attempts = 0;
    const auto recover = [&]() -> const RestoreAttempt&
    {
        auto& attempt = attempts[attempt_count++];
        attempt = restore_primary(operations,context);
        context.recovery = attempt.recovery;
        if (attempt.recovery == PersistenceRecovery::Succeeded) primary_moved = false;
        return attempt;
    };
    const auto append_pending = [&](std::size_t through)
    {
        // Publish the assembled report only after all allocations succeed.
        auto diagnostic = failure.diagnostic;
        for (auto index = reported_attempts; index < through; ++index)
            append_restore_attempt(diagnostic,context,attempts[index]);
        failure.diagnostic = std::move(diagnostic);
        reported_attempts = through;
    };
    const auto failed = [&](std::string_view stage,core::FailureDiagnostic diagnostic)
        -> std::expected<void,PersistenceFailure>
    {
        failure.diagnostic = std::move(diagnostic);
        has_failure = true;
        context.stage = stage;
        if (primary_moved)
        {
            const auto& attempt = recover();
            if (auto exception = attempt.exception()) std::rethrow_exception(exception);
        }
        append_pending(attempt_count);
        observe_files(operations,failure);
        return std::unexpected(std::move(failure));
    };
    try
    {
        if (!primary.parent_path().empty())
        {
            current_stage = "create-directory";
            auto error = before_operation(operations,"create-directory",primary.parent_path());
            if (!error) std::filesystem::create_directories(primary.parent_path(),error);
            if (error) return failed("create-directory",operation_failure("create-directory",primary.parent_path(),error));
        }
        current_stage = "primary-status";
        auto existing_primary = file_exists(operations,primary,"primary-status");
        if (!existing_primary) return failed(current_stage,existing_primary.error());
        if (!*existing_primary)
        {
            current_stage = "temporary-status";
            auto temporary = file_exists(operations,context.temporary.path,current_stage);
            if (!temporary) return failed(current_stage,temporary.error());
            if (*temporary)
            {
                current_stage = "inspect-temporary";
                if (auto error = before_operation(operations,current_stage,context.temporary.path))
                    return failed(current_stage,operation_failure(current_stage,context.temporary.path,error));
                auto valid = inspect(context.temporary.path);
                if (!valid) return failed(current_stage,valid.error());
                if (*valid)
                {
                    current_stage = "promote-temporary";
                    if (auto promoted = rename_file(operations,context.temporary.path,primary,current_stage); !promoted)
                        return failed(current_stage,promoted.error());
                }
            }
        }
        std::ofstream output;
        for (const auto stage : {"open-temporary","write-temporary","flush-temporary","close-temporary"})
        {
            current_stage = stage;
            auto error = before_operation(operations,stage,context.temporary.path);
            if (!error)
            {
                if (std::string_view(stage) == "open-temporary") output.open(context.temporary.path,std::ios::binary|std::ios::trunc);
                else if (std::string_view(stage) == "write-temporary") output.write(content.data(),static_cast<std::streamsize>(content.size()));
                else if (std::string_view(stage) == "flush-temporary") output.flush();
                else output.close();
                if (!output.good()) error = std::make_error_code(std::errc::io_error);
            }
            if (error)
            {
                auto diagnostic = operation_failure(stage,context.temporary.path,error);
                if (output.is_open())
                {
                    auto close_error = before_operation(operations,"cleanup-close-temporary",context.temporary.path);
                    output.clear();
                    output.close();
                    if (!close_error && output.fail()) close_error = std::make_error_code(std::errc::io_error);
                    if (close_error) append_failure_context(diagnostic,operation_failure("cleanup-close-temporary",context.temporary.path,close_error));
                }
                return failed(stage,std::move(diagnostic));
            }
        }
        current_stage = "verify-temporary";
        if (auto error = before_operation(operations,"verify-temporary",context.temporary.path))
            return failed("verify-temporary",operation_failure("verify-temporary",context.temporary.path,error));
        if (auto verified = verify(context.temporary.path); !verified)
            return failed("verify-temporary",verified.error());
        current_stage = "primary-status";
        const auto exists = file_exists(operations,primary,"primary-status");
        if (!exists) return failed("primary-status",exists.error());
        current_stage = "backup-status";
        const auto backup = file_exists(operations,context.backup.path,"backup-status");
        if (!backup) return failed("backup-status",backup.error());
        if (*exists)
        {
            if (*backup)
            {
                current_stage = "remove-backup";
                auto error = before_operation(operations,"remove-backup",context.backup.path);
                if (!error) std::filesystem::remove(context.backup.path,error);
                if (error) return failed("remove-backup",operation_failure("remove-backup",context.backup.path,error));
            }
            current_stage = "backup-primary";
            if (auto moved = rename_file(operations,primary,context.backup.path,"backup-primary"); !moved)
                return failed("backup-primary",moved.error());
            primary_moved = true;
        }
        current_stage = "publish-primary";
        if (auto published = rename_file(operations,context.temporary.path,primary,"publish-primary"); !published)
            return failed("publish-primary",published.error());
        primary_moved = false;
        return {};
    }
    catch (...)
    {
        const auto unexpected = std::current_exception();
        // Raw cleanup runs before any report allocation, and cannot replace the exception.
        const auto preceding_attempts = attempt_count;
        if (primary_moved) (void)recover();
        try
        {
            context.stage = current_stage;
            if (!has_failure) failure.diagnostic = exception_diagnostic(unexpected);
            append_pending(preceding_attempts);
            if (has_failure) append_failure_context(failure.diagnostic,exception_diagnostic(unexpected));
            append_pending(attempt_count);
            // A diagnostic/status failure must not prevent the already collected report being logged.
            try { observe_files(operations,failure); }
            catch (...)
            {
                append_failure_context(failure.diagnostic,exception_diagnostic(std::current_exception()));
            }
            ELYSIA_LOG_ERROR("persistence_cleanup",core::format_failure_diagnostic(failure.diagnostic,"PERSISTENCE-CLEANUP","persistence"));
        }
        catch (...)
        {
            try { ELYSIA_LOG_ERROR("persistence_cleanup","Persistence cleanup report unavailable; original exception propagated."); }
            catch (...) {}
        }
        throw;
    }
}
}

