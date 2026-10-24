#pragma once

#include "../user_config_data.h"
#include "../../io/json/json_loader.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace elysia::config::detail
{
using Json = elysia::io::json;

inline bool parse_positive_int(
    const Json& node,
    std::string_view key,
    int& out,
    std::string& error)
{
    const std::string key_string(key);
    if (!node.contains(key_string) || !node.at(key_string).is_number_integer())
    {
        error = key_string + " must be an integer.";
        return false;
    }

    const auto& number = node.at(key_string);
    if (number.is_number_unsigned() && number.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
    {
        error = key_string + " is out of range.";
        return false;
    }
    const auto value = number.get<std::int64_t>();
    if (value <= 0 || value > std::numeric_limits<int>::max())
    {
        error = key_string + " is out of range.";
        return false;
    }

    out = static_cast<int>(value);
    return true;
}

inline bool parse_boolean(
    const Json& node,
    std::string_view key,
    bool& out,
    std::string& error)
{
    const std::string key_string(key);
    if (!node.contains(key_string) || !node.at(key_string).is_boolean())
    {
        error = key_string + " must be boolean.";
        return false;
    }

    out = node.at(key_string).get<bool>();
    return true;
}

inline bool parse_window_mode(
    const Json& node,
    WindowMode& out,
    std::string& error)
{
    if (!node.is_string())
    {
        error = "window.mode must be a string.";
        return false;
    }

    const std::string value = node.get<std::string>();
    if (value == "windowed")
    {
        out = WindowMode::Windowed;
        return true;
    }
    if (value == "borderless_fullscreen")
    {
        out = WindowMode::BorderlessFullscreen;
        return true;
    }

    error = "window.mode must be windowed or borderless_fullscreen.";
    return false;
}

inline bool parse_volume(
    const Json& node,
    std::string_view key,
    int& out,
    std::string& error)
{
    const std::string key_string(key);
    if (!node.contains(key_string) || !node.at(key_string).is_number_integer())
    {
        error = key_string + " must be an integer.";
        return false;
    }

    const auto& number = node.at(key_string);
    if (number.is_number_unsigned() && number.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
    {
        error = key_string + " is out of range.";
        return false;
    }
    const auto value = number.get<std::int64_t>();
    if (value < 0 || value > 100)
    {
        error = key_string + " must be within 0..100.";
        return false;
    }

    out = static_cast<int>(value);
    return true;
}

inline bool parse_positive_number(
    const Json& node,
    std::string_view key,
    double& out,
    std::string& error)
{
    const std::string key_string(key);
    if (!node.contains(key_string) || !node.at(key_string).is_number())
    {
        error = key_string + " must be numeric.";
        return false;
    }

    out = node.at(key_string).get<double>();
    if (!std::isfinite(out) || out <= 0.0)
    {
        error = key_string + " must be finite and positive.";
        return false;
    }

    return true;
}

inline bool parse_non_empty_string(
    const Json& node,
    std::string_view key,
    std::string& out,
    std::string& error)
{
    const std::string key_string(key);
    if (!node.contains(key_string) || !node.at(key_string).is_string())
    {
        error = key_string + " must be a string.";
        return false;
    }

    out = node.at(key_string).get<std::string>();
    if (out.empty())
    {
        error = key_string + " must be non-empty.";
        return false;
    }

    return true;
}
}
