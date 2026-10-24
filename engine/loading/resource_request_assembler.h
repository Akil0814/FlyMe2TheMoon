#pragma once

#include <span>
#include <string>
#include <expected>
#include "content_load_failure.h"
#include "resource_load_plan.h"

namespace elysia::loading
{
struct ContentManifestResult;
class ResourceLoadPlan;

class ResourceRequestAssembler
{
public:
	[[nodiscard]] std::expected<ResourceLoadPlan,ContentLoadFailure> assemble(
		const ContentManifestResult& config_result,
		std::span<const int> project_font_point_sizes) const;
};

}
