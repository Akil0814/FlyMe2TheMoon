#include "save_data.h"

#include <algorithm>

namespace elysia::save
{
bool SaveData::contains(std::string_view key) const noexcept
{
    return _values.contains(key);
}

bool SaveData::erase(std::string_view key)
{
    const auto iterator = _values.find(key);
    if (iterator == _values.end()) return false;
    _values.erase(iterator);
    return true;
}

std::vector<std::string> SaveData::keys(std::string_view prefix) const
{
    std::vector<std::string> result;
    for (const auto& [key,value] : _values)
    {
        (void)value;
        if (key.starts_with(prefix)) result.push_back(key);
    }
    return result;
}

const std::map<std::string,SaveValue,std::less<>>& SaveData::entries() const noexcept
{
    return _values;
}

bool SaveData::value_is_valid(const SaveValue& value) noexcept
{
    return std::visit([](const auto& stored)
    {
        using Value = std::remove_cvref_t<decltype(stored)>;
        if constexpr (std::same_as<Value,double>)
        {
            return std::isfinite(stored);
        }
        else if constexpr (std::same_as<Value,std::vector<double>>)
        {
            return std::ranges::all_of(stored,[](double number)
            {
                return std::isfinite(number);
            });
        }
        else
        {
            return true;
        }
    },value);
}

std::string_view SaveData::value_type_name(const SaveValue& value) noexcept
{
    return std::visit([](const auto& stored) -> std::string_view
    {
        return type_name<decltype(stored)>();
    },value);
}
}
