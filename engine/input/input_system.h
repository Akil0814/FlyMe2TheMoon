#pragma once

#include "input_snapshot.h"
#include "input_suppression.h"
#include <map>
#include "input_capture.h"
#include "gamepad_device_manager.h"

#include "translator/gamepad_input_translator.h"
#include "translator/keyboard_mouse_input_translator.h"
#include "translator/input_translator.h"

#include <SDL3/SDL.h>
#include <optional>
#include <vector>

namespace elysia::input
{
class InputSystem
{
public:
    void initialize();
    void shutdown();
    [[nodiscard]] bool is_initialized() const noexcept { return _initialized; }
    void begin_frame();
    void process_event(const SDL_Event& event);
    void set_development_input_capture(
        InputCapture capture) noexcept;
    [[nodiscard]] InputCapture development_input_capture() const noexcept
    {
        return _development_input_capture;
    }
    InputSnapshot snapshot() const;
    void set_renderer(SDL_Renderer* renderer);

private:
    void translate_event(const SDL_Event& event, InputDevice event_device);
    InputTranslator* select_translator(InputDevice device);
    RawInputEvent normalize_mouse_event(const RawInputEvent& event) const;
    void update_mouse_frame_cache(const RawInputEvent& event);
    void refresh_mouse_position();
    void convert_window_to_logical(float window_x, float window_y, int& logical_x, int& logical_y) const;
    void apply_event(const RawInputEvent& event);
    void append_event(const RawInputEvent& event);

    void handle_gamepad_removed(const SDL_Event& event);

    void reset_input_lifecycle();
    bool should_clear_state_for_event(const SDL_Event& event) const;
    bool is_window_size_changed_event(const SDL_Event& event) const;

private:
    std::vector<RawInputEvent> _events;
    GamepadDeviceManager _gamepad_devices;
    SDL_Renderer* _renderer = nullptr;
    KeyboardMouseInputTranslator _keyboard_mouse_translator;
    struct SourceState
    {
        RawInputState physical, initial;
        GamepadInputTranslator translator;
        InputDevice device = InputDevice::Unknown;
    };
    std::map<InputSourceId, SourceState> _sources;
    InputSourceId _translating_source = InputSourceId::keyboard();
    std::vector<InputSourceId> _connected, _removed;
    bool _focus_lost = false;
    int _mouse_x = 0;
    int _mouse_y = 0;
    int _mouse_delta_x = 0;
    int _mouse_delta_y = 0;
    bool _has_mouse_position = false;

    bool _initialized = false;
    InputCapture _development_input_capture =
        InputCapture::None;
};

}
