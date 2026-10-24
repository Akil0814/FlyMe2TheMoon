#include "resource_load_plan_validator.h"

#include "resource_load_plan.h"

#include <sstream>
#include <unordered_map>

namespace elysia::loading
{
namespace
{
template<typename Range, typename KeySelector>
bool validate_registry(
	const char* registry,
	const Range& requests,
	KeySelector select_key,
	ResourceLoadPlanValidationError& error)
{
	std::unordered_map<std::string, elysia::resources::ResourceOrigin> origins;
	for (const auto& request : requests)
	{
		const std::string& key = select_key(request);
		const auto [position, inserted] = origins.emplace(key, request.origin);
		if (!inserted)
		{
			error.registry = registry;
			error.key = key;
			error.first = position->second;
			error.second = request.origin;
			error.duplicate = true;
			error.origin = std::source_location::current();
			return false;
		}
	}
	return true;
}
}

std::string ResourceLoadPlanValidationError::describe() const
{
	if (!duplicate) return message;
	std::ostringstream stream;
	stream << "Duplicate " << registry << " key: " << key
		<< "\n  first:  " << first.describe()
		<< "\n  second: " << second.describe();
	return stream.str();
}

elysia::core::FailureDiagnostic ResourceLoadPlanValidationError::diagnostic() const
{
	std::vector<elysia::core::FailureDiagnosticEntry> entries;
	if (duplicate)
	{
		entries.push_back(elysia::core::make_failure_diagnostic_entry(
			registry,key,{},first.config_path,first.json_pointer,
			"first declaration",origin));
		entries.push_back(elysia::core::make_failure_diagnostic_entry(
			registry,key,{},second.config_path,second.json_pointer,
			"duplicate declaration",origin));
	}
	else if (!first.config_path.empty() || !first.json_pointer.empty())
	{
		entries.push_back(elysia::core::make_failure_diagnostic_entry(
			registry,key,{},first.config_path,first.json_pointer,message,origin));
	}
	return elysia::core::make_failure_diagnostic(describe(),std::move(entries),origin);
}

std::expected<void,ResourceLoadPlanValidationError> ResourceLoadPlanValidator::validate(
	const ResourceLoadPlan& plan) const
{
	ResourceLoadPlanValidationError error;
	if (!validate_registry("Atlas", plan.atlas_build_requests(),
		[](const auto& request) -> const std::string& { return request.atlas_key; }, error)
		|| !validate_registry("Animation", plan.animation_build_requests(),
			[](const auto& request) -> const std::string& { return request.animation_key; }, error)
		|| !validate_registry("Effect", plan.animation_effect_build_requests(),
			[](const auto& request) -> const std::string& { return request.effect_key; }, error)
		|| !validate_registry("Texture", plan.texture_requests(),
			[](const auto& request) -> const std::string& { return request.key; }, error)
		|| !validate_registry("Font", plan.font_requests(),
			[](const auto& request) -> const std::string& { return request.key; }, error)
		|| !validate_registry("Sound", plan.sound_requests(),
			[](const auto& request) -> const std::string& { return request.key; }, error)
		|| !validate_registry("Music", plan.music_requests(),
			[](const auto& request) -> const std::string& { return request.key; }, error))
		return std::unexpected(std::move(error));

	std::unordered_map<std::string, bool> atlas_keys;
	for (const auto& request : plan.atlas_build_requests()) atlas_keys.emplace(request.atlas_key, true);
	for (const auto& request : plan.animation_build_requests())
	{
		if (!atlas_keys.contains(request.atlas_key))
		{
			error.registry = "Animation";
			error.key = request.animation_key;
			error.first = request.origin;
			error.message = "Animation references missing Atlas key: " + request.atlas_key
				+ "\n  source: " + request.origin.describe();
			error.origin = std::source_location::current();
			return std::unexpected(std::move(error));
		}
	}
	std::unordered_map<std::string, bool> animation_keys;
	for (const auto& request : plan.animation_build_requests()) animation_keys.emplace(request.animation_key, true);
	for (const auto& request : plan.animation_effect_build_requests())
	{
		if (!animation_keys.contains(request.animation_key))
		{
			error.registry = "Effect";
			error.key = request.effect_key;
			error.first = request.origin;
			error.message = "Effect references missing Animation key: " + request.animation_key
				+ "\n  source: " + request.origin.describe();
			error.origin = std::source_location::current();
			return std::unexpected(std::move(error));
		}
	}
	return {};
}
}
