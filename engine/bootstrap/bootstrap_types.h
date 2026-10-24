#pragma once

#include "../config/user_config_types.h"
#include "../core/diagnostics/failure_diagnostic.h"
#include "../io/loaders/asset_config_types.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <optional>
#include <utility>

namespace elysia::bootstrap
{
struct BootstrapFailure
{
    enum class Code
    {
        ProjectRoot,
        PathAccess,
        AssetDirectory,
        RuntimeDirectory,
        ContentRegistry,
        AppConfig,
        UserConfig,
        Preload,
        Runtime
    };

    Code code = Code::Runtime;
    elysia::core::FailureDiagnostic diagnostic;

    BootstrapFailure() = default;
    BootstrapFailure(
        Code failure_code,
        elysia::core::FailureDiagnostic failure_diagnostic)
        : code(failure_code),diagnostic(std::move(failure_diagnostic)) {}
    BootstrapFailure(
        Code failure_code,
        std::string message,
        std::source_location origin = std::source_location::current())
        : code(failure_code),diagnostic(elysia::core::make_failure_diagnostic(
            std::move(message),{},{},origin)) {}
    [[nodiscard]] std::string_view error_code() const noexcept
    {
        switch (code)
        {
        case Code::ProjectRoot: return "BOOTSTRAP-PROJECT-ROOT";
        case Code::PathAccess: return "BOOTSTRAP-PATH-ACCESS";
        case Code::AssetDirectory: return "BOOTSTRAP-ASSET-DIRECTORY";
        case Code::RuntimeDirectory: return "BOOTSTRAP-RUNTIME-DIRECTORY";
        case Code::ContentRegistry: return "BOOTSTRAP-CONTENT-REGISTRY";
        case Code::AppConfig: return "BOOTSTRAP-APP-CONFIG";
        case Code::UserConfig: return "BOOTSTRAP-USER-CONFIG";
        case Code::Preload: return "BOOTSTRAP-PRELOAD";
        case Code::Runtime: return "BOOTSTRAP-RUNTIME";
        }
        return "BOOTSTRAP-RUNTIME";
    }
};

struct RuntimeSettings
{
    std::string window_title = "Elysia Engine";
    elysia::config::UserConfigData user;
};

struct BootstrapOutput
{
    RuntimeSettings runtime_settings;
    elysia::io::ContentRegistry content_registry;
    std::filesystem::path i18n_manifest_path;
    std::optional<elysia::config::UserConfigFailure> warning;
};
}
