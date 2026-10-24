#pragma once
#include "../../ui/input/ui_input_router.h"
#include "../../input/input_suppression.h"
#include <functional>
#include <map>
namespace elysia::scene
{
class Scene;
}
namespace elysia::gameplay
{
class GameplayScene;
class ControllerManager;
}
namespace elysia::scene
{
enum class UiInteractionMode
{
    Pointer,
    Navigation
};
struct UiDeviceAccess
{
    elysia::input::InputSourceId gamepad;
};
class SceneInputRouter
{
  public:
    explicit SceneInputRouter(elysia::scene::Scene &scene) : _scene(scene)
    {
    }
    void route(const elysia::input::InputSnapshot &);
    void reset();
    void suppress(elysia::input::LocalPlayerId);
    void consume_input(const elysia::input::RawInputEvent &);
    const std::vector<elysia::input::RawInputEvent> &consumed_operations() const
    {
        return _consumed;
    }
    void set_ui_gamepad(elysia::input::InputSourceId);
    elysia::input::InputSourceId ui_gamepad() const
    {
        return _ui_access->gamepad;
    }
    void set_ui_interaction_mode(UiInteractionMode);
    UiInteractionMode ui_interaction_mode() const
    {
        return _mode;
    }
    void set_all_gameplay_input_blocked(bool);
    void set_auto_claim_ui_gamepad(bool enabled)
    {
        _auto_claim_ui_gamepad = enabled;
    }
    void set_shortcut_devices(elysia::input::InputCapture devices)
    {
        _shortcut_devices = devices;
    }
  private:
    friend class elysia::scene::Scene;
    friend class elysia::gameplay::GameplayScene;
    friend class elysia::gameplay::ControllerManager;
    void set_ui_access(UiDeviceAccess &access) noexcept
    {
        _ui_access = &access;
    }
    void set_cancel_handler(std::function<void(elysia::input::LocalPlayerId, elysia::input::InputCancelReason)> handler)
    {
        _cancel = std::move(handler);
    }
    void cancel(elysia::input::LocalPlayerId player,
                elysia::input::InputCancelReason reason = elysia::input::InputCancelReason::Suppressed);
    void flush_cancellations();
    elysia::input::InputCapture gameplay_capture(elysia::input::InputSourceId source,
        elysia::input::InputCapture external, bool focus_lost,
        elysia::input::InputCapture additional = elysia::input::InputCapture::None) const;
    elysia::input::InputCapture ui_capture() const;
    bool ui_source(elysia::input::InputSourceId source) const;
    bool ui_enabled(elysia::input::InputSourceId source) const;
    void cancel_source(elysia::input::InputSourceId,
        elysia::input::InputCancelReason = elysia::input::InputCancelReason::Suppressed);
    void reset_ui_interaction();
    void cancel_ui_interactions();
    void dispatch_ui_frame(const elysia::ui::UiInputFrame &input);
    elysia::scene::Scene &_scene;
    bool _auto_claim_ui_gamepad = true;
    UiDeviceAccess _standalone_ui_access;
    UiDeviceAccess *_ui_access = &_standalone_ui_access;
    UiInteractionMode _mode = UiInteractionMode::Navigation;
    elysia::input::InputCapture _shortcut_devices =
        elysia::input::InputCapture::Keyboard | elysia::input::InputCapture::Gamepad;
    std::function<void(elysia::input::LocalPlayerId, elysia::input::InputCancelReason)> _cancel;
    elysia::input::InputDevice _ui_active_device = elysia::input::InputDevice::Keyboard;
    elysia::ui::UiInputState _last_ui_state;
    bool _await_initial_input = true;
    std::vector<elysia::input::InputSourceId> _previous_ui_sources;
    std::map<elysia::input::InputSourceId, elysia::input::InputCapture> _previous_capture;
    std::map<elysia::input::InputSourceId, elysia::input::InputSuppression> _suppression, _ui_suppression;
    elysia::input::InputSnapshot _last_input;
    std::vector<elysia::input::RawInputEvent> _consumed;
    bool _block_gameplay = false;
    bool _routing = false;
    std::map<elysia::input::LocalPlayerId, elysia::input::InputCancelReason> _pending_cancellations;
    elysia::ui::UiInputRouter _ui_input_router;
};
} // namespace elysia::scene
