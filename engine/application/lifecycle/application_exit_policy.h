#pragma once

#include "../application_run_result.h"
#include "../../tools/termination_manager.h"

namespace elysia::application
{
enum class ApplicationExitDecision
{
    Continue,
    NormalExit,
    FaultExit
};

[[nodiscard]] inline ApplicationExitDecision resolve_application_exit(
    bool normal_exit_requested,
    elysia::tools::TerminationManager& termination_manager
) noexcept
{
    if (termination_manager.termination_requested())
        return ApplicationExitDecision::FaultExit;
    if (!normal_exit_requested)
        return ApplicationExitDecision::Continue;
    return termination_manager.seal_for_shutdown()
        ? ApplicationExitDecision::FaultExit
        : ApplicationExitDecision::NormalExit;
}

[[nodiscard]] inline ApplicationRunResult to_application_run_result(
    ApplicationExitDecision decision) noexcept
{
    return decision == ApplicationExitDecision::FaultExit
        ? ApplicationRunResult::FaultExit
        : ApplicationRunResult::NormalExit;
}
}
