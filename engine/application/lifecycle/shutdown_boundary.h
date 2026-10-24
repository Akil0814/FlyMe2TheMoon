#pragma once

#include "../../tools/logger.h"
#include <exception>
#include <utility>

namespace elysia::application
{
// Cleanup failures must remain visible even after normal exit seals termination.
template <typename Callable>
bool run_shutdown_boundary(const char* phase, Callable&& callable) noexcept
{
    try
    {
        std::forward<Callable>(callable)();
        return true;
    }
    catch (const std::exception& error)
    {
        elysia::tools::Logger::instance()->error(phase,error.what());
    }
    catch (...)
    {
        elysia::tools::Logger::instance()->error(phase,"Unknown exception during shutdown");
    }
    return false;
}
}
