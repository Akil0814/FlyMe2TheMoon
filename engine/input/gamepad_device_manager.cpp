#include "gamepad_device_manager.h"

#include "../tools/logger.h"

#include <algorithm>

namespace elysia::input
{
GamepadDeviceManager::~GamepadDeviceManager()
{
    shutdown();
}

void GamepadDeviceManager::initialize()
{
    if (_initialized)
    {
        return;
    }

    _initialized = true;
    open_connected_controllers();
}

void GamepadDeviceManager::shutdown()
{
    if (!_initialized)
    {
        return;
    }

    close_all_controllers();
    _initialized = false;
}

void GamepadDeviceManager::handle_event(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_EVENT_GAMEPAD_ADDED:
        open_controller(event.gdevice.which);
        break;

    case SDL_EVENT_GAMEPAD_REMOVED:
        close_controller(event.gdevice.which);
        break;

    default:
        break;
    }
}

void GamepadDeviceManager::open_connected_controllers()
{
    int count=0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    for (int i=0;i<count;++i) open_controller(ids[i]);
    SDL_free(ids);
}

void GamepadDeviceManager::open_controller(SDL_JoystickID joystick_id)
{
    if (!SDL_IsGamepad(joystick_id))
    {
        return;
    }

    SDL_Gamepad* controller = SDL_OpenGamepad(joystick_id);
    if (!controller)
    {
        elysia::tools::Logger::instance()->warn("input","Failed to open controller");
        elysia::tools::Logger::instance()->warn("input",SDL_GetError());
        SDL_ClearError();
        return;
    }

    SDL_Joystick* joystick = SDL_GetGamepadJoystick(controller);


    for (SDL_Gamepad* existing_controller : _controllers)
    {
        if (!existing_controller)
        {
            continue;
        }

        SDL_Joystick* existing_joystick = SDL_GetGamepadJoystick(existing_controller);
        if (SDL_GetJoystickID(existing_joystick) == joystick_id)
        {
            SDL_CloseGamepad(controller);
            return;
        }
    }

    _controllers.push_back(controller);
}

void GamepadDeviceManager::close_controller(SDL_JoystickID joystick_id)
{
    std::vector<SDL_Gamepad*>::iterator iter = std::remove_if(
        _controllers.begin(),
        _controllers.end(),
        [joystick_id](SDL_Gamepad* controller)
        {
            if (!controller)
            {
                return true;
            }

            SDL_Joystick* joystick = SDL_GetGamepadJoystick(controller);
            if (SDL_GetJoystickID(joystick) != joystick_id)
            {
                return false;
            }

            SDL_CloseGamepad(controller);
            return true;
        }
    );

    _controllers.erase(iter, _controllers.end());
}

void GamepadDeviceManager::close_all_controllers()
{
    for (SDL_Gamepad* controller : _controllers)
    {
        if (controller)
        {
            SDL_CloseGamepad(controller);
        }
    }

    _controllers.clear();
}

}
