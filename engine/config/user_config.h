#pragma once

#include "user_config_types.h"

#include <expected>
#include <string>
#include <string_view>

namespace elysia::config
{
class IUserConfigChangeHandler;

class UserConfig
{
public:
    [[nodiscard]] const WindowSettings&
        window_settings() const noexcept;
    [[nodiscard]] double target_fps() const noexcept;
    [[nodiscard]] bool vsync() const noexcept;
    [[nodiscard]] int master_volume() const noexcept;
    [[nodiscard]] int music_volume() const noexcept;
    [[nodiscard]] int sound_volume() const noexcept;
    [[nodiscard]] std::string_view language() const noexcept;

    [[nodiscard]] std::expected<UserConfigApplyStatus,UserConfigFailure>
        set_window_settings(const WindowSettings& settings);
    [[nodiscard]] std::expected<UserConfigApplyStatus,UserConfigFailure> set_target_fps(double value);
    [[nodiscard]] std::expected<UserConfigApplyStatus,UserConfigFailure> set_vsync(bool value);
    [[nodiscard]] std::expected<UserConfigApplyStatus,UserConfigFailure> set_master_volume(int value);
    [[nodiscard]] std::expected<UserConfigApplyStatus,UserConfigFailure> set_music_volume(int value);
    [[nodiscard]] std::expected<UserConfigApplyStatus,UserConfigFailure> set_sound_volume(int value);
    [[nodiscard]] std::expected<UserConfigApplyStatus,UserConfigFailure> set_language(std::string value);

    [[nodiscard]] UserConfigData snapshot() const;
    [[nodiscard]] UserConfigRuntimeState runtime_state() const;
    [[nodiscard]] bool is_dirty() const noexcept;
    [[nodiscard]] bool restart_required() const noexcept;

private:
    friend class UserConfigService;
    void initialize(const UserConfigData& settings);
    void mark_persisted(UserConfigData&& prepared) noexcept;
    void reset() noexcept;
    void register_change_handler(IUserConfigChangeHandler& handler) noexcept;
    void unregister_change_handler(IUserConfigChangeHandler& handler) noexcept;

    [[nodiscard]] std::expected<void,UserConfigFailure> require_handler(std::string_view setting) const;
    [[nodiscard]] std::expected<void,UserConfigFailure> validate_snapshot(
        const UserConfigData& settings) const;
    [[nodiscard]] std::expected<UserConfigApplyStatus,UserConfigFailure> apply_snapshot(
        const UserConfigData& settings,
        bool continue_after_failure = false);
    void restore_restart_required(bool restart_required) noexcept;
    UserConfigData _current_settings;
    UserConfigData _persisted_snapshot;
    IUserConfigChangeHandler* _change_handler = nullptr;
    bool _active_vsync = true;
    bool _vsync_restart_pending = false;
};
}
