#pragma once

#include <filesystem>
#include <cstddef>
#include <optional>
#include <string>

namespace elysia::resources
{
enum class ResourceOriginScope
{
	Core,
	AdditionalModule
};

struct ResourceOrigin
{
	std::filesystem::path config_path;
	std::string json_pointer;
	std::string module;
	std::string capability;
	std::string entity_id;
	std::string logical_name;
	std::optional<size_t> segment_index;
	ResourceOriginScope scope = ResourceOriginScope::Core;

	[[nodiscard]] std::string describe() const;
};

[[nodiscard]] ResourceOrigin make_resource_origin(
	const std::filesystem::path& config_path,
	std::string json_pointer,
	std::string module,
	std::string capability,
	std::string entity_id = {},
	std::string logical_name = {},
	std::optional<size_t> segment_index = std::nullopt
);
}
