#pragma once

#include "user_config.h"
#include "user_config_types.h"
#include "../tools/singleton.h"

#include <expected>
#include <filesystem>
#include <memory>
#include <string_view>

#define ELYSIA_USER_CONFIG (::elysia::config::UserConfigService::instance())

namespace elysia::config
{
class IUserConfigChangeHandler
{
public:
    virtual ~IUserConfigChangeHandler() = default;

    virtual std::expected<void,UserConfigFailure> apply_master_volume(int value) = 0;
    virtual std::expected<void,UserConfigFailure> apply_music_volume(int value) = 0;
    virtual std::expected<void,UserConfigFailure> apply_sound_volume(int value) = 0;
    virtual std::expected<void,UserConfigFailure> apply_language(std::string_view language) = 0;
    virtual std::expected<void,UserConfigFailure> apply_target_fps(double value) = 0;
    virtual std::expected<void,UserConfigFailure> apply_window_settings(
        const WindowSettings& settings) = 0;
};

class UserConfigStore;
class UserConfigService final : public elysia::tools::Singleton<UserConfigService>
{
    friend elysia::tools::Singleton<UserConfigService>;

public:
    [[nodiscard]] UserConfig& user_config() noexcept { return _user_config; }
    [[nodiscard]] const UserConfig& user_config() const noexcept { return _user_config; }
    [[nodiscard]] std::expected<void,UserConfigFailure> save_user_config();
    [[nodiscard]] std::expected<UserConfigApplyStatus,UserConfigCommitFailure>
        apply_and_save_user_config(const UserConfigData& settings);
    [[nodiscard]] std::expected<UserConfigApplyStatus,UserConfigCommitFailure>
        apply_and_save_user_config(
            const UserConfigData& settings,
            const UserConfigRuntimeState& rollback_state);
    void register_user_config_change_handler(IUserConfigChangeHandler& handler) noexcept;
    void unregister_user_config_change_handler(IUserConfigChangeHandler& handler) noexcept;
    [[nodiscard]] bool is_initialized() const noexcept { return _initialized; }
    [[nodiscard]] std::expected<UserConfigLoadResult,UserConfigFailure> initialize(
        const UserConfigData& default_settings,
        const std::filesystem::path& user_config_path);
    void shutdown() noexcept;

private:
    UserConfigService();
    ~UserConfigService();
    UserConfig _user_config;
    std::unique_ptr<UserConfigStore> _user_config_store;
    std::filesystem::path _user_config_path;
    bool _initialized = false;
};
}
