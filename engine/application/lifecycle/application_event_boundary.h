#pragma once

#include "../../tools/termination_manager.h"
#include "../../tools/logger.h"
#include "../../core/render/render_failure.h"

#include <exception>
#include <utility>

namespace elysia::application
{
template <typename Callable>
bool run_event_boundary(const char* phase,Callable&& callable) noexcept
{
    try
    {
        std::forward<Callable>(callable)();
        return true;
    }
    catch (const elysia::core::RenderBackendError& error)
    {
        const auto& failure = error.failure();
        try
        {
            const std::string report = elysia::core::format_failure_diagnostic(
                failure.diagnostic,"RENDER-BACKEND","render");
            elysia::tools::Logger::instance()->error("render",report,failure.diagnostic.origin);
            elysia::tools::TerminationManager::instance()->request_termination(
                elysia::tools::TerminationReason::FatalRuntimeFailure,"render",report,failure.diagnostic.origin);
        }
        catch (...)
        {
            elysia::tools::Logger::instance()->error("render",error.what(),failure.diagnostic.origin);
            elysia::tools::TerminationManager::instance()->request_termination(
                elysia::tools::TerminationReason::FatalRuntimeFailure,"render",error.what(),failure.diagnostic.origin);
        }
    }
    catch (const std::exception& error)
    {
        const auto origin = std::source_location::current();
        elysia::tools::Logger::instance()->error(phase,error.what(),origin);
        elysia::tools::TerminationManager::instance()->request_termination(
            elysia::tools::TerminationReason::UnhandledException,phase,error.what(),origin);
    }
    catch (...)
    {
        const auto origin = std::source_location::current();
        elysia::tools::Logger::instance()->error(phase,"Unhandled non-standard exception",origin);
        elysia::tools::TerminationManager::instance()->request_termination(
            elysia::tools::TerminationReason::UnhandledException,phase,
            "Unhandled non-standard exception",origin);
    }
    return false;
}
}
