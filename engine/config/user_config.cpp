#include "user_config_service.h"

#include <cmath>
#include <type_traits>

namespace elysia::config
{
namespace
{
[[nodiscard]] std::unexpected<UserConfigFailure> invalid(std::string setting,std::string message,
    std::source_location origin = std::source_location::current())
{
    return std::unexpected(make_user_config_failure(UserConfigError::InvalidValue,std::move(setting),std::move(message),origin));
}

[[nodiscard]] bool is_volume(int value) noexcept { return value >= 0 && value <= 100; }
}

const WindowSettings&
UserConfig::window_settings() const noexcept { return _current_settings.window; }
double UserConfig::target_fps() const noexcept { return _current_settings.target_fps; }
bool UserConfig::vsync() const noexcept { return _current_settings.vsync; }
int UserConfig::master_volume() const noexcept { return _current_settings.audio.master_volume; }
int UserConfig::music_volume() const noexcept { return _current_settings.audio.music_volume; }
int UserConfig::sound_volume() const noexcept { return _current_settings.audio.sound_volume; }
std::string_view UserConfig::language() const noexcept { return _current_settings.language; }

std::expected<void,UserConfigFailure> UserConfig::require_handler(std::string_view setting) const
{
    if (_change_handler) return {};
    return std::unexpected(make_user_config_failure(UserConfigError::ChangeHandlerUnavailable,std::string(setting),"A runtime settings change handler is not registered."));
}

std::expected<void,UserConfigFailure> UserConfig::validate_snapshot(
    const UserConfigData& settings) const
{
    if (settings.window.mode != WindowMode::Windowed
        && settings.window.mode
            != WindowMode::BorderlessFullscreen)
        return invalid("window_settings","Unknown window mode.");
    if (settings.window.windowed_size.width <= 0
        || settings.window.windowed_size.height <= 0)
        return invalid("window_size","Window width and height must be positive.");
    if (!std::isfinite(settings.target_fps) || settings.target_fps <= 0.0)
        return invalid("target_fps","Target FPS must be finite and positive.");
    if (!is_volume(settings.audio.master_volume))
        return invalid("master_volume","Volume must be within 0..100.");
    if (!is_volume(settings.audio.music_volume))
        return invalid("music_volume","Volume must be within 0..100.");
    if (!is_volume(settings.audio.sound_volume))
        return invalid("sound_volume","Volume must be within 0..100.");
    if (settings.language.empty())
        return invalid("language","Language must be non-empty.");
    return {};
}

std::expected<UserConfigApplyStatus,UserConfigFailure> UserConfig::apply_snapshot(
    const UserConfigData& settings,
    bool continue_after_failure)
{
    if (const auto valid = validate_snapshot(settings); !valid)
        return std::unexpected(valid.error());

    std::optional<UserConfigFailure> first_failure;
    UserConfigApplyStatus status = UserConfigApplyStatus::Applied;
    const auto apply = [&](auto&& operation)
    {
        const auto result = operation();
        if (!result)
        {
            if (!first_failure)
                first_failure = result.error();
            else
                io::append_failure_context(first_failure->diagnostic,result.error().diagnostic);
            return continue_after_failure;
        }
        if (*result == UserConfigApplyStatus::PendingRestart)
            status = UserConfigApplyStatus::PendingRestart;
        return true;
    };

    if (!apply([&]() { return set_window_settings(settings.window); }))
        return std::unexpected(*first_failure);
    if (!apply([&]() { return set_target_fps(settings.target_fps); }))
        return std::unexpected(*first_failure);
    if (!apply([&]() { return set_master_volume(settings.audio.master_volume); }))
        return std::unexpected(*first_failure);
    if (!apply([&]() { return set_music_volume(settings.audio.music_volume); }))
        return std::unexpected(*first_failure);
    if (!apply([&]() { return set_sound_volume(settings.audio.sound_volume); }))
        return std::unexpected(*first_failure);
    if (!apply([&]() { return set_language(settings.language); }))
        return std::unexpected(*first_failure);
    if (!apply([&]() { return set_vsync(settings.vsync); }))
        return std::unexpected(*first_failure);

    if (first_failure)
        return std::unexpected(*first_failure);
    return status;
}

std::expected<UserConfigApplyStatus,UserConfigFailure>
UserConfig::set_window_settings(
    const WindowSettings& settings)
{
    if (settings.mode != WindowMode::Windowed
        && settings.mode
            != WindowMode::BorderlessFullscreen)
        return invalid("window_settings","Unknown window mode.");
    if (settings.windowed_size.width <= 0
        || settings.windowed_size.height <= 0)
    {
        return invalid(
            "window_settings",
            "Window width and height must be positive.");
    }
    if (settings == _current_settings.window)
        return UserConfigApplyStatus::Applied;
    if (const auto handler = require_handler("window_settings"); !handler)
        return std::unexpected(handler.error());
    if (const auto applied =
            _change_handler->apply_window_settings(settings);
        !applied)
    {
        return std::unexpected(applied.error());
    }
    _current_settings.window = settings;
    return UserConfigApplyStatus::Applied;
}

std::expected<UserConfigApplyStatus,UserConfigFailure> UserConfig::set_target_fps(double value)
{
    if (!std::isfinite(value) || value <= 0.0)
        return invalid("target_fps","Target FPS must be finite and positive.");
    if (value == _current_settings.target_fps) return UserConfigApplyStatus::Applied;
    if (const auto handler = require_handler("target_fps"); !handler) return std::unexpected(handler.error());
    if (const auto applied = _change_handler->apply_target_fps(value); !applied) return std::unexpected(applied.error());
    _current_settings.target_fps = value;
    return UserConfigApplyStatus::Applied;
}

std::expected<UserConfigApplyStatus,UserConfigFailure> UserConfig::set_vsync(bool value)
{
    if (value == _current_settings.vsync)
    {
        return _vsync_restart_pending
            ? UserConfigApplyStatus::PendingRestart
            : UserConfigApplyStatus::Applied;
    }
    _current_settings.vsync = value;
    _vsync_restart_pending = _current_settings.vsync != _active_vsync;
    return _vsync_restart_pending
        ? UserConfigApplyStatus::PendingRestart
        : UserConfigApplyStatus::Applied;
}

std::expected<UserConfigApplyStatus,UserConfigFailure> UserConfig::set_master_volume(int value)
{
    if (!is_volume(value)) return invalid("master_volume","Volume must be within 0..100.");
    if (value == _current_settings.audio.master_volume) return UserConfigApplyStatus::Applied;
    if (const auto handler = require_handler("master_volume"); !handler) return std::unexpected(handler.error());
    if (const auto applied = _change_handler->apply_master_volume(value); !applied) return std::unexpected(applied.error());
    _current_settings.audio.master_volume = value;
    return UserConfigApplyStatus::Applied;
}

std::expected<UserConfigApplyStatus,UserConfigFailure> UserConfig::set_music_volume(int value)
{
    if (!is_volume(value)) return invalid("music_volume","Volume must be within 0..100.");
    if (value == _current_settings.audio.music_volume) return UserConfigApplyStatus::Applied;
    if (const auto handler = require_handler("music_volume"); !handler) return std::unexpected(handler.error());
    if (const auto applied = _change_handler->apply_music_volume(value); !applied) return std::unexpected(applied.error());
    _current_settings.audio.music_volume = value;
    return UserConfigApplyStatus::Applied;
}

std::expected<UserConfigApplyStatus,UserConfigFailure> UserConfig::set_sound_volume(int value)
{
    if (!is_volume(value)) return invalid("sound_volume","Volume must be within 0..100.");
    if (value == _current_settings.audio.sound_volume) return UserConfigApplyStatus::Applied;
    if (const auto handler = require_handler("sound_volume"); !handler) return std::unexpected(handler.error());
    if (const auto applied = _change_handler->apply_sound_volume(value); !applied) return std::unexpected(applied.error());
    _current_settings.audio.sound_volume = value;
    return UserConfigApplyStatus::Applied;
}

std::expected<UserConfigApplyStatus,UserConfigFailure> UserConfig::set_language(std::string value)
{
    if (value.empty()) return invalid("language","Language must be non-empty.");
    if (value == _current_settings.language) return UserConfigApplyStatus::Applied;
    if (const auto handler = require_handler("language"); !handler) return std::unexpected(handler.error());
    if (const auto applied = _change_handler->apply_language(value); !applied) return std::unexpected(applied.error());
    _current_settings.language = std::move(value);
    return UserConfigApplyStatus::Applied;
}

UserConfigData UserConfig::snapshot() const { return _current_settings; }
UserConfigRuntimeState UserConfig::runtime_state() const
{
    return UserConfigRuntimeState{
        .settings = _current_settings,
        .restart_required = _vsync_restart_pending
    };
}
bool UserConfig::is_dirty() const noexcept { return _current_settings != _persisted_snapshot; }
bool UserConfig::restart_required() const noexcept { return _vsync_restart_pending; }
void UserConfig::initialize(const UserConfigData& settings)
{
    UserConfigData current = settings;
    UserConfigData persisted = settings;
    static_assert(std::is_nothrow_move_assignable_v<UserConfigData>);
    _current_settings = std::move(current);
    _persisted_snapshot = std::move(persisted);
    _change_handler = nullptr;
    _active_vsync = settings.vsync;
    _vsync_restart_pending = false;
}
void UserConfig::mark_persisted(UserConfigData&& prepared) noexcept
{
    static_assert(std::is_nothrow_move_assignable_v<UserConfigData>);
    _persisted_snapshot = std::move(prepared);
}
void UserConfig::reset() noexcept { _current_settings = {}; _persisted_snapshot = {}; _change_handler = nullptr; _active_vsync = true; _vsync_restart_pending = false; }
void UserConfig::register_change_handler(IUserConfigChangeHandler& handler) noexcept { _change_handler = &handler; }
void UserConfig::unregister_change_handler(IUserConfigChangeHandler& handler) noexcept { if (_change_handler == &handler) _change_handler = nullptr; }
void UserConfig::restore_restart_required(bool restart_required) noexcept { _vsync_restart_pending = restart_required; }
}
