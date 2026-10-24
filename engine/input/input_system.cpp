#include "input_system.h"

#include <cmath>

namespace elysia::input
{
namespace
{
InputDevice detect_event_device(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_EDITING:
    case SDL_EVENT_TEXT_INPUT:
        return InputDevice::Keyboard;

    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
        return InputDevice::Mouse;

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        return InputDevice::Gamepad;

    default:
        return InputDevice::Unknown;
    }
}


[[nodiscard]] bool has_mouse_position(const RawInputEvent& event) noexcept
{
    return event.device == InputDevice::Mouse
        && (event.type == RawInputEventType::MouseMoved
            || event.type == RawInputEventType::MouseWheel
            || event.mouse_button != 0);
}
}

void InputSystem::initialize()
{
    reset_input_lifecycle();
    _gamepad_devices.initialize();
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    for (int i = 0; i < count; ++i)
    {
        auto id = InputSourceId::gamepad(ids[i]);
        _sources.try_emplace(id);
        _connected.push_back(id);
    }
    SDL_free(ids);
    _initialized = true;
}

void InputSystem::shutdown()
{
    _gamepad_devices.shutdown();
    reset_input_lifecycle();
    _initialized = false;
}

void InputSystem::set_development_input_capture(
    InputCapture capture) noexcept
{
    _development_input_capture = capture;
}

void InputSystem::begin_frame()
{
    for (auto &[id, source] : _sources)
    {
        source.physical.begin_frame();
        source.initial = source.physical;
    }
    _connected.clear();
    _removed.clear();
    _focus_lost = false;
    _events.clear();
    _mouse_delta_x = 0;
    _mouse_delta_y = 0;
}

void InputSystem::process_event(const SDL_Event& event)
{
    _gamepad_devices.handle_event(event);

    if (event.type == SDL_EVENT_GAMEPAD_REMOVED)
    {
        handle_gamepad_removed(event);
        return;
    }

    if (event.type == SDL_EVENT_GAMEPAD_ADDED)
    {
        auto id = InputSourceId::gamepad(event.gdevice.which);
        if (_sources.try_emplace(id).second)
            _connected.push_back(id);
        return;
    }
    if (should_clear_state_for_event(event))
    {
        _focus_lost = true;
        _events.clear();
        // Keep physical state until release so scene suppression can require neutral.
        return;
    }
    if (is_window_size_changed_event(event))
    {
        refresh_mouse_position();
        return;
    }
    const auto event_device = detect_event_device(event);
    if (event_device == InputDevice::Unknown)
        return;
    _translating_source =
        event_device == InputDevice::Gamepad
            ? InputSourceId::gamepad(event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION ? event.gaxis.which
                                                                                 : event.gbutton.which)
            : (event_device == InputDevice::Mouse ? InputSourceId::mouse() : InputSourceId::keyboard());
    if (_sources.try_emplace(_translating_source).second && _translating_source.is_gamepad())
        _connected.push_back(_translating_source);
    _sources[_translating_source].device = event_device;
    translate_event(event, event_device);
}

void InputSystem::set_renderer(SDL_Renderer* renderer)
{
    _renderer = renderer;
}

void InputSystem::translate_event(const SDL_Event& event, InputDevice event_device)
{
    InputTranslator* translator = select_translator(event_device);
    if (!translator)
    {
        return;
    }

    SDL_Event logical_event = event;
    if (_renderer && (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP))
        SDL_ConvertEventToRenderCoordinates(_renderer,&logical_event);
    std::vector<RawInputEvent> input_events = translator->translate_event(logical_event);

    for (const RawInputEvent& input_event : input_events)
    {
        auto sourced = normalize_mouse_event(input_event);
        sourced.source = _translating_source;
        append_event(sourced);
    }
}

InputTranslator* InputSystem::select_translator(InputDevice device)
{
    if (device == InputDevice::Gamepad)
    {
        return &_sources[_translating_source].translator;
    }

    if (device == InputDevice::Keyboard || device == InputDevice::Mouse)
    {
        return &_keyboard_mouse_translator;
    }

    return nullptr;
}

