#pragma once

#include "../user_config_types.h"
#include "../../io/persistence/detail/persistence_operations.h"

#include <expected>
#include <filesystem>
#include <string>

namespace elysia::config
{
class UserConfigStore
{
public:
    explicit UserConfigStore(io::detail::PersistenceOperations operations = {}) : _operations(std::move(operations)) {}
    [[nodiscard]] std::expected<UserConfigLoadResult,UserConfigFailure> load(
        const std::filesystem::path& path,
        const UserConfigData& defaults) const;
    [[nodiscard]] std::expected<void,UserConfigFailure> save(
        const std::filesystem::path& path,
        const UserConfigData& settings) const;
private:
    io::detail::PersistenceOperations _operations;
};
}
