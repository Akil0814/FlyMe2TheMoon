#pragma once

#include "../save_data.h"
#include "../../io/persistence/detail/persistence_operations.h"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace elysia::save::detail
{
struct SaveStoreLoadResult
{
    SaveData data;
    bool recovered = false;
    std::optional<SaveFailure> warning;
};

class SaveStore
{
public:
    explicit SaveStore(std::filesystem::path save_directory,io::detail::PersistenceOperations operations = {});

    [[nodiscard]] std::expected<void,SaveFailure> initialize() const;
    [[nodiscard]] std::expected<SaveStoreLoadResult,SaveFailure> load(
        std::string_view save_name) const;
    [[nodiscard]] std::expected<void,SaveFailure> save(
        std::string_view save_name,
        const SaveData& data) const;
    [[nodiscard]] std::expected<bool,SaveFailure> exists(
        std::string_view save_name) const;
    [[nodiscard]] std::expected<std::vector<std::string>,SaveFailure>
        list_save_names() const;
    [[nodiscard]] std::expected<void,SaveFailure> remove(
        std::string_view save_name) const;

    [[nodiscard]] static std::expected<void,SaveFailure> validate_save_name(
        std::string_view save_name);

private:
    [[nodiscard]] std::filesystem::path primary_path(
        std::string_view save_name) const;
    [[nodiscard]] std::filesystem::path temporary_path(
        std::string_view save_name) const;
    [[nodiscard]] std::filesystem::path backup_path(
        std::string_view save_name) const;

private:
    std::filesystem::path _save_directory;
    io::detail::PersistenceOperations _operations;
};
}
