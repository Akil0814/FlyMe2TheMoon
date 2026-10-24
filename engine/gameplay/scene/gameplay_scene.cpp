#include "gameplay_scene.h"
#include "../control/controller_manager.h"
#include "../collision/gameplay_collision_service.h"
#include "../../physics/contracts/physics_participant.h"
#include "../../scene/detail/scene_failure_log.h"

#include <exception>
namespace elysia::gameplay
{
namespace
{
elysia::scene::SceneRuntimeFeatures scene_features(const GameplaySceneFeatures& features)
{
    return elysia::scene::SceneRuntimeFeatures{
        .fixed_step = features.fixed_step,
        .physics = features.physics,
        .camera = features.camera};
}
} // namespace

GameplayScene::GameplayScene() : GameplayScene(GameplaySceneFeatures{}) {}

GameplayScene::GameplayScene(GameplaySceneFeatures features)
    : Scene(scene_features(features))
{
    if (features.gameplay_collision && !features.physics)
        throw std::invalid_argument("Gameplay collision requires a physics runtime.");
    if (features.gameplay_collision)
        _collision_runtime = std::make_unique<collision::GameplayCollisionRuntime>(physics_world());

    input_router().set_auto_claim_ui_gamepad(false);
    set_ui_interaction_mode(elysia::scene::UiInteractionMode::Pointer);
    input_router().set_cancel_handler([this](auto player, auto reason) {
        ControllerManager::instance()->cancel_local(_control_context, player, reason);
    });
}
GameplayScene::~GameplayScene() = default;

collision::GameplayCollisionRuntime* GameplayScene::try_collision_runtime() noexcept
{
    return _collision_runtime.get();
}

collision::GameplayCollisionRuntime& GameplayScene::collision_runtime()
{
    if (!_collision_runtime)
        throw std::logic_error("GameplayScene does not have a collision runtime.");
    return *_collision_runtime;
}

bool GameplayScene::contains_control_target(const elysia::core::GameObject *target) const
{
    return contains_object_address(target);
}
void GameplayScene::on_routed_input(const elysia::input::InputSnapshot &input)
{
    ControllerManager::instance()->input(_control_context, input);
}
void GameplayScene::on_fixed_update(std::uint64_t tick, double delta)
{
    ControllerManager::instance()->advance(_control_context, tick, delta);
    on_game_fixed_update(tick, delta);
}
void GameplayScene::on_pause_changed(bool paused)
{
    if (paused)
        ControllerManager::instance()->pause(_control_context);
}
void GameplayScene::on_scene_object_removing(elysia::core::SceneObject &object)
{
    std::exception_ptr failure;
    try
    {
        if (auto *target = dynamic_cast<elysia::core::GameObject *>(&object))
            ControllerManager::instance()->object_removed(_control_context, *target);
    }
    catch (...)
    {
        failure = std::current_exception();
    }
    try
    {
        if (_collision_runtime)
        {
            for (const auto collider : registered_physics_colliders(object))
                (void)_collision_runtime->unbind_collider(collider);
        }
    }
    catch (...)
    {
        if (!failure)
            failure = std::current_exception();
        else
            elysia::scene::detail::log_cleanup_exception("Gameplay collision unbind");
    }
    try
    {
        on_control_target_removing(object);
    }
    catch (...)
    {
        if (!failure)
            failure = std::current_exception();
        else
            elysia::scene::detail::log_cleanup_exception("Control target removal");
    }
    if (failure)
        std::rethrow_exception(failure);
}

void GameplayScene::on_runtime_attach()
{
    if (_collision_runtime &&
        !collision::GameplayCollisionService::instance()->attach_runtime(*_collision_runtime))
    {
        throw std::logic_error("Gameplay collision runtime activation failed.");
    }
    try
    {
        _control_context.activate();
    }
    catch (...)
    {
        if (_collision_runtime)
            (void)collision::GameplayCollisionService::instance()->detach_runtime(*_collision_runtime);
        throw;
    }
}

void GameplayScene::on_runtime_detach()
{
    std::exception_ptr failure;
    try
    {
        _control_context.deactivate();
    }
    catch (...)
    {
        failure = std::current_exception();
    }
    try
    {
        if (_collision_runtime)
            (void)collision::GameplayCollisionService::instance()->detach_runtime(*_collision_runtime);
    }
    catch (...)
    {
        if (!failure)
            failure = std::current_exception();
        else
            elysia::scene::detail::log_cleanup_exception("Gameplay collision runtime detach");
    }
    if (failure)
        std::rethrow_exception(failure);
}

void GameplayScene::on_runtime_reset()
{
    _control_context.reset();
}
} // namespace elysia::gameplay
