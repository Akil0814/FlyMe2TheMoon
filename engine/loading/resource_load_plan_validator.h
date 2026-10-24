#pragma once

#include "../core/diagnostics/failure_diagnostic.h"
#include "../resources/resource_origin.h"

#include <string>
#include <expected>

namespace elysia::loading
{
class ResourceLoadPlan;

struct ResourceLoadPlanValidationError
{
	std::string registry;
	std::string key;
	elysia::resources::ResourceOrigin first;
	elysia::resources::ResourceOrigin second;
	std::string message;
	bool duplicate = false;
	std::source_location origin = std::source_location::current();

	[[nodiscard]] std::string describe() const;
	[[nodiscard]] elysia::core::FailureDiagnostic diagnostic() const;
};

class ResourceLoadPlanValidator
{
public:
	[[nodiscard]] std::expected<void,ResourceLoadPlanValidationError> validate(
		const ResourceLoadPlan& plan) const;
};
}
