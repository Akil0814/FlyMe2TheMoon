#include "atlas_build_preparer.h"

#include "../texture/surface_loader.h"

#include <iomanip>
#include <sstream>
namespace elysia::resources
{
namespace
{
std::filesystem::path make_frame_path(
	const std::filesystem::path& directory_path,
	const std::string& filename_prefix,
	size_t frame_index
)
{
	std::ostringstream filename;
	filename << filename_prefix << '_' << std::setw(3) << std::setfill('0')
		<< frame_index << ".png";
	return directory_path / filename.str();
}
}

std::expected<std::vector<AtlasFramePrepareTask>,ResourceFailure>
AtlasBuildPreparer::expand_build_request(const AtlasBuildRequest& request) const
{
	std::vector<AtlasFramePrepareTask> out_tasks;

	if (!request.is_valid())
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,
			"Expand atlas build request failed: request is invalid.",
			"atlas",request.atlas_key,request.source_path,request.origin));

	if (request.source_type == AtlasSourceType::HorizontalStrip)
	{
		std::error_code error;
		if (!std::filesystem::is_regular_file(request.source_path,error))
			return std::unexpected(make_resource_failure(
				error && error != std::errc::no_such_file_or_directory
					? ResourceError::InvalidRequest : ResourceError::MissingFile,
				error && error != std::errc::no_such_file_or_directory
					? "Expand atlas build request failed: " + error.message()
					: "Expand atlas build request failed: horizontal strip does not exist.",
				"atlas-strip",request.atlas_key,request.source_path,request.origin));

		AtlasFramePrepareTask task;
		task.atlas_key = request.atlas_key;
		task.frame_path = request.source_path;
		task.frame_index = 0;
		task.expected_frame_count = request.frame_count;
		task.source_type = request.source_type;
		task.origin = request.origin;
		out_tasks.push_back(std::move(task));
		return out_tasks;
	}

	std::error_code directory_error;
	if (!std::filesystem::is_directory(request.source_path,directory_error))
		return std::unexpected(make_resource_failure(
			directory_error && directory_error != std::errc::no_such_file_or_directory
				? ResourceError::InvalidRequest : ResourceError::MissingFile,
			directory_error && directory_error != std::errc::no_such_file_or_directory
				? "Expand atlas build request failed: " + directory_error.message()
				: "Expand atlas build request failed: directory does not exist.",
			"atlas-directory",request.atlas_key,request.source_path,request.origin));

	std::vector<std::filesystem::path> frame_paths;
	frame_paths.reserve(request.frame_count);
	for (size_t index = 0; index < request.frame_count; ++index)
	{
		std::filesystem::path frame_path = make_frame_path(
			request.source_path,
			request.frame_filename_prefix,
			index
		);
		std::error_code frame_error;
		if (!std::filesystem::is_regular_file(frame_path,frame_error))
			return std::unexpected(make_resource_failure(
				frame_error && frame_error != std::errc::no_such_file_or_directory
					? ResourceError::InvalidRequest : ResourceError::MissingFile,
				frame_error && frame_error != std::errc::no_such_file_or_directory
					? "Expand atlas build request failed: " + frame_error.message()
					: "Expand atlas build request failed: expected frame is missing.",
				"atlas-frame",request.atlas_key,frame_path,request.origin));
		frame_paths.push_back(std::move(frame_path));
	}

	out_tasks.reserve(frame_paths.size());
	for (size_t index = 0; index < frame_paths.size(); ++index)
	{
		AtlasFramePrepareTask task;
		task.atlas_key = request.atlas_key;
		task.frame_path = frame_paths[index];
		task.frame_index = index;
		task.expected_frame_count = request.frame_count;
		task.source_type = request.source_type;
		task.origin = request.origin;
		out_tasks.push_back(std::move(task));
	}

	return out_tasks;
}

std::expected<AtlasFramePreparedResult,ResourceFailure>
AtlasBuildPreparer::prepare_frame(
	const AtlasFramePrepareTask& task
) const
{
	AtlasFramePreparedResult result;
	result.task = task;

	if (task.atlas_key.empty())
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Prepare atlas frame failed: atlas key is empty.",
			"atlas-frame",{},task.frame_path,task.origin));

	if (task.frame_path.empty())
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Prepare atlas frame failed: frame path is empty.",
			"atlas-frame",task.atlas_key,{},task.origin));

	if (task.expected_frame_count == 0)
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,
			"Prepare atlas frame failed: expected frame count is zero.",
			"atlas-frame",task.atlas_key,task.frame_path,task.origin));

	SurfaceLoadRequest surface_request;
	surface_request._asset_key = task.atlas_key;
	surface_request._subject_type = "atlas-frame";
	surface_request._frame_path = task.frame_path;
	surface_request._frame_index = task.frame_index;
	surface_request._origin = task.origin;

	SurfaceLoader surface_loader;
	auto surface_result = surface_loader.load_surface(surface_request);
	if (!surface_result)
		return std::unexpected(surface_result.error());
	result.surface_result = std::move(*surface_result);
	auto coverage_mask = create_coverage_mask_surface(*result.surface_result._surface);
	if (!coverage_mask)
		return std::unexpected(make_resource_failure(
			coverage_mask.error().code,coverage_mask.error().diagnostic.message,
			"atlas-frame",task.atlas_key,task.frame_path,task.origin,
			coverage_mask.error().diagnostic.origin));
	result.coverage_mask_surface = std::move(*coverage_mask);
	return result;
}

}
