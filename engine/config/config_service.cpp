#include "config_service.h"

#include "content/config_snapshot_internal.h"
#include "../tools/logger.h"

#include <cmath>
#include <limits>

namespace elysia::config
{
namespace
{
using Json = elysia::io::json;
using Node = ConfigSnapshotNode;

std::string type_name(const Json& value)
{
    if (value.is_object()) return "object"; if (value.is_array()) return "array";
    if (value.is_string()) return "string"; if (value.is_boolean()) return "bool";
    if (value.is_number_integer() || value.is_number_unsigned()) return "integer";
    if (value.is_number_float()) return "double"; if (value.is_null()) return "null"; return "unknown";
}

std::expected<float,std::string> finite_float(const Json& value)
{
    if (!value.is_number()) return std::unexpected("value is not numeric");
    const double number = value.get<double>();
    if (!std::isfinite(number) || number < -std::numeric_limits<float>::max()
        || number > std::numeric_limits<float>::max())
        return std::unexpected("value is not a finite representable float");
    return static_cast<float>(number);
}

bool exact_shape(const Json& value,std::initializer_list<std::string_view> names)
{
    if (!value.is_object() || value.size() != names.size()) return false;
    for (auto name : names) if (!value.contains(std::string(name))) return false;
    return true;
}

std::expected<Node,ConfigAccessFailure> find_node(const std::shared_ptr<const ConfigSnapshot::Impl>& snapshot,
    std::string_view key,std::string expected)
{
    if (!snapshot) return std::unexpected(ConfigAccessFailure{ConfigAccessError::NotInitialized,
        std::string(key),std::move(expected),{}, {},"ConfigService is not initialized."});
    const auto found = snapshot->nodes.find(std::string(key));
    if (found == snapshot->nodes.end())
        return std::unexpected(ConfigAccessFailure{ConfigAccessError::MissingKey,std::string(key),
            std::move(expected),"missing",{},"Required config key is missing: " + std::string(key)});
    return found->second;
}

ConfigAccessFailure mismatch(const Node& node,std::string expected,std::string message = {})
{
    if (message.empty()) message = "Config type mismatch for " + node.origin.full_key
        + ": expected " + expected + ", actual " + type_name(node.value);
    return {ConfigAccessError::TypeMismatch,node.origin.full_key,std::move(expected),
        type_name(node.value),node.origin,std::move(message)};
}
}

void ConfigService::publish(std::shared_ptr<const ConfigSnapshot> snapshot) noexcept
{
    std::scoped_lock lock(_mutex);
    _snapshot = std::move(snapshot);
    _logged_access_errors.clear();
}

void ConfigService::shutdown() noexcept
{
    std::scoped_lock lock(_mutex); _snapshot.reset(); _logged_access_errors.clear();
}

bool ConfigService::is_initialized() const noexcept
{
    std::scoped_lock lock(_mutex); return static_cast<bool>(_snapshot);
}

bool ConfigService::contains(std::string_view key) const
{
    std::scoped_lock lock(_mutex);
    return _snapshot && _snapshot->_impl->nodes.contains(std::string(key));
}

void ConfigService::log_once(const ConfigAccessFailure& failure) const
{
    const std::string dedupe = std::to_string(static_cast<int>(failure.error)) + "|"
        + failure.key + "|" + failure.expected_type;
    bool log = false;
    { std::scoped_lock lock(_mutex); log = _logged_access_errors.insert(dedupe).second; }
    if (log) ELYSIA_LOG_ERROR("config",failure.message);
}

#define CONFIG_SCALAR_BODY(TYPE, EXPECTED, ...) \
    std::shared_ptr<const ConfigSnapshot> snapshot; { std::scoped_lock lock(_mutex); snapshot = _snapshot; } \
    auto node = find_node(snapshot ? snapshot->_impl : nullptr,key,EXPECTED); if (!node) { log_once(node.error()); return std::unexpected(node.error()); } \
    auto converted = (__VA_ARGS__); if (!converted) { auto failure = mismatch(*node,EXPECTED,converted.error()); log_once(failure); return std::unexpected(failure); } \
    return *converted

std::expected<std::int64_t,ConfigAccessFailure> ConfigService::get_int(std::string_view key) const
{
    CONFIG_SCALAR_BODY(std::int64_t,"int",[&]() -> std::expected<std::int64_t,std::string> {
        if (!node->value.is_number_integer() && !node->value.is_number_unsigned())
            return std::unexpected("Config value is not an integer: " + node->origin.describe());
        if (node->value.is_number_unsigned()
            && node->value.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            return std::unexpected("Config integer is outside int64 range: " + node->origin.describe());
        return node->value.get<std::int64_t>();
    }());
}

std::expected<double,ConfigAccessFailure> ConfigService::get_double(std::string_view key) const
{
    CONFIG_SCALAR_BODY(double,"double",[&]() -> std::expected<double,std::string> {
        if (!node->value.is_number()) return std::unexpected("Config value is not numeric: " + node->origin.describe());
        const double value = node->value.get<double>();
        if (!std::isfinite(value)) return std::unexpected("Config number is not finite: " + node->origin.describe());
        return value;
    }());
}

std::expected<bool,ConfigAccessFailure> ConfigService::get_bool(std::string_view key) const
{ CONFIG_SCALAR_BODY(bool,"bool",[&]() -> std::expected<bool,std::string> { if (!node->value.is_boolean()) return std::unexpected("Config value is not boolean: " + node->origin.describe()); return node->value.get<bool>(); }()); }

std::expected<std::string,ConfigAccessFailure> ConfigService::get_string(std::string_view key) const
{ CONFIG_SCALAR_BODY(std::string,"string",[&]() -> std::expected<std::string,std::string> { if (!node->value.is_string()) return std::unexpected("Config value is not a string: " + node->origin.describe()); return node->value.get<std::string>(); }()); }

std::expected<elysia::core::Vector2,ConfigAccessFailure> ConfigService::get_vector2(std::string_view key) const
{
    CONFIG_SCALAR_BODY(elysia::core::Vector2,"Vector2",[&]() -> std::expected<elysia::core::Vector2,std::string> {
        if (!exact_shape(node->value,{"x","y"})) return std::unexpected("Vector2 must contain exactly x and y: " + node->origin.describe());
        auto x=finite_float(node->value.at("x")); auto y=finite_float(node->value.at("y"));
        if (!x||!y) return std::unexpected("Vector2 components must be finite representable floats: " + node->origin.describe());
        return elysia::core::Vector2{*x,*y};
    }());
}

std::expected<elysia::core::Rect,ConfigAccessFailure> ConfigService::get_rect(std::string_view key) const
{
    CONFIG_SCALAR_BODY(elysia::core::Rect,"Rect",[&]() -> std::expected<elysia::core::Rect,std::string> {
        if (!exact_shape(node->value,{"x","y","width","height"})) return std::unexpected("Rect must contain exactly x, y, width and height: " + node->origin.describe());
        auto x=finite_float(node->value.at("x")); auto y=finite_float(node->value.at("y"));
        auto w=finite_float(node->value.at("width")); auto h=finite_float(node->value.at("height"));
        if (!x||!y||!w||!h||*w<0||*h<0) return std::unexpected("Rect components must be finite floats and size must be non-negative: " + node->origin.describe());
        return elysia::core::Rect{*x,*y,*w,*h};
    }());
}
#undef CONFIG_SCALAR_BODY

#define CONFIG_ARRAY_GETTER(NAME, TYPE, SCALAR) \
std::expected<std::vector<TYPE>,ConfigAccessFailure> ConfigService::NAME(std::string_view key) const { \
    std::shared_ptr<const ConfigSnapshot> snapshot; { std::scoped_lock lock(_mutex); snapshot = _snapshot; } \
    auto node=find_node(snapshot ? snapshot->_impl : nullptr,key,#TYPE " array"); if(!node){log_once(node.error());return std::unexpected(node.error());} \
    if(!node->value.is_array()){auto failure=mismatch(*node,#TYPE " array");log_once(failure);return std::unexpected(failure);} \
    std::vector<TYPE> result; result.reserve(node->value.size()); for(size_t i=0;i<node->value.size();++i){ \
        auto value=SCALAR(std::string(key)+"."+std::to_string(i)); if(!value)return std::unexpected(value.error()); result.push_back(std::move(*value));} return result; }
CONFIG_ARRAY_GETTER(get_int_array,std::int64_t,get_int)
CONFIG_ARRAY_GETTER(get_double_array,double,get_double)
CONFIG_ARRAY_GETTER(get_bool_array,bool,get_bool)
CONFIG_ARRAY_GETTER(get_string_array,std::string,get_string)
CONFIG_ARRAY_GETTER(get_vector2_array,elysia::core::Vector2,get_vector2)
CONFIG_ARRAY_GETTER(get_rect_array,elysia::core::Rect,get_rect)
#undef CONFIG_ARRAY_GETTER
}
