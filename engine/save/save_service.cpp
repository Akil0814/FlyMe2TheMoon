#include "save_service.h"

#include "detail/save_store.h"
#include "../tools/logger.h"

#include <set>
#include <utility>

namespace elysia::save
{
namespace
{
SaveFailure failure(
    SaveError error,
    std::string_view save_name,
    std::string message,
    std::string key = {},
    std::source_location origin = std::source_location::current())
{
    return make_save_failure(error,std::string(save_name),std::move(key),std::move(message),origin);
}
}

SaveService::SaveService() = default;

SaveService::~SaveService()
{
    shutdown();
}

std::expected<void,SaveFailure> SaveService::initialize(
    const std::filesystem::path& save_directory)
{
    if (_initialized)
    {
        return std::unexpected(failure(
            SaveError::AlreadyInitialized,
            {},
            "SaveService is already initialized."));
    }

    auto store = std::make_unique<detail::SaveStore>(save_directory);
    if (auto initialized = store->initialize(); !initialized)
        return std::unexpected(initialized.error());

    _store = std::move(store);
    _entries.clear();
    _initialized = true;
    return {};
}

void SaveService::shutdown() noexcept
{
    if (!_initialized)
    {
        _entries.clear();
        _store.reset();
        return;
    }

    for (const auto& [name,entry] : _entries)
    {
        if (entry.dirty)
            ELYSIA_LOG_WARN("save","Discarding uncommitted save data during shutdown: " << name);
    }
    _entries.clear();
    _store.reset();
    _initialized = false;
}

bool SaveService::is_initialized() const noexcept
{
    return _initialized;
}

std::expected<void,SaveFailure> SaveService::create(
    std::string_view save_name,
    SaveCreateMode mode)
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    if (auto valid = detail::SaveStore::validate_save_name(save_name); !valid)
        return std::unexpected(valid.error());
    if (is_open(save_name))
        return std::unexpected(failure(SaveError::AlreadyOpen,save_name,"Save is already open."));

    const auto stored = _store->exists(save_name);
    if (!stored) return std::unexpected(stored.error());
    if (*stored && mode == SaveCreateMode::FailIfExists)
        return std::unexpected(failure(SaveError::AlreadyExists,save_name,"Save already exists."));

    _entries.emplace(std::string(save_name),SaveEntry{SaveData{},true,1});
    return {};
}

std::expected<SaveOpenResult,SaveFailure> SaveService::open(
    std::string_view save_name)
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    if (auto valid = detail::SaveStore::validate_save_name(save_name); !valid)
        return std::unexpected(valid.error());
    if (is_open(save_name))
        return std::unexpected(failure(SaveError::AlreadyOpen,save_name,"Save is already open."));

    auto loaded = _store->load(save_name);
    if (!loaded) return std::unexpected(loaded.error());
    SaveOpenResult result{loaded->recovered,std::move(loaded->warning)};
    _entries.emplace(
        std::string(save_name),
        SaveEntry{std::move(loaded->data),false,0});
    return result;
}

std::expected<bool,SaveFailure> SaveService::contains(
    std::string_view save_name,
    std::string_view key) const
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    const SaveEntry* entry = find_entry(save_name);
    if (!entry) return std::unexpected(not_open_failure(save_name));
    return entry->data.contains(key);
}

std::expected<bool,SaveFailure> SaveService::erase(
    std::string_view save_name,
    std::string_view key)
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    SaveEntry* entry = find_entry(save_name);
    if (!entry) return std::unexpected(not_open_failure(save_name));
    const bool removed = entry->data.erase(key);
    if (removed)
    {
        entry->dirty = true;
        ++entry->revision;
    }
    return removed;
}

std::expected<std::vector<std::string>,SaveFailure> SaveService::keys(
    std::string_view save_name,
    std::string_view prefix) const
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    const SaveEntry* entry = find_entry(save_name);
    if (!entry) return std::unexpected(not_open_failure(save_name));
    return entry->data.keys(prefix);
}

