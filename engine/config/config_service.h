#pragma once

#include "config_types.h"
#include "../core/geometry/rect.h"
#include "../core/geometry/vector2.h"
#include "../tools/singleton.h"

#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>


#define ELYSIA_CONFIG (::elysia::config::ConfigService::instance())

namespace elysia::config
{
class ConfigSnapshot;
class ConfigService final : public elysia::tools::Singleton<ConfigService>
{
    friend elysia::tools::Singleton<ConfigService>;
public:
    void publish(std::shared_ptr<const ConfigSnapshot> snapshot) noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool is_initialized() const noexcept;
    [[nodiscard]] bool contains(std::string_view key) const;

    [[nodiscard]] std::expected<std::int64_t,ConfigAccessFailure> get_int(std::string_view key) const;
    [[nodiscard]] std::expected<double,ConfigAccessFailure> get_double(std::string_view key) const;
    [[nodiscard]] std::expected<bool,ConfigAccessFailure> get_bool(std::string_view key) const;
    [[nodiscard]] std::expected<std::string,ConfigAccessFailure> get_string(std::string_view key) const;
    [[nodiscard]] std::expected<elysia::core::Vector2,ConfigAccessFailure> get_vector2(std::string_view key) const;
    [[nodiscard]] std::expected<elysia::core::Rect,ConfigAccessFailure> get_rect(std::string_view key) const;

    [[nodiscard]] std::expected<std::vector<std::int64_t>,ConfigAccessFailure> get_int_array(std::string_view key) const;
    [[nodiscard]] std::expected<std::vector<double>,ConfigAccessFailure> get_double_array(std::string_view key) const;
    [[nodiscard]] std::expected<std::vector<bool>,ConfigAccessFailure> get_bool_array(std::string_view key) const;
    [[nodiscard]] std::expected<std::vector<std::string>,ConfigAccessFailure> get_string_array(std::string_view key) const;
    [[nodiscard]] std::expected<std::vector<elysia::core::Vector2>,ConfigAccessFailure> get_vector2_array(std::string_view key) const;
    [[nodiscard]] std::expected<std::vector<elysia::core::Rect>,ConfigAccessFailure> get_rect_array(std::string_view key) const;

private:
    ConfigService() = default;
    void log_once(const ConfigAccessFailure& failure) const;
    mutable std::mutex _mutex;
    std::shared_ptr<const ConfigSnapshot> _snapshot;
    mutable std::unordered_set<std::string> _logged_access_errors;
};
}
