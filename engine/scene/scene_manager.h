#pragma once

#include <SDL3/SDL.h>

#include <functional>
#include <expected>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "detail/scene_factory.h"
#include "routing/scene_request.h"
#include "routing/scene_request_observer.h"
#include "scene.h"
#include "scene_boundary_failure.h"
#include "scene_manager_observer.h"

#include "../core/event/subject.h"

namespace elysia::scene
{
namespace detail { class SceneFailureCollector; }
enum class SceneManagerState
{
    Constructed,
    Ready,
    Running,
    ShuttingDown,
    Stopped,
    Faulted
};

using SceneFailureRouteFactory =
    std::function<SceneRoute(const SceneBoundaryFailure&)>;

class SceneManager : public elysia::core::Subject<SceneManagerObserver>,
                     public SceneRequestObserver
{
public:
    SceneManager() = default;
    ~SceneManager();
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;
    SceneManager(SceneManager&&) = delete;
    SceneManager& operator=(SceneManager&&) = delete;

    void initialize(
        const SceneRuntimeContext& context,
        SceneFailureRouteFactory failure_route_factory = {});

    template <typename T, typename... Args>
    void register_game_scene(SceneKey scene_key, Args&&... args);

    template <typename T, typename... Args>
    void register_engine_scene(SceneKey scene_key, Args&&... args);

    void start(const SceneRoute& route);
    void on_input(const elysia::input::InputSnapshot& input);
    void on_update(double delta);
    void on_render(SDL_Renderer* renderer);
    void on_scene_request(const SceneRequest& request) override;

    [[nodiscard]] SceneManagerState state() const noexcept { return _state; }
    [[nodiscard]] SceneKey current_scene_key() const noexcept { return _current_scene_key; }
    [[nodiscard]] elysia::input::LocalPlayerRegistry& local_players() noexcept
    {
        return _local_players;
    }

    bool shutdown() noexcept;

private:
    using SceneBuilder = std::function<std::unique_ptr<Scene>()>;

    template <typename T, typename... Args>
    void add_scene_builder(SceneKey scene_key, Args&&... args);

    void ensure_ready_for_registration() const;
    void notify_quit_requested();
    void notify_fault(const SceneBoundaryFailure& failure) noexcept;
    void process_pending_request();
    void recover_from_failure(const SceneBoundaryFailure& failure);

    [[nodiscard]] std::expected<void, SceneBoundaryFailure>
        switch_to_registered_scene(const SceneRoute& route);
    [[nodiscard]] std::expected<void, SceneBoundaryFailure>
        switch_to_scene(Scene* next_scene, std::unique_ptr<Scene> staged_scene,
                        const SceneRoute& route);
    void leave_current_scene(detail::SceneFailureCollector& failures);

    void attach_to_scene(Scene& scene,SceneKey key);
    void detach_from_scene(Scene& scene,SceneKey key);
    void discard_scene(SceneKey key, Scene* expected);
    [[nodiscard]] SceneBoundaryFailure make_failure(
        SceneKey key, SceneBoundary boundary,
        std::source_location origin = std::source_location::current()) const;
    [[noreturn]] static void throw_invalid_route_key(SceneKey key);

    SceneManagerState _state = SceneManagerState::Constructed;
    bool _shutdown_succeeded = true;
    bool _shutdown_performed = false;
    bool _recovering_failure = false;
    Scene* _current_scene = nullptr;
    SceneKey _current_scene_key = SceneKeys::Invalid;

    elysia::input::LocalPlayerRegistry _local_players;
    elysia::scene::UiDeviceAccess _ui_device_access;
    SceneFactory _scene_factory;
    std::unordered_map<SceneKey, SceneBuilder> _scene_builders;
    const SceneRuntimeContext* _runtime_context = nullptr;
    SceneFailureRouteFactory _failure_route_factory;

    SceneRequest _pending_request{};
    bool _has_pending_request = false;
    bool _is_processing_request = false;
};

template <typename T, typename... Args>
void SceneManager::register_game_scene(SceneKey scene_key, Args&&... args)
{
    if (!SceneKeys::is_game(scene_key))
    {
        throw std::logic_error(
            "SceneManager::register_game_scene received a SceneKey outside the game range [1, 999].");
    }
    add_scene_builder<T>(scene_key, std::forward<Args>(args)...);
}

template <typename T, typename... Args>
void SceneManager::register_engine_scene(SceneKey scene_key, Args&&... args)
{
    if (!SceneKeys::is_engine_owned(scene_key))
    {
        throw std::logic_error(
            "SceneManager::register_engine_scene received a SceneKey outside the engine-owned keys.");
    }
    add_scene_builder<T>(scene_key, std::forward<Args>(args)...);
}

template <typename T, typename... Args>
void SceneManager::add_scene_builder(SceneKey scene_key, Args&&... args)
{
    static_assert(std::is_base_of_v<Scene, T>, "T must derive from Scene.");
    ensure_ready_for_registration();
    if (_scene_builders.contains(scene_key))
        throw std::logic_error("SceneManager scene registration received a duplicate SceneKey.");

    using StoredArguments = std::tuple<std::decay_t<Args>...>;
    static_assert(std::is_copy_constructible_v<StoredArguments>,
                  "Scene registration arguments must be copyable. Use std::ref for borrowed dependencies.");
    static_assert(std::is_constructible_v<T, std::decay_t<Args>&...>,
                  "The scene must be constructible from reusable registration arguments.");

    _scene_builders.emplace(
        scene_key,
        [constructor_args = StoredArguments(std::forward<Args>(args)...)]() mutable
            -> std::unique_ptr<Scene> {
            return std::apply(
                [](auto&... stored_args) -> std::unique_ptr<Scene> {
                    return std::make_unique<T>(stored_args...);
                },
                constructor_args);
        });
}
} // namespace elysia::scene
