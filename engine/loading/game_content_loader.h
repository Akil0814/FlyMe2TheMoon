#pragma once

#include <SDL3/SDL.h>

#include "resource_load_plan.h"
#include "content_load_failure.h"
#include "../io/loaders/asset_config_types.h"
#include "../resources/atlas/atlas_build_preparer.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <expected>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace elysia::config
{
class ConfigSnapshot;
}

namespace elysia::loading
{
enum class GameContentLoaderState
{
	Idle,
	PreparingRequests,
	StreamingTextureAndAtlasWork,
	LoadingFonts,
	LoadingAudio,
	RegisteringAnimations,
	RegisteringEffects,
	Finished,
	Failed
};

class GameContentLoader
{
public:
	GameContentLoader() = default;
	~GameContentLoader();

	void reset();

	[[nodiscard]] std::expected<void,ContentLoadFailure> start(
		SDL_Renderer* renderer,
		const elysia::io::ContentRegistry& content_registry,
		std::span<const int> project_font_point_sizes);
	void update();

	bool is_running() const;
	bool is_finished() const;
	bool has_failed() const;
	float progress() const;
	[[nodiscard]] const ContentLoadFailure* failure() const noexcept;
	GameContentLoaderState state() const;

private:
    friend struct GameContentLoaderTestAccess;
    enum class WorkerStage { Starting, Preparing, Publishing };
    // Internal injection seam; unused by normal loaders.
    void (*_worker_probe)(WorkerStage,std::size_t) = nullptr;
	template<typename T>
	struct PrepareOutcome
	{
		std::expected<T,elysia::resources::ResourceFailure> result;
	};

	struct PrepareJob
	{
		std::variant<elysia::resources::TextureLoadRequest, elysia::resources::AtlasFramePrepareTask> payload;
	};

	bool initialize_streaming_work();
	void reset_streaming_state();
	void start_worker_threads();
	void request_worker_stop();
	void shutdown_worker_threads();
	void worker_loop();
    bool handle_worker_exception();
	void dispatch_prepare_jobs();
	void drain_completed_prepare_results();
	bool commit_ready_streaming_results();
	bool commit_texture_result(const elysia::resources::SurfaceLoadResult& surface_result);
	bool commit_atlas_frame_result(const elysia::resources::AtlasFramePreparedResult& prepared_result);
	bool is_streaming_phase_complete();
	bool load_fonts();
	bool load_audio();
	bool register_animations();
	bool register_animation_effects();
	void update_progress_value();
	void fail(ContentLoadFailure failure);

private:
	SDL_Renderer* _renderer = nullptr;
	ResourceLoadPlan _load_plan;
	std::shared_ptr<const elysia::config::ConfigSnapshot> _config_snapshot;
	std::optional<ContentLoadFailure> _failure;
	GameContentLoaderState _state = GameContentLoaderState::Idle;
	float _progress = 0.0f;
	std::size_t _total_work_units = 0;
	std::size_t _completed_work_units = 0;

	std::vector<elysia::resources::AtlasFramePrepareTask> _atlas_frame_tasks;
	std::size_t _next_texture_request_index = 0;
	std::size_t _next_atlas_frame_task_index = 0;
	bool _dispatch_texture_turn = true;

	std::deque<PrepareJob> _prepare_jobs;
	std::mutex _prepare_mutex;
	std::condition_variable _prepare_cv;
	std::atomic<bool> _stop_workers = false;
	std::atomic<std::size_t> _in_flight_prepare_job_count = 0;
	std::vector<std::thread> _worker_threads;

	std::mutex _completed_results_mutex;
    struct WorkerException
    {
        std::exception_ptr cause;
        WorkerStage stage;
        std::optional<PrepareJob> job;
        std::source_location origin;
    };
    std::optional<WorkerException> _worker_exception;
	std::deque<PrepareOutcome<elysia::resources::SurfaceLoadResult>> _completed_texture_results;
	std::deque<PrepareOutcome<elysia::resources::AtlasFramePreparedResult>> _completed_atlas_frame_results;
	std::deque<PrepareOutcome<elysia::resources::SurfaceLoadResult>> _ready_texture_results;
	std::deque<PrepareOutcome<elysia::resources::AtlasFramePreparedResult>> _ready_atlas_frame_results;

	std::size_t _prepared_texture_count = 0;
	std::size_t _prepared_atlas_frame_count = 0;
	std::size_t _committed_texture_count = 0;
	std::size_t _committed_atlas_frame_count = 0;
	std::size_t _next_sound_request_index = 0;
	std::size_t _next_music_request_index = 0;
};

}
