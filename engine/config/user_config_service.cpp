#include "user_config_service.h"

#include "user/user_config_store.h"

#include <memory>

namespace elysia::config
{
UserConfigService::UserConfigService() = default;

std::expected<UserConfigLoadResult,UserConfigFailure> UserConfigService::initialize(
    const UserConfigData& default_settings,
    const std::filesystem::path& user_config_path)
{
    auto prepared_store = std::make_unique<UserConfigStore>();
    auto prepared_path = user_config_path;
    auto settings_result = prepared_store->load(prepared_path,default_settings);
    if (!settings_result)
        return std::unexpected(settings_result.error());
    _user_config.initialize(settings_result->settings);
    _user_config_store = std::move(prepared_store);
    _user_config_path.swap(prepared_path);
    _initialized = true;
    return std::move(*settings_result);
}

std::expected<void,UserConfigFailure> UserConfigService::save_user_config()
{
    if (!_initialized)
        return std::unexpected(make_user_config_failure(UserConfigError::SaveFailed,{},"Config service is not initialized."));
    auto prepared = _user_config.snapshot();
    const auto result = _user_config_store->save(_user_config_path,prepared);
    if (!result) return std::unexpected(result.error());
    _user_config.mark_persisted(std::move(prepared));
    return {};
}

std::expected<UserConfigApplyStatus,UserConfigCommitFailure>
UserConfigService::apply_and_save_user_config(
    const UserConfigData& settings)
{
    return apply_and_save_user_config(settings,_user_config.runtime_state());
}

std::expected<UserConfigApplyStatus,UserConfigCommitFailure>
UserConfigService::apply_and_save_user_config(
    const UserConfigData& settings,
    const UserConfigRuntimeState& rollback_state)
{
    if (!_initialized)
    {
        return std::unexpected(UserConfigCommitFailure{
            make_user_config_failure(UserConfigError::SaveFailed,{},"Config service is not initialized."),
            std::nullopt
        });
    }

    const auto rollback = [&]() -> std::optional<UserConfigFailure>
    {
        const auto restored =
            _user_config.apply_snapshot(rollback_state.settings,true);
        _user_config.restore_restart_required(
            rollback_state.restart_required);
        if (!restored)
            return restored.error();
        return std::nullopt;
    };

    const auto applied = _user_config.apply_snapshot(settings);
    if (!applied)
    {
        return std::unexpected(UserConfigCommitFailure{
            applied.error(),
            rollback()
        });
    }

    const auto saved = save_user_config();
    if (!saved)
    {
        return std::unexpected(UserConfigCommitFailure{
            saved.error(),
            rollback()
        });
    }

    return *applied;
}

void UserConfigService::register_user_config_change_handler(IUserConfigChangeHandler& handler) noexcept { _user_config.register_change_handler(handler); }
void UserConfigService::unregister_user_config_change_handler(IUserConfigChangeHandler& handler) noexcept { _user_config.unregister_change_handler(handler); }
void UserConfigService::shutdown() noexcept { _user_config.reset(); _user_config_store.reset(); _user_config_path.clear(); _initialized = false; }
UserConfigService::~UserConfigService() = default;
}
