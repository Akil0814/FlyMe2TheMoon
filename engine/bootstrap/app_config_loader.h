#pragma once

#include "bootstrap_types.h"

#include <filesystem>
#include <expected>
#include <string>

namespace elysia::bootstrap
{
struct AppConfig
{
    std::string window_title = "Elysia Engine";
    elysia::config::UserConfigData user_defaults;
};

class AppConfigLoader
{
public:
    [[nodiscard]] std::expected<AppConfig,BootstrapFailure> load(const std::filesystem::path& app_config_path) const;
};

}
