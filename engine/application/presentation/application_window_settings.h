#pragma once

#include "../../config/user_config_data.h"
#include "../../core/diagnostics/failure_diagnostic.h"
#include <expected>
#include <functional>
#include <optional>

struct SDL_Window;

namespace elysia::application::detail
{
using WindowOperationResult = std::expected<void,elysia::core::FailureDiagnostic>;
struct WindowPosition { int x; int y; };
struct ApplicationWindowOperations
{
    std::function<WindowOperationResult(bool)> set_fullscreen;
    std::function<WindowOperationResult(int,int)> set_size;
    std::function<WindowOperationResult(int,int)> set_position;
    std::function<std::expected<elysia::config::WindowMode,elysia::core::FailureDiagnostic>()> get_mode;
    std::function<std::expected<elysia::config::WindowSize,elysia::core::FailureDiagnostic>()> get_size;
    std::function<std::expected<WindowPosition,elysia::core::FailureDiagnostic>()> get_position;
};
struct ApplicationWindowSnapshot
{
    elysia::config::WindowSettings settings;
    int x = 0;
    int y = 0;
};
[[nodiscard]] ApplicationWindowOperations make_sdl_window_operations(SDL_Window* window);
[[nodiscard]] WindowOperationResult validate_window_settings(
    const elysia::config::WindowSettings& settings,const ApplicationWindowOperations& operations);
[[nodiscard]] std::expected<ApplicationWindowSnapshot,elysia::core::FailureDiagnostic> capture_window_snapshot(
    const elysia::config::WindowSettings& remembered,const ApplicationWindowOperations& operations);
[[nodiscard]] WindowOperationResult restore_window_snapshot(
    const ApplicationWindowSnapshot& previous,const ApplicationWindowOperations& operations);
// Successful fallback carries the original fullscreen failure for the caller's warning log.
[[nodiscard]] std::expected<std::optional<elysia::core::FailureDiagnostic>,elysia::core::FailureDiagnostic>
    apply_startup_window_settings(const elysia::config::WindowSettings& settings,
        const ApplicationWindowOperations& operations);

[[nodiscard]] WindowOperationResult apply_window_settings(
    const elysia::config::WindowSettings& settings,
    const ApplicationWindowOperations& operations);
[[nodiscard]] WindowOperationResult apply_window_settings_transactional(
    const elysia::config::WindowSettings& settings,
    const ApplicationWindowSnapshot& previous,
    const ApplicationWindowOperations& operations);
}