std::expected<SaveData,SaveFailure> SaveService::snapshot(
    std::string_view save_name) const
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    const SaveEntry* entry = find_entry(save_name);
    if (!entry) return std::unexpected(not_open_failure(save_name));
    return entry->data;
}

std::expected<void,SaveFailure> SaveService::replace(
    std::string_view save_name,
    SaveData data)
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    SaveEntry* entry = find_entry(save_name);
    if (!entry) return std::unexpected(not_open_failure(save_name));
    if (entry->data == data) return {};
    entry->data = std::move(data);
    entry->dirty = true;
    ++entry->revision;
    return {};
}

std::expected<void,SaveFailure> SaveService::commit(
    std::string_view save_name)
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    SaveEntry* entry = find_entry(save_name);
    if (!entry) return std::unexpected(not_open_failure(save_name));
    if (!entry->dirty) return {};

    const std::uint64_t saved_revision = entry->revision;
    const SaveData snapshot = entry->data;
    auto saved = _store->save(save_name,snapshot);
    if (!saved) return std::unexpected(saved.error());
    if (entry->revision == saved_revision) entry->dirty = false;
    return {};
}

std::expected<void,SaveFailure> SaveService::close(
    std::string_view save_name,
    SaveClosePolicy policy)
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    const auto iterator = _entries.find(save_name);
    if (iterator == _entries.end())
        return std::unexpected(not_open_failure(save_name));
    if (iterator->second.dirty && policy == SaveClosePolicy::RejectIfDirty)
        return std::unexpected(failure(SaveError::DirtySave,save_name,"Save has uncommitted changes."));
    _entries.erase(iterator);
    return {};
}

std::expected<bool,SaveFailure> SaveService::exists(
    std::string_view save_name) const
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    if (is_open(save_name)) return true;
    return _store->exists(save_name);
}

std::expected<std::vector<std::string>,SaveFailure>
SaveService::list_save_names() const
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    auto stored = _store->list_save_names();
    if (!stored) return std::unexpected(stored.error());
    std::set<std::string> names(stored->begin(),stored->end());
    for (const auto& [name,entry] : _entries)
    {
        (void)entry;
        names.insert(name);
    }
    return std::vector<std::string>(names.begin(),names.end());
}

std::expected<void,SaveFailure> SaveService::remove(
    std::string_view save_name)
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    if (is_open(save_name))
        return std::unexpected(failure(SaveError::AlreadyOpen,save_name,"Close the save before removing it."));
    return _store->remove(save_name);
}

bool SaveService::is_open(std::string_view save_name) const noexcept
{
    return _entries.contains(save_name);
}

std::expected<bool,SaveFailure> SaveService::is_dirty(
    std::string_view save_name) const
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    const SaveEntry* entry = find_entry(save_name);
    if (!entry) return std::unexpected(not_open_failure(save_name));
    return entry->dirty;
}

std::expected<std::uint64_t,SaveFailure> SaveService::revision(
    std::string_view save_name) const
{
    if (!_initialized) return std::unexpected(not_initialized_failure());
    const SaveEntry* entry = find_entry(save_name);
    if (!entry) return std::unexpected(not_open_failure(save_name));
    return entry->revision;
}

SaveFailure SaveService::not_initialized_failure() const
{
    return failure(SaveError::NotInitialized,{},"SaveService is not initialized.");
}

SaveFailure SaveService::not_open_failure(std::string_view save_name) const
{
    return failure(SaveError::NotOpen,save_name,"Save is not open.");
}

SaveService::SaveEntry* SaveService::find_entry(std::string_view save_name) noexcept
{
    const auto iterator = _entries.find(save_name);
    return iterator == _entries.end() ? nullptr : &iterator->second;
}

const SaveService::SaveEntry* SaveService::find_entry(
    std::string_view save_name) const noexcept
{
    const auto iterator = _entries.find(save_name);
    return iterator == _entries.end() ? nullptr : &iterator->second;
}
}