RawInputEvent InputSystem::normalize_mouse_event(const RawInputEvent& event) const
{
    RawInputEvent converted_event = event;
    if (!has_mouse_position(event))
    {
        return converted_event;
    }

    // Motion/buttons were converted before translation; queried wheel positions are window coordinates.
    if (event.type == RawInputEventType::MouseWheel)
    {
        if (_has_mouse_position)
        {
            converted_event.mouse_x = _mouse_x;
            converted_event.mouse_y = _mouse_y;
        }
        else
        {
            float window_x=0.0f,window_y=0.0f;
            SDL_GetMouseState(&window_x,&window_y);
            convert_window_to_logical(
                window_x,
                window_y,
                converted_event.mouse_x,
                converted_event.mouse_y
            );
        }
    }

    if (event.type == RawInputEventType::MouseMoved)
    {
        if (_has_mouse_position)
        {
            converted_event.mouse_delta_x = converted_event.mouse_x - _mouse_x;
            converted_event.mouse_delta_y = converted_event.mouse_y - _mouse_y;
        }
        else
        {
            converted_event.mouse_delta_x = 0;
            converted_event.mouse_delta_y = 0;
        }
    }
    else
    {
        converted_event.mouse_delta_x = 0;
        converted_event.mouse_delta_y = 0;
    }

    return converted_event;
}

void InputSystem::update_mouse_frame_cache(const RawInputEvent& event)
{
    if (!has_mouse_position(event))
    {
        return;
    }

    _mouse_x = event.mouse_x;
    _mouse_y = event.mouse_y;

    if (event.type == RawInputEventType::MouseMoved)
    {
        _mouse_delta_x += event.mouse_delta_x;
        _mouse_delta_y += event.mouse_delta_y;
    }

    _has_mouse_position = true;
}

void InputSystem::refresh_mouse_position()
{
    RawInputEvent mouse_event;
    mouse_event.type = RawInputEventType::MouseMoved;
    mouse_event.device = InputDevice::Mouse;
    mouse_event.source = InputSourceId::mouse();
    float window_x=0,window_y=0;
    SDL_GetMouseState(&window_x,&window_y);

    RawInputEvent converted_event = mouse_event;
    convert_window_to_logical(
        window_x,
        window_y,
        converted_event.mouse_x,
        converted_event.mouse_y
    );
    converted_event.mouse_delta_x = 0;
    converted_event.mouse_delta_y = 0;
    append_event(converted_event);
}

void InputSystem::convert_window_to_logical(float window_x, float window_y, int& logical_x, int& logical_y) const
{
    if (!_renderer)
    {
        logical_x = static_cast<int>(std::lround(window_x));
        logical_y = static_cast<int>(std::lround(window_y));
        return;
    }

    float converted_x = static_cast<float>(window_x);
    float converted_y = static_cast<float>(window_y);
    SDL_RenderCoordinatesFromWindow(_renderer, window_x, window_y, &converted_x, &converted_y);

    logical_x = static_cast<int>(std::lround(converted_x));
    logical_y = static_cast<int>(std::lround(converted_y));
}

void InputSystem::apply_event(const RawInputEvent& event)
{
    apply_raw_event(_sources[event.source].physical, event);
}
void InputSystem::append_event(const RawInputEvent& event)
{
    apply_event(event);
    update_mouse_frame_cache(event);
    _events.push_back(event);
}
void InputSystem::handle_gamepad_removed(const SDL_Event& event)
{
    auto id = InputSourceId::gamepad(event.gdevice.which);
    _sources.erase(id);
    _removed.push_back(id);
    std::erase_if(_events, [&](const auto &e) { return e.source == id; });
}
InputSnapshot InputSystem::snapshot() const
{
    InputSnapshot result;
    result.events = _events;
    result.connected = _connected;
    result.removed = _removed;
    result.focus_lost = _focus_lost;
    result.capture = _development_input_capture;
    for (const auto &[id, source] : _sources)
    {
        RawInputFrame frame;
        frame.state = source.physical;
        frame.active_device = source.device;
        if (id.is_mouse())
        {
            frame.mouse_x = _mouse_x;
            frame.mouse_y = _mouse_y;
            frame.mouse_delta_x = _mouse_delta_x;
            frame.mouse_delta_y = _mouse_delta_y;
        }
        result.sources.push_back({id, source.initial, frame});
    }
    return result;
}

void InputSystem::reset_input_lifecycle()
{
    _events.clear();
    _sources.clear();
    _sources.emplace(InputSourceId::keyboard(), SourceState{});
    _sources.emplace(InputSourceId::mouse(), SourceState{});
    _connected.clear();
    _removed.clear();
    _focus_lost = false;
    _mouse_x = 0;
    _mouse_y = 0;
    _mouse_delta_x = 0;
    _mouse_delta_y = 0;
    _has_mouse_position = false;
    _development_input_capture = InputCapture::None;
}

bool InputSystem::should_clear_state_for_event(const SDL_Event& event) const
{
    return event.type == SDL_EVENT_WINDOW_FOCUS_LOST;
}

bool InputSystem::is_window_size_changed_event(const SDL_Event& event) const
{
    return event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
}

}
