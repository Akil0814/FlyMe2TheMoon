#include "atlas_builder.h"
namespace elysia::resources
{
std::expected<void,ResourceFailure> AtlasBuilder::build_atlas(
	const AtlasBuildRequest& request,
	const std::vector<AtlasCommittedFrame>& committed_frames,
	Atlas& atlas
) const
{
	if (request.atlas_key.empty())
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Build atlas failed: key is empty.",
			"atlas",request.atlas_key,request.source_path,request.origin));

	if (request.frame_count == 0)
	{
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Build atlas failed: frame count is zero.",
			"atlas",request.atlas_key,request.source_path,request.origin));
	}

	if (committed_frames.size() != request.frame_count)
	{
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidBuildState,
			"Build atlas failed: committed frame count does not match the request.",
			"atlas",request.atlas_key,request.source_path,request.origin));
	}

	atlas.clear();
	atlas.set_name(request.atlas_key);

	for (size_t index = 0; index < committed_frames.size(); ++index)
	{
		const AtlasCommittedFrame& committed_frame = committed_frames[index];
		if (!committed_frame.texture)
			return std::unexpected(make_resource_failure(
				ResourceError::InvalidBuildState,"Build atlas failed: frame texture is null.",
				"atlas-frame",request.atlas_key,committed_frame.frame_path,request.origin));
		if (!committed_frame.coverage_mask)
		{
			return std::unexpected(make_resource_failure(
				ResourceError::InvalidBuildState,"Build atlas failed: coverage mask is null.",
				"atlas-frame",request.atlas_key,committed_frame.frame_path,request.origin));
		}

		if (committed_frame.frame_index != index)
		{
			return std::unexpected(make_resource_failure(
				ResourceError::InvalidBuildState,"Build atlas failed: frame index mismatch.",
				"atlas-frame",request.atlas_key,committed_frame.frame_path,request.origin));
		}

		if (!atlas.add_frame(
			committed_frame.frame_path,
			committed_frame.texture,
			committed_frame.coverage_mask,
			committed_frame.source_rect))
		{
			return std::unexpected(make_resource_failure(
				ResourceError::InvalidBuildState,"Build atlas failed: frame could not be added.",
				"atlas-frame",request.atlas_key,committed_frame.frame_path,request.origin));
		}
	}

	return {};
}

}
