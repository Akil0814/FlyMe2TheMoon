#include "application_window_settings.h"
#include <SDL3/SDL.h>

namespace elysia::application::detail
{
namespace
{
WindowOperationResult checked(bool success,const char* operation,
    std::source_location origin = std::source_location::current())
{
    if (success) return {};
    return std::unexpected(elysia::core::make_failure_diagnostic(
        std::string(operation) + ": " + SDL_GetError(),{},{},origin));
}

void append_restore_failure(elysia::core::FailureDiagnostic& original,
    const elysia::core::FailureDiagnostic& restored)
{
    original.entries.push_back(elysia::core::make_failure_diagnostic_entry(
        "window-rollback",{},{},{},{},restored.message,restored.origin));
    original.entries.insert(original.entries.end(),restored.entries.begin(),restored.entries.end());
}
}

ApplicationWindowOperations make_sdl_window_operations(SDL_Window* window)
{
    return {
        .set_fullscreen = [window](bool enabled) { return checked(SDL_SetWindowFullscreen(window,enabled),"SDL_SetWindowFullscreen"); },
        .set_size = [window](int width,int height) { return checked(SDL_SetWindowSize(window,width,height),"SDL_SetWindowSize"); },
        .set_position = [window](int x,int y) { return checked(SDL_SetWindowPosition(window,x,y),"SDL_SetWindowPosition"); },
        .get_mode = [window]() -> std::expected<elysia::config::WindowMode,elysia::core::FailureDiagnostic> {
            if (!window) return std::unexpected(elysia::core::make_failure_diagnostic("Application window is unavailable."));
            return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)
                ? elysia::config::WindowMode::BorderlessFullscreen : elysia::config::WindowMode::Windowed;
        },
        .get_size = [window]() -> std::expected<elysia::config::WindowSize,elysia::core::FailureDiagnostic> {
            elysia::config::WindowSize size;
            if (auto result = checked(SDL_GetWindowSize(window,&size.width,&size.height),"SDL_GetWindowSize"); !result)
                return std::unexpected(std::move(result.error()));
            return size;
        },
        .get_position = [window]() -> std::expected<WindowPosition,elysia::core::FailureDiagnostic> {
            WindowPosition position{};
            if (auto result = checked(SDL_GetWindowPosition(window,&position.x,&position.y),"SDL_GetWindowPosition"); !result)
                return std::unexpected(std::move(result.error()));
            return position;
        }
    };
}

WindowOperationResult validate_window_settings(
    const elysia::config::WindowSettings& settings,const ApplicationWindowOperations& operations)
{
    if (settings.windowed_size.width <= 0 || settings.windowed_size.height <= 0)
        return std::unexpected(elysia::core::make_failure_diagnostic("Windowed size must be positive."));
    if (settings.mode != elysia::config::WindowMode::Windowed
        && settings.mode != elysia::config::WindowMode::BorderlessFullscreen)
        return std::unexpected(elysia::core::make_failure_diagnostic("Unknown window mode."));
    if (!operations.set_fullscreen || !operations.set_size || !operations.set_position)
        return std::unexpected(elysia::core::make_failure_diagnostic("Window operations are unavailable."));
    return {};
}

std::expected<ApplicationWindowSnapshot,elysia::core::FailureDiagnostic> capture_window_snapshot(
    const elysia::config::WindowSettings& remembered,const ApplicationWindowOperations& operations)
{
    if (!operations.get_mode || !operations.get_size || !operations.get_position)
        return std::unexpected(elysia::core::make_failure_diagnostic("Window snapshot operations are unavailable."));
    auto position = operations.get_position();
    if (!position) return std::unexpected(std::move(position.error()));
    auto mode = operations.get_mode();
    if (!mode) return std::unexpected(std::move(mode.error()));
    ApplicationWindowSnapshot snapshot{remembered,position->x,position->y};
    snapshot.settings.mode = *mode;
    if (*mode == elysia::config::WindowMode::Windowed)
    {
        auto size = operations.get_size();
        if (!size) return std::unexpected(std::move(size.error()));
        snapshot.settings.windowed_size = *size;
    }
    return snapshot;
}

WindowOperationResult restore_window_snapshot(
    const ApplicationWindowSnapshot& previous,const ApplicationWindowOperations& operations)
{
    if (auto valid = validate_window_settings(previous.settings,operations); !valid) return valid;
    WindowOperationResult result;
    const auto restore = [&](WindowOperationResult restored) {
        if (restored) return;
        if (result) result = std::move(restored);
        else append_restore_failure(result.error(),restored.error());
    };
    // Restore physical state directly and continue after every failed step.
    restore(operations.set_fullscreen(false));
    restore(operations.set_size(previous.settings.windowed_size.width,previous.settings.windowed_size.height));
    restore(operations.set_position(previous.x,previous.y));
    if (previous.settings.mode == elysia::config::WindowMode::BorderlessFullscreen)
        restore(operations.set_fullscreen(true));
    return result;
}

WindowOperationResult apply_window_settings(
    const elysia::config::WindowSettings& settings,const ApplicationWindowOperations& operations)
{
    if (auto valid = validate_window_settings(settings,operations); !valid) return valid;
    if (settings.mode == elysia::config::WindowMode::BorderlessFullscreen)
        return operations.set_fullscreen(true);
    if (auto result = operations.set_fullscreen(false); !result) return result;
    if (auto result = operations.set_size(settings.windowed_size.width,settings.windowed_size.height); !result) return result;
    return operations.set_position(SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);
}

WindowOperationResult apply_window_settings_transactional(
    const elysia::config::WindowSettings& settings,const ApplicationWindowSnapshot& previous,
    const ApplicationWindowOperations& operations)
{
    if (auto valid = validate_window_settings(settings,operations); !valid) return valid;
    auto result = apply_window_settings(settings,operations);
    if (!result)
        if (auto restored = restore_window_snapshot(previous,operations); !restored)
            append_restore_failure(result.error(),restored.error());
    return result;
}

std::expected<std::optional<elysia::core::FailureDiagnostic>,elysia::core::FailureDiagnostic>
apply_startup_window_settings(const elysia::config::WindowSettings& settings,const ApplicationWindowOperations& operations)
{
    if (auto valid = validate_window_settings(settings,operations); !valid)
        return std::unexpected(std::move(valid.error()));
    if (settings.mode == elysia::config::WindowMode::Windowed) return std::nullopt;
    auto result = operations.set_fullscreen(true);
    if (result) return std::nullopt;
    const ApplicationWindowSnapshot fallback{
        {elysia::config::WindowMode::Windowed,settings.windowed_size},SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED};
    if (auto restored = restore_window_snapshot(fallback,operations); !restored)
    {
        append_restore_failure(result.error(),restored.error());
        return std::unexpected(std::move(result.error()));
    }
    return std::move(result.error());
}
}
