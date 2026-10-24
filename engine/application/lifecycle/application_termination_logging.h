#pragma once

#include "application_exit_policy.h"
#include "../../tools/logger.h"
#include "../../tools/termination_manager.h"

#include <optional>
#include <source_location>

namespace elysia::application
{
inline void log_published_termination(
    const std::optional<elysia::tools::TerminationInfo>& info,
    std::source_location location = std::source_location::current()
) noexcept
{
    auto* logger = elysia::tools::Logger::instance();
    if (!info)
    {
        logger->error("termination",
            "Application termination requested without diagnostic information",location);
        logger->terminating("application",
            "Application terminating without diagnostic information",location);
        return;
    }

    logger->error(info->category,info->message,info->location);
    if (info->category_truncated || info->message_truncated)
        logger->error("termination","Termination record was truncated; consult the original failure log",info->location);
    const char* reason = info->reason == elysia::tools::TerminationReason::UnhandledException
        ? "Application terminating after an unhandled exception"
        : "Application terminating after a fatal runtime failure";
    logger->terminating("application",reason,location);
}

inline void log_fault_exit_if_needed(
    ApplicationExitDecision decision,
    const std::optional<elysia::tools::TerminationInfo>& info,
    std::source_location location = std::source_location::current()
) noexcept
{
    if (decision == ApplicationExitDecision::FaultExit)
        log_published_termination(info,location);
}
}
