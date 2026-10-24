#pragma once
#include "control_command.h"
#include "../../input/action/input_action_map.h"
#include <functional>
namespace elysia::gameplay
{
class ControllerManager;
class Controller
{
  public:
    virtual ~Controller() = default;
    Controller(const Controller &) = delete;
    Controller &operator=(const Controller &) = delete;
    ControllerHandle handle() const
    {
        return _handle;
    }

  protected:
    Controller() = default;
    virtual void produce_intent(std::uint64_t tick, double fixed_delta)
    {
    }
    virtual void cancelled(InputCancelReason)
    {
    }
    void submit(elysia::input::ActionInputResult input)
    {
        if (_submit)
            _submit(std::move(input));
    }

  private:
    friend class ControllerManager;
    ControllerHandle _handle;
    std::function<void(elysia::input::ActionInputResult)> _submit;
};
class LocalPlayerController : public Controller
{
  public:
    LocalPlayerController(elysia::input::LocalPlayerId player, elysia::input::InputActionMap map)
        : _player(player), _map(std::move(map))
    {
    }
    elysia::input::LocalPlayerId player() const
    {
        return _player;
    }
    const elysia::input::InputActionMap &input_map() const
    {
        return _map;
    }

  protected:
    virtual void on_mapped_input(const elysia::input::ActionInputResult &)
    {
    }

  private:
    friend class ControllerManager;
    void process_input(const elysia::input::InputSnapshot &input)
    {
        auto actions = _map.resolve(input);
        on_mapped_input(actions);
        submit(std::move(actions));
    }
    elysia::input::LocalPlayerId _player;
    elysia::input::InputActionMap _map;
    std::vector<elysia::input::InputSourceId> _sources;
    std::uint64_t _binding_version = 0;
};
} // namespace elysia::gameplay
