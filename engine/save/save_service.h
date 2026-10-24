#pragma once

#include "save_data.h"
#include "../tools/singleton.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#define ELYSIA_SAVE (::elysia::save::SaveService::instance())

namespace elysia::save
{
namespace detail { class SaveStore; }

class SaveService final : public elysia::tools::Singleton<SaveService>
{
    friend elysia::tools::Singleton<SaveService>;

public:
    ~SaveService();

    [[nodiscard]] std::expected<void,SaveFailure> initialize(const std::filesystem::path& save_directory);
    void shutdown() noexcept;
    [[nodiscard]] bool is_initialized() const noexcept;

    [[nodiscard]] std::expected<void,SaveFailure> create(std::string_view save_name,
        SaveCreateMode mode = SaveCreateMode::FailIfExists);
    [[nodiscard]] std::expected<SaveOpenResult,SaveFailure> open(std::string_view save_name);

    template<SaveValueCompatible T>
    [[nodiscard]] std::expected<void,SaveFailure> set(std::string_view save_name,
        std::string key,T&& value);

    template<SaveValueCompatible T>
    [[nodiscard]] std::expected<std::remove_cvref_t<T>,SaveFailure> get(std::string_view save_name,
        std::string_view key) const;

    [[nodiscard]] std::expected<bool,SaveFailure> contains(std::string_view save_name,
        std::string_view key) const;
    [[nodiscard]] std::expected<bool,SaveFailure> erase(
        std::string_view save_name,
        std::string_view key);
    [[nodiscard]] std::expected<std::vector<std::string>,SaveFailure> keys(
        std::string_view save_name,
        std::string_view prefix = {}) const;

    [[nodiscard]] std::expected<SaveData,SaveFailure> snapshot(
        std::string_view save_name) const;
    [[nodiscard]] std::expected<void,SaveFailure> replace(
        std::string_view save_name,
        SaveData data);

    [[nodiscard]] std::expected<void,SaveFailure> commit(
        std::string_view save_name);
    [[nodiscard]] std::expected<void,SaveFailure> close(
        std::string_view save_name,
        SaveClosePolicy policy = SaveClosePolicy::RejectIfDirty);

    [[nodiscard]] std::expected<bool,SaveFailure> exists(
        std::string_view save_name) const;
    [[nodiscard]] std::expected<std::vector<std::string>,SaveFailure>
        list_save_names() const;
    [[nodiscard]] std::expected<void,SaveFailure> remove(
        std::string_view save_name);

    [[nodiscard]] bool is_open(std::string_view save_name) const noexcept;
    [[nodiscard]] std::expected<bool,SaveFailure> is_dirty(
        std::string_view save_name) const;
    [[nodiscard]] std::expected<std::uint64_t,SaveFailure> revision(
        std::string_view save_name) const;

private:
    struct SaveEntry
    {
        SaveData data;
        bool dirty = false;
        std::uint64_t revision = 0;
    };

    SaveService();

    [[nodiscard]] SaveFailure not_initialized_failure() const;
    [[nodiscard]] SaveFailure not_open_failure(
        std::string_view save_name) const;
    [[nodiscard]] SaveEntry* find_entry(std::string_view save_name) noexcept;
    [[nodiscard]] const SaveEntry* find_entry(
        std::string_view save_name) const noexcept;

private:
    std::unique_ptr<detail::SaveStore> _store;
    std::map<std::string,SaveEntry,std::less<>> _entries;
    bool _initialized = false;
};

template<SaveValueCompatible T>
std::expected<void,SaveFailure> SaveService::set(
    std::string_view save_name,
    std::string key,
    T&& value)
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    SaveEntry* entry = find_entry(save_name);
    if (!entry) return std::unexpected(not_open_failure(save_name));

    auto changed = entry->data.set(std::move(key),std::forward<T>(value));
    if (!changed)
    {
        SaveFailure error = changed.error();
        bind_save_name(error,save_name);
        return std::unexpected(std::move(error));
    }
    if (*changed)
    {
        entry->dirty = true;
        ++entry->revision;
    }
    return {};
}

template<SaveValueCompatible T>
std::expected<std::remove_cvref_t<T>,SaveFailure> SaveService::get(
    std::string_view save_name,
    std::string_view key) const
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    const SaveEntry* entry = find_entry(save_name);
    if (!entry) return std::unexpected(not_open_failure(save_name));

    auto result = entry->data.get<T>(key);
    if (!result)
    {
        SaveFailure error = result.error();
        bind_save_name(error,save_name);
        return std::unexpected(std::move(error));
    }
    return result;
}
}
