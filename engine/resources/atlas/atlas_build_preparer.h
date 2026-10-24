#pragma once

#include "atlas.h"
#include "../resource_types.h"
#include "../texture/surface_loader.h"

#include <vector>
#include <expected>

namespace elysia::resources
{
struct AtlasFramePrepareTask
{
	std::string atlas_key;
	std::filesystem::path frame_path;
	size_t frame_index = 0;
	size_t expected_frame_count = 0;
	AtlasSourceType source_type = AtlasSourceType::FrameDirectory;
	ResourceOrigin origin;
};

struct AtlasFramePreparedResult
{
	AtlasFramePrepareTask task;
	SurfaceLoadResult surface_result;
	SurfacePtr coverage_mask_surface;
};

class AtlasBuildPreparer
{
public:
	[[nodiscard]] std::expected<std::vector<AtlasFramePrepareTask>,ResourceFailure>
	expand_build_request(const AtlasBuildRequest& request) const;

	[[nodiscard]] std::expected<AtlasFramePreparedResult,ResourceFailure>
	prepare_frame(const AtlasFramePrepareTask& task) const;
};

}
