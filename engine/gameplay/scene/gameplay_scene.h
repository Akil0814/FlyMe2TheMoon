#pragma once
#include "../../scene/scene.h"
#include "../collision/gameplay_collision_runtime.h"
#include "../control/controller_service.h"
namespace elysia::gameplay
{
struct GameplaySceneFeatures
{
    elysia::scene::FixedStepConfig fixed_step{};
    std::optional<elysia::physics::PhysicsWorldConfig> physics;
    bool gameplay_collision = false;
    std::optional<elysia::scene::CameraSceneConfig> camera;
};

class GameplayScene : public elysia::scene::Scene
{
  public:
    GameplayScene();
    explicit GameplayScene(GameplaySceneFeatures features);
    ~GameplayScene() override;
    SceneControlContext &control_context()
    {
        return _control_context;
    }
    [[nodiscard]] bool has_collision_runtime() const noexcept
    {
        return static_cast<bool>(_collision_runtime);
    }

  protected:
    [[nodiscard]] collision::GameplayCollisionRuntime* try_collision_runtime() noexcept;
    [[nodiscard]] collision::GameplayCollisionRuntime& collision_runtime();
    virtual void on_game_fixed_update(
        [[maybe_unused]] std::uint64_t tick, [[maybe_unused]] double delta)
    {
    }
    virtual void on_control_target_removing([[maybe_unused]] elysia::core::SceneObject &object)
    {
    }

  private:
    friend class ControllerManager;
    bool contains_control_target(const elysia::core::GameObject *) const;
    void on_routed_input(const elysia::input::InputSnapshot &) final;
    void on_fixed_update(std::uint64_t, double) final;
    void on_pause_changed(bool) final;
    void on_scene_object_removing(elysia::core::SceneObject &) final;
    void on_runtime_attach() final;
    void on_runtime_detach() final;
    void on_runtime_reset() final;
    std::unique_ptr<collision::GameplayCollisionRuntime> _collision_runtime;
    SceneControlContext _control_context{*this};
};
} // namespace elysia::gameplay
