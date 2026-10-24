#pragma once
#include "control_command.h"
#include "scene_control_context.h"
#include <memory>
#include <optional>
namespace elysia::gameplay
{
enum class ControllerScope
{
    Scene,
    Session
};
enum class ControllerError
{
    NotInitialized,
    NoSession,
    SessionAlreadyActive,
    SessionEnding,
    InvalidHandle,
    InvalidContext,
    InvalidTarget,
    TargetBusy,
    PlayerBusy,
    InvalidPlayer,
    InvalidMap,
    Superseded,
    CallbackFailed
};
struct ControllerCreateInfo
{
    ControllerScope scope = ControllerScope::Scene;
    SceneControlToken scene;
};
struct ControllerDescription
{
    ControllerHandle handle;
    ControllerScope scope;
    SceneControlToken owner, bound_scene;
    std::uint64_t binding_generation;
    bool bound;
};
enum class ControllerOperationStatus
{
    Pending,
    Succeeded,
    Failed
};
// Main-thread result view. Retaining it never retains a controller or scene.
class ControllerOperation
{
  public:
    ControllerOperationStatus status() const noexcept
    {
        return _state->status;
    }
    std::optional<ControllerError> error() const noexcept
    {
        return _state->error;
    }
    bool succeeded() const noexcept
    {
        return status() == ControllerOperationStatus::Succeeded;
    }
    bool failed() const noexcept
    {
        return status() == ControllerOperationStatus::Failed;
    }
    bool pending() const noexcept
    {
        return status() == ControllerOperationStatus::Pending;
    }

  private:
    friend class ControllerManager;
    struct State
    {
        ControllerOperationStatus status = ControllerOperationStatus::Pending;
        std::optional<ControllerError> error;
    };
    ControllerOperation() = default;
    void finish(std::optional<ControllerError> error = {}) const
    {
        if (!pending())
            return;
        _state->error = error;
        _state->status = error ? ControllerOperationStatus::Failed : ControllerOperationStatus::Succeeded;
    }
    std::shared_ptr<State> _state = std::make_shared<State>();
};
} // namespace elysia::gameplay
