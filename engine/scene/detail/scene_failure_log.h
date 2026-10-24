#pragma once

#include "../scene_boundary_failure.h"
#include "../../tools/logger.h"

#include <exception>
#include <string_view>

namespace elysia::scene::detail
{
inline void log_scene_failure(const SceneBoundaryFailure& failure) noexcept
{
    elysia::tools::Logger::instance()->log_stream(elysia::tools::LogLevel::Error,"scene",
        [&](std::ostream& output) {
            output << elysia::core::format_failure_diagnostic(
                to_failure_diagnostic(failure),"APPLICATION-FATAL","scene");
        },failure.diagnostic.origin);
}

// Called from a catch handler after the primary failure has already been saved.
inline void log_cleanup_exception(std::string_view stage) noexcept
{
    try
    {
        if (const auto error = std::current_exception())
            std::rethrow_exception(error);
    }
    catch (const std::exception& error)
    {
        ELYSIA_LOG_ERROR("scene_cleanup", stage << ": " << error.what());
    }
    catch (...)
    {
        ELYSIA_LOG_ERROR("scene_cleanup", stage << ": Unknown cleanup exception.");
    }
}
} // namespace elysia::scene::detail
