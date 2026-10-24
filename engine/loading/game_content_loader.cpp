#include "../tools/logger.h"
#include "game_content_loader.h"

#include "content_manifest_pipeline.h"
#include "content_runtime_cleanup.h"
#include "resource_request_assembler.h"
#include "../animation/runtime/animation_manager.h"
#include "../config/config_service.h"
#include "../effects/runtime/effect_manager.h"
#include "../io/path/path_manager.h"
#include "../resources/resource_service.h"
#include "../resources/runtime/resource_manager.h"
#include "../resources/texture/surface_loader.h"
#include "../resources/texture/texture_loader.h"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace elysia::loading
{
namespace
{
constexpr std::size_t kMaxInFlightPrepareJobs = 16;
constexpr std::size_t kTextureCommitBudgetPerUpdate = 4;
constexpr std::size_t kAtlasFrameCommitBudgetPerUpdate = 8;
constexpr std::size_t kAudioLoadBudgetPerUpdate = 8;

ContentLoadFailure from_resource_failure(
	ContentLoadError code,elysia::resources::ResourceFailure failure)
{
	if (const auto* paths = elysia::io::PathManager::instance();
		paths && paths->is_initialized())
		elysia::core::normalize_failure_diagnostic_paths(
			failure.diagnostic,paths->root());
	return make_content_load_failure(code,std::move(failure.diagnostic));
}

std::size_t resolve_worker_count(std::size_t total_prepare_jobs)
{
	if (total_prepare_jobs == 0)
		return 0;

	std::size_t worker_count = std::thread::hardware_concurrency();
	if (worker_count == 0)
		worker_count = 2;

	worker_count = std::min<std::size_t>(worker_count, 4);
	return std::min(worker_count, total_prepare_jobs);
}
}

GameContentLoader::~GameContentLoader()
{
	reset();
}

void GameContentLoader::reset()
{
	const bool was_loading = is_running();
	shutdown_worker_threads();
	reset_streaming_state();
	if (was_loading)
		clear_loaded_content();
	_renderer = nullptr;
	_load_plan.clear();
	_config_snapshot.reset();
	_failure.reset();
	_state = GameContentLoaderState::Idle;
	_progress = 0.0f;
	_total_work_units = 0;
	_completed_work_units = 0;
}

std::expected<void,ContentLoadFailure> GameContentLoader::start(
	SDL_Renderer* renderer,
	const elysia::io::ContentRegistry& content_registry,
	std::span<const int> project_font_point_sizes)
{
	reset();
	clear_loaded_content();

	if (!renderer)
	{
		ContentLoadFailure failure = make_content_load_failure(ContentLoadError::Plan,
			"GameContentLoader start failed: renderer is null.");
		fail(failure);
		return std::unexpected(std::move(failure));
	}

	_renderer = renderer;
	_state = GameContentLoaderState::PreparingRequests;

	elysia::io::PathManager* path_manager = elysia::io::PathManager::instance();
	if (!path_manager->is_initialized())
	{
		ContentLoadFailure failure = make_content_load_failure(ContentLoadError::Config,
			"GameContentLoader start failed: path manager is not initialized.");
		fail(failure);
		return std::unexpected(std::move(failure));
	}

	ContentManifestPipeline content_manifest_pipeline;
	auto config_result = content_manifest_pipeline.load(content_registry);
	if (!config_result)
	{
		ContentLoadFailure failure = std::move(config_result.error());
		fail(failure);
		return std::unexpected(std::move(failure));
	}
	_config_snapshot = config_result->config_snapshot;

	ResourceRequestAssembler assembler;
	auto load_plan = assembler.assemble(*config_result,project_font_point_sizes);
	if (!load_plan)
	{
		ContentLoadFailure failure = std::move(load_plan.error());
		fail(failure);
		return std::unexpected(std::move(failure));
	}
	_load_plan = std::move(*load_plan);

	try
	{
		if (!initialize_streaming_work())
			return std::unexpected(*_failure);
	}
	catch (const std::exception& error)
	{
		fail(make_content_load_failure(ContentLoadError::Plan,
			std::string("Content worker startup failed: ") + error.what()));
		return std::unexpected(*_failure);
	}
	catch (...)
	{
		fail(make_content_load_failure(ContentLoadError::Plan,"Unknown content worker startup exception."));
		return std::unexpected(*_failure);
	}

	_state = GameContentLoaderState::StreamingTextureAndAtlasWork;
	_progress = 0.0f;

	ELYSIA_LOG("resource","Total requests built: " << _load_plan.total_request_count()
		<< ", total work units: " << _total_work_units);
	return {};
}

void GameContentLoader::update()
{
	if (_state == GameContentLoaderState::Idle
		|| _state == GameContentLoaderState::Finished
		|| _state == GameContentLoaderState::Failed)
	{
		return;
	}

	if (_state == GameContentLoaderState::StreamingTextureAndAtlasWork)
	{
		if (!_renderer)
		{
			fail(make_content_load_failure(ContentLoadError::Texture,
				"GameContentLoader update failed: renderer is null."));
			return;
		}

		if (handle_worker_exception()) return;
		dispatch_prepare_jobs();
		if (handle_worker_exception()) return;
		drain_completed_prepare_results();
		if (handle_worker_exception()) return;
		if (!commit_ready_streaming_results())
			return;

		if (is_streaming_phase_complete())
		{
			shutdown_worker_threads();
			_state = GameContentLoaderState::LoadingFonts;
		}

		update_progress_value();
		return;
	}

	if (_state == GameContentLoaderState::LoadingFonts)
	{
		if (!load_fonts())
			return;

		_state = GameContentLoaderState::LoadingAudio;
		update_progress_value();
		return;
	}

	if (_state == GameContentLoaderState::LoadingAudio)
	{
		if (!load_audio())
			return;

		_state = GameContentLoaderState::RegisteringAnimations;
		update_progress_value();
		return;
	}

	if (_state == GameContentLoaderState::RegisteringAnimations)
	{
		if (!register_animations())
			return;

		_state = GameContentLoaderState::RegisteringEffects;
		update_progress_value();
		return;
	}

	if (_state == GameContentLoaderState::RegisteringEffects)
	{
		if (!register_animation_effects())
			return;
		if (!_config_snapshot)
		{
			fail(make_content_load_failure(ContentLoadError::Config,
				"GameContentLoader config publish failed: snapshot is missing."));
			return;
		}
		elysia::config::ConfigService::instance()->publish(_config_snapshot);

		_state = GameContentLoaderState::Finished;
		_progress = 1.0f;
	}
}

bool GameContentLoader::is_running() const
{
	return _state == GameContentLoaderState::PreparingRequests
		|| _state == GameContentLoaderState::StreamingTextureAndAtlasWork
		|| _state == GameContentLoaderState::LoadingFonts
		|| _state == GameContentLoaderState::LoadingAudio
		|| _state == GameContentLoaderState::RegisteringAnimations
		|| _state == GameContentLoaderState::RegisteringEffects;
}

bool GameContentLoader::is_finished() const
{
	return _state == GameContentLoaderState::Finished;
}

bool GameContentLoader::has_failed() const
{
	return _state == GameContentLoaderState::Failed;
}

float GameContentLoader::progress() const
{
	return _progress;
}

const ContentLoadFailure* GameContentLoader::failure() const noexcept
{
	return _failure ? &*_failure : nullptr;
}

GameContentLoaderState GameContentLoader::state() const
{
	return _state;
}

bool GameContentLoader::initialize_streaming_work()
{
	elysia::resources::AtlasBuildPreparer atlas_build_preparer;
	std::vector<elysia::resources::AtlasFramePrepareTask> atlas_frame_tasks;
	for (const elysia::resources::AtlasBuildRequest& request : _load_plan.atlas_build_requests())
	{
		auto expanded_tasks = atlas_build_preparer.expand_build_request(request);
		if (!expanded_tasks)
		{
			fail(from_resource_failure(ContentLoadError::Atlas,
				std::move(expanded_tasks.error())));
			return false;
		}

		atlas_frame_tasks.insert(
			atlas_frame_tasks.end(),
			std::make_move_iterator(expanded_tasks->begin()),
			std::make_move_iterator(expanded_tasks->end())
		);
	}

	elysia::resources::ResourceManager* resource_manager = elysia::resources::ResourceManager::instance();
	auto begin_result = resource_manager->begin_atlas_builds(_load_plan.atlas_build_requests());
	if (!begin_result)
	{
		fail(from_resource_failure(ContentLoadError::Atlas,
			std::move(begin_result.error())));
		return false;
	}

	_atlas_frame_tasks = std::move(atlas_frame_tasks);
	_total_work_units = _load_plan.texture_requests().size()
		+ _atlas_frame_tasks.size()
		+ _load_plan.font_requests().size()
		+ _load_plan.sound_requests().size()
		+ _load_plan.music_requests().size()
		+ _load_plan.animation_build_requests().size()
		+ _load_plan.animation_effect_build_requests().size();

	start_worker_threads();
	return true;
}

void GameContentLoader::reset_streaming_state()
{
	_atlas_frame_tasks.clear();
	_next_texture_request_index = 0;
	_next_atlas_frame_task_index = 0;
	_dispatch_texture_turn = true;
	_prepare_jobs.clear();
	_worker_exception.reset();
	_completed_texture_results.clear();
	_completed_atlas_frame_results.clear();
	_ready_texture_results.clear();
	_ready_atlas_frame_results.clear();
	_in_flight_prepare_job_count.store(0);
	_stop_workers.store(false);
	_prepared_texture_count = 0;
	_prepared_atlas_frame_count = 0;
	_committed_texture_count = 0;
	_committed_atlas_frame_count = 0;
	_next_sound_request_index = 0;
	_next_music_request_index = 0;
}

void GameContentLoader::start_worker_threads()
{
	const std::size_t total_prepare_jobs =
		_load_plan.texture_requests().size() + _atlas_frame_tasks.size();
	const std::size_t worker_count = resolve_worker_count(total_prepare_jobs);
	if (worker_count == 0)
		return;

	try
	{
		_worker_threads.reserve(worker_count);
		for (std::size_t index = 0; index < worker_count; ++index)
		{
			if (_worker_probe) _worker_probe(WorkerStage::Starting,index);
			_worker_threads.emplace_back(&GameContentLoader::worker_loop, this);
		}
	}
	catch (...)
	{
		shutdown_worker_threads();
		throw;
	}
}

void GameContentLoader::request_worker_stop()
{
	// Publish the predicate while holding the wait mutex, so notification cannot
	// slip between a worker's predicate check and its actual wait.
	{
		std::lock_guard<std::mutex> lock(_prepare_mutex);
		_stop_workers.store(true);
	}
	_prepare_cv.notify_all();
}

void GameContentLoader::shutdown_worker_threads()
{
	request_worker_stop();

	for (std::thread& worker : _worker_threads)
	{
		if (worker.joinable())
			worker.join();
	}
	_worker_threads.clear();
	// Producers run on the main thread; after join no worker owns a task.
	std::lock_guard<std::mutex> lock(_prepare_mutex);
	_prepare_jobs.clear();
	_in_flight_prepare_job_count.store(0);
}

void GameContentLoader::worker_loop()
{
	std::optional<PrepareJob> active_job;
	WorkerStage worker_stage = WorkerStage::Starting;
	try
	{
		elysia::resources::SurfaceLoader surface_loader;
		elysia::resources::AtlasBuildPreparer atlas_build_preparer;
		for (;;)
		{
			{
				std::unique_lock<std::mutex> lock(_prepare_mutex);
				_prepare_cv.wait(lock,[this] { return _stop_workers.load() || !_prepare_jobs.empty(); });
				if (_stop_workers.load()) return;
				active_job.emplace(std::move(_prepare_jobs.front()));
				_prepare_jobs.pop_front();
			}
			struct CompletionGuard
			{
				std::atomic<std::size_t>& count;
				~CompletionGuard() { count.fetch_sub(1); }
			} completion{_in_flight_prepare_job_count};
			worker_stage = WorkerStage::Preparing;
			if (_worker_probe) _worker_probe(worker_stage,0);
			if (const auto* texture = std::get_if<elysia::resources::TextureLoadRequest>(&active_job->payload))
			{
				elysia::resources::SurfaceLoadRequest request;
				request._asset_key = texture->key;
				request._subject_type = "texture";
				request._frame_path = texture->file_path;
				request._origin = texture->origin;
				auto result = surface_loader.load_surface(request);
				worker_stage = WorkerStage::Publishing;
				if (_worker_probe) _worker_probe(worker_stage,0);
				std::lock_guard<std::mutex> lock(_completed_results_mutex);
				_completed_texture_results.push_back({std::move(result)});
			}
			else
			{
				auto result = atlas_build_preparer.prepare_frame(
					std::get<elysia::resources::AtlasFramePrepareTask>(active_job->payload));
				worker_stage = WorkerStage::Publishing;
				if (_worker_probe) _worker_probe(worker_stage,0);
				std::lock_guard<std::mutex> lock(_completed_results_mutex);
				_completed_atlas_frame_results.push_back({std::move(result)});
			}
			active_job.reset();
		}
	}
	catch (...)
	{
		// Moving the job and exception_ptr into reserved member storage requires no queue allocation.
		{
			std::lock_guard<std::mutex> lock(_completed_results_mutex);
			if (!_worker_exception)
				_worker_exception.emplace(WorkerException{std::current_exception(),worker_stage,
					std::move(active_job),std::source_location::current()});
		}
		request_worker_stop();
	}
}

bool GameContentLoader::handle_worker_exception()
{
	std::optional<WorkerException> failure;
	{
		std::lock_guard<std::mutex> lock(_completed_results_mutex);
		if (!_worker_exception) return false;
		failure = std::move(_worker_exception);
		_worker_exception.reset();
	}
	shutdown_worker_threads();
	ContentLoadError stage = ContentLoadError::Plan;
	std::string key;
	std::filesystem::path path;
	elysia::resources::ResourceOrigin declaration;
	if (failure->job)
	{
		if (const auto* texture = std::get_if<elysia::resources::TextureLoadRequest>(&failure->job->payload))
		{
			stage = ContentLoadError::Texture;
			key = texture->key; path = texture->file_path; declaration = texture->origin;
		}
		else
		{
			const auto& atlas = std::get<elysia::resources::AtlasFramePrepareTask>(failure->job->payload);
			stage = ContentLoadError::Atlas;
			key = atlas.atlas_key; path = atlas.frame_path; declaration = atlas.origin;
		}
	}
	std::string message = "Unknown content worker exception.";
	try { std::rethrow_exception(failure->cause); }
	catch (const std::exception& error) { message = error.what(); }
	catch (...) {}
	const char* phase = failure->stage == WorkerStage::Publishing ? "publish"
		: failure->stage == WorkerStage::Preparing ? "prepare" : "initialize";
	message = std::string("Content worker ") + phase + ": " + message;
	fail(from_resource_failure(stage,elysia::resources::make_resource_failure(
		elysia::resources::ResourceError::DecodeFailed,std::move(message),
		std::string(content_load_subject_type(stage)),std::move(key),std::move(path),
		std::move(declaration),failure->origin)));
	return true;
}

void GameContentLoader::dispatch_prepare_jobs()
{
	auto try_enqueue_texture = [this]() -> bool
	{
		if (_next_texture_request_index >= _load_plan.texture_requests().size())
			return false;

		PrepareJob job;
		job.payload = _load_plan.texture_requests()[_next_texture_request_index++];
		{
			std::lock_guard<std::mutex> lock(_prepare_mutex);
			if (_stop_workers.load()) return false;
			_prepare_jobs.push_back(std::move(job));
			_in_flight_prepare_job_count.fetch_add(1);
		}

		_prepare_cv.notify_one();
		return true;
	};

	auto try_enqueue_atlas = [this]() -> bool
	{
		if (_next_atlas_frame_task_index >= _atlas_frame_tasks.size())
			return false;

		PrepareJob job;
		job.payload = _atlas_frame_tasks[_next_atlas_frame_task_index++];
		{
			std::lock_guard<std::mutex> lock(_prepare_mutex);
			if (_stop_workers.load()) return false;
			_prepare_jobs.push_back(std::move(job));
			_in_flight_prepare_job_count.fetch_add(1);
		}

		_prepare_cv.notify_one();
		return true;
	};

	while (_in_flight_prepare_job_count.load() < kMaxInFlightPrepareJobs)
	{
		bool enqueued = false;
		if (_dispatch_texture_turn)
			enqueued = try_enqueue_texture() || try_enqueue_atlas();
		else
			enqueued = try_enqueue_atlas() || try_enqueue_texture();

		if (!enqueued)
			break;

		_dispatch_texture_turn = !_dispatch_texture_turn;
	}
}

void GameContentLoader::drain_completed_prepare_results()
{
	std::lock_guard<std::mutex> lock(_completed_results_mutex);

	while (!_completed_texture_results.empty())
	{
		_ready_texture_results.push_back(std::move(_completed_texture_results.front()));
		_completed_texture_results.pop_front();
		++_prepared_texture_count;
	}

	while (!_completed_atlas_frame_results.empty())
	{
		_ready_atlas_frame_results.push_back(
			std::move(_completed_atlas_frame_results.front())
		);
		_completed_atlas_frame_results.pop_front();
		++_prepared_atlas_frame_count;
	}
}

bool GameContentLoader::commit_ready_streaming_results()
{
	std::size_t committed_texture_count = 0;
	while (committed_texture_count < kTextureCommitBudgetPerUpdate
		&& !_ready_texture_results.empty())
	{
		auto surface_result = std::move(_ready_texture_results.front().result);
		_ready_texture_results.pop_front();
		if (!surface_result)
		{
			fail(from_resource_failure(ContentLoadError::Texture,
				std::move(surface_result.error())));
			return false;
		}
		if (!commit_texture_result(*surface_result))
			return false;

		++committed_texture_count;
	}

	std::size_t committed_atlas_count = 0;
	while (committed_atlas_count < kAtlasFrameCommitBudgetPerUpdate
		&& !_ready_atlas_frame_results.empty())
	{
		auto prepared_result =
			std::move(_ready_atlas_frame_results.front().result);
		_ready_atlas_frame_results.pop_front();
		if (!prepared_result)
		{
			fail(from_resource_failure(ContentLoadError::Atlas,
				std::move(prepared_result.error())));
			return false;
		}
		if (!commit_atlas_frame_result(*prepared_result))
			return false;

		++committed_atlas_count;
	}

	return true;
}

bool GameContentLoader::commit_texture_result(const elysia::resources::SurfaceLoadResult& surface_result)
{
	if (!surface_result._surface)
	{
		fail(from_resource_failure(ContentLoadError::Texture,
			elysia::resources::make_resource_failure(
				elysia::resources::ResourceError::InvalidRequest,
				"GameContentLoader texture commit failed: prepared surface is invalid.",
				"texture",surface_result._asset_key,surface_result._frame_path,
				surface_result._origin)));
		return false;
	}

	elysia::resources::TextureLoader texture_loader;
	auto texture_result =
		texture_loader.load_texture(_renderer, surface_result);
	if (!texture_result)
	{
		fail(from_resource_failure(ContentLoadError::Texture,
			std::move(texture_result.error())));
		return false;
	}

	elysia::resources::ResourceManager* resource_manager = elysia::resources::ResourceManager::instance();
	auto store_result = resource_manager->store_texture(
		surface_result._asset_key,
		std::move(texture_result->_texture),surface_result._origin,
		surface_result._frame_path);
	if (!store_result)
	{
		fail(from_resource_failure(ContentLoadError::Texture,
			std::move(store_result.error())));
		return false;
	}

	++_committed_texture_count;
	++_completed_work_units;
	return true;
}

bool GameContentLoader::commit_atlas_frame_result(
	const elysia::resources::AtlasFramePreparedResult& prepared_result
)
{
	if (!prepared_result.surface_result._surface)
	{
		fail(from_resource_failure(ContentLoadError::Atlas,
			elysia::resources::make_resource_failure(
				elysia::resources::ResourceError::InvalidRequest,
				"GameContentLoader atlas frame commit failed: prepared surface is invalid.",
				"atlas-frame",prepared_result.task.atlas_key,
				prepared_result.task.frame_path,prepared_result.task.origin)));
		return false;
	}

	auto commit_result = elysia::resources::ResourceManager::instance()->commit_prepared_atlas_frame(
		_renderer,
		prepared_result);
	if (!commit_result)
	{
		fail(from_resource_failure(ContentLoadError::Atlas,
			std::move(commit_result.error())));
		return false;
	}

	++_committed_atlas_frame_count;
	++_completed_work_units;
	return true;
}

bool GameContentLoader::is_streaming_phase_complete()
{
	if (_next_texture_request_index != _load_plan.texture_requests().size())
		return false;

	if (_next_atlas_frame_task_index != _atlas_frame_tasks.size())
		return false;

	if (_committed_texture_count != _load_plan.texture_requests().size())
		return false;

	if (_committed_atlas_frame_count != _atlas_frame_tasks.size())
		return false;

	if (_in_flight_prepare_job_count.load() != 0)
		return false;

	if (!_ready_texture_results.empty() || !_ready_atlas_frame_results.empty())
		return false;

	{
		std::lock_guard<std::mutex> lock(_completed_results_mutex);
		if (!_completed_texture_results.empty()
			|| !_completed_atlas_frame_results.empty())
		{
			return false;
		}
	}

	return !elysia::resources::ResourceManager::instance()->has_in_progress_atlas_builds();
}

bool GameContentLoader::load_fonts()
{
	elysia::resources::ResourceManager* resource_manager = elysia::resources::ResourceManager::instance();
	for (const elysia::resources::FontLoadRequest& request : _load_plan.font_requests())
	{
		auto result = resource_manager->load_font(request);
		if (!result)
		{
			fail(from_resource_failure(ContentLoadError::Font,std::move(result.error())));
			return false;
		}

		++_completed_work_units;
	}

	return true;
}

bool GameContentLoader::load_audio()
{
	elysia::resources::ResourceManager* resource_manager = elysia::resources::ResourceManager::instance();
	std::size_t loaded_this_update = 0;

	while (_next_sound_request_index < _load_plan.sound_requests().size()
		&& loaded_this_update < kAudioLoadBudgetPerUpdate)
	{
		const elysia::resources::SoundLoadRequest& request =
			_load_plan.sound_requests()[_next_sound_request_index];
		auto result = resource_manager->load_sound(request);
		if (!result)
		{
			fail(from_resource_failure(ContentLoadError::Audio,std::move(result.error())));
			return false;
		}

		++_next_sound_request_index;
		++loaded_this_update;
		++_completed_work_units;
	}

	while (_next_music_request_index < _load_plan.music_requests().size()
		&& loaded_this_update < kAudioLoadBudgetPerUpdate)
	{
		const elysia::resources::MusicLoadRequest& request =
			_load_plan.music_requests()[_next_music_request_index];
		auto result = resource_manager->load_music(request);
		if (!result)
		{
			fail(from_resource_failure(ContentLoadError::Audio,std::move(result.error())));
			return false;
		}

		++_next_music_request_index;
		++loaded_this_update;
		++_completed_work_units;
	}

	update_progress_value();
	return _next_sound_request_index == _load_plan.sound_requests().size()
		&& _next_music_request_index == _load_plan.music_requests().size();
}

bool GameContentLoader::register_animations()
{
	elysia::animation::AnimationManager* animation_manager = elysia::animation::AnimationManager::instance();
	for (const elysia::resources::AnimationBuildRequest& request : _load_plan.animation_build_requests())
	{
		const elysia::resources::Atlas* atlas =
			elysia::resources::ResourceService::instance()->find_atlas(request.atlas_key);
		auto result = animation_manager->register_animation(request, atlas);
		if (!result)
		{
			fail(make_content_load_failure(ContentLoadError::Animation,
				std::move(result.error().diagnostic)));
			return false;
		}

		++_completed_work_units;
	}

	return true;
}

bool GameContentLoader::register_animation_effects()
{
	elysia::effects::EffectManager* effect_manager = elysia::effects::EffectManager::instance();
	for (const elysia::resources::AnimationEffectBuildRequest& request : _load_plan.animation_effect_build_requests())
	{
		auto result = effect_manager->register_animation_effect(request);
		if (!result)
		{
			fail(make_content_load_failure(ContentLoadError::Effect,
				std::move(result.error().diagnostic)));
			return false;
		}

		++_completed_work_units;
	}

	return true;
}

void GameContentLoader::update_progress_value()
{
	if (_state == GameContentLoaderState::Finished)
	{
		_progress = 1.0f;
		return;
	}

	if (_total_work_units == 0)
	{
		_progress = 0.0f;
		return;
	}

	const float ratio = static_cast<float>(_completed_work_units)
		/ static_cast<float>(_total_work_units);
	_progress = std::clamp(ratio, 0.0f, 1.0f);
}

void GameContentLoader::fail(ContentLoadFailure failure)
{
	shutdown_worker_threads();
	reset_streaming_state();
	_stop_workers.store(true);
	clear_loaded_content();
	_config_snapshot.reset();
	_failure = std::move(failure);
	_state = GameContentLoaderState::Failed;
	update_progress_value();
}


}
