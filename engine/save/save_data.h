#pragma once

#include "save_types.h"

#include <cmath>
#include <concepts>
#include <cstdint>
#include <expected>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace elysia::save
{
using SaveValue = std::variant<
    bool,
    std::int64_t,
    double,
    std::string,
    std::vector<bool>,
    std::vector<std::int64_t>,
    std::vector<double>,
    std::vector<std::string>>;

template<typename T>
inline constexpr bool is_save_value_v =
    std::same_as<std::remove_cvref_t<T>,bool>
    || std::same_as<std::remove_cvref_t<T>,std::int64_t>
    || std::same_as<std::remove_cvref_t<T>,double>
    || std::same_as<std::remove_cvref_t<T>,std::string>
    || std::same_as<std::remove_cvref_t<T>,std::vector<bool>>
    || std::same_as<std::remove_cvref_t<T>,std::vector<std::int64_t>>
    || std::same_as<std::remove_cvref_t<T>,std::vector<double>>
    || std::same_as<std::remove_cvref_t<T>,std::vector<std::string>>;

template<typename T>
concept SaveValueCompatible = is_save_value_v<T>;

class SaveData
{
public:
    template<SaveValueCompatible T>
    [[nodiscard]] std::expected<bool,SaveFailure> set(
        std::string key,
        T&& value);

    template<SaveValueCompatible T>
    [[nodiscard]] std::expected<std::remove_cvref_t<T>,SaveFailure> get(
        std::string_view key) const;

    [[nodiscard]] bool contains(std::string_view key) const noexcept;
    [[nodiscard]] bool erase(std::string_view key);
    [[nodiscard]] std::vector<std::string> keys(
        std::string_view prefix = {}) const;

    [[nodiscard]] const std::map<std::string,SaveValue,std::less<>>& entries()
        const noexcept;

    friend bool operator==(const SaveData&,const SaveData&) = default;

private:
    static bool value_is_valid(const SaveValue& value) noexcept;
    static std::string_view value_type_name(const SaveValue& value) noexcept;

    template<typename T>
    static constexpr std::string_view type_name() noexcept;

private:
    std::map<std::string,SaveValue,std::less<>> _values;
};

template<typename T>
constexpr std::string_view SaveData::type_name() noexcept
{
    using Value = std::remove_cvref_t<T>;
    if constexpr (std::same_as<Value,bool>) return "bool";
    else if constexpr (std::same_as<Value,std::int64_t>) return "int64";
    else if constexpr (std::same_as<Value,double>) return "double";
    else if constexpr (std::same_as<Value,std::string>) return "string";
    else if constexpr (std::same_as<Value,std::vector<bool>>) return "bool_array";
    else if constexpr (std::same_as<Value,std::vector<std::int64_t>>) return "int64_array";
    else if constexpr (std::same_as<Value,std::vector<double>>) return "double_array";
    else return "string_array";
}

template<SaveValueCompatible T>
std::expected<bool,SaveFailure> SaveData::set(std::string key,T&& value)
{
    using Value = std::remove_cvref_t<T>;
    if (key.empty())
    {
        return std::unexpected(make_save_failure(
            SaveError::InvalidKey,
            {},
            {},
            "SaveData key must not be empty."
        ));
    }

    SaveValue stored = Value(std::forward<T>(value));
    if (!value_is_valid(stored))
    {
        return std::unexpected(make_save_failure(
            SaveError::InvalidValue,
            {},
            key,
            "SaveData floating-point values must be finite."
        ));
    }

    const auto existing = _values.find(key);
    if (existing != _values.end() && existing->second == stored)
        return false;

    _values.insert_or_assign(std::move(key),std::move(stored));
    return true;
}

template<SaveValueCompatible T>
std::expected<std::remove_cvref_t<T>,SaveFailure> SaveData::get(
    std::string_view key) const
{
    using Value = std::remove_cvref_t<T>;
    const auto iterator = _values.find(key);
    if (iterator == _values.end())
    {
        return std::unexpected(make_save_failure(
            SaveError::KeyNotFound,
            {},
            std::string(key),
            "SaveData key was not found."
        ));
    }

    if (const Value* value = std::get_if<Value>(&iterator->second))
        return *value;

    return std::unexpected(make_save_failure(
        SaveError::TypeMismatch,
        {},
        std::string(key),
        "SaveData type mismatch: expected " + std::string(type_name<Value>())
            + ", found " + std::string(value_type_name(iterator->second)) + "."
    ));
}
}
