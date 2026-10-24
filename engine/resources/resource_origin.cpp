#include "resource_origin.h"

#include "../io/path/path_manager.h"

#include <sstream>
#include <utility>

namespace elysia::resources
{
std::string ResourceOrigin::describe() const
{
	std::ostringstream stream;
	stream << config_path.generic_string() << "#" << json_pointer;
	if (scope == ResourceOriginScope::Core)
		stream << "\n        scope=core module=core";
	else
		stream << "\n        scope=additional module=" << (module.empty() ? "\"\"" : module);
	stream << " capability=" << capability;
	if (!entity_id.empty()) stream << " entity=" << entity_id;
	if (!logical_name.empty()) stream << " logical=" << logical_name;
	if (segment_index) stream << " segment=" << *segment_index;
	return stream.str();
}

ResourceOrigin make_resource_origin(
	const std::filesystem::path& config_path,
	std::string json_pointer,
	std::string module,
	std::string capability,
	std::string entity_id,
	std::string logical_name,
	std::optional<size_t> segment_index)
{
	if (module.empty()) module = "core";
	std::filesystem::path relative_path = config_path.lexically_normal();
	if (const auto* paths = elysia::io::PathManager::instance(); paths && paths->is_initialized())
	{
		std::error_code error;
		const auto relative = std::filesystem::relative(relative_path, paths->root(), error);
		if (!error && !relative.empty() && *relative.begin() != "..")
			relative_path = relative.lexically_normal();
	}
	if (relative_path.is_absolute()) relative_path = relative_path.filename();
	return {
		std::move(relative_path), std::move(json_pointer), std::move(module),
		std::move(capability), std::move(entity_id), std::move(logical_name), segment_index
	};
}
}
