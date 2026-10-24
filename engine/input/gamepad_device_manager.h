#pragma once

#include <SDL3/SDL.h>

#include <vector>

namespace elysia::input
{
class GamepadDeviceManager
{
public:
    GamepadDeviceManager() = default;
    ~GamepadDeviceManager();

    GamepadDeviceManager(const GamepadDeviceManager&) = delete;
    GamepadDeviceManager& operator=(const GamepadDeviceManager&) = delete;

    GamepadDeviceManager(GamepadDeviceManager&&) = delete;
    GamepadDeviceManager& operator=(GamepadDeviceManager&&) = delete;

    void initialize();
    void shutdown();
    [[nodiscard]] bool is_initialized() const noexcept { return _initialized; }
    void handle_event(const SDL_Event& event);

private:
    void open_connected_controllers();
    void open_controller(SDL_JoystickID joystick_id);
    void close_controller(SDL_JoystickID joystick_id);
    void close_all_controllers();

private:
    std::vector<SDL_Gamepad*> _controllers;
    bool _initialized = false;
};

}
