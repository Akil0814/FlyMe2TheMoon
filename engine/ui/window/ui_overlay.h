#pragma once

#include "../../core/geometry/vector2.h"

namespace elysia::ui
{
class UiWindow;

// Optional client protocol for overlay elements that keep a borrowed pointer to
// their registering window. UiWindow calls this before its registration state
// is discarded; implementations must only clear local state.
class UiOverlayWindowClient
{
public:
    virtual ~UiOverlayWindowClient() = default;
    virtual void on_window_detached(UiWindow& window) noexcept = 0;
};

enum class UiOverlayPlacement
{
    Center,
    LeftDrawer,
    RightDrawer,
    TopSheet,
    BottomSheet
};

enum class UiOverlayTransition
{
    None,
    Slide
};

// Window-managed overlay behavior, placement, and dismissal policy.
struct UiOverlayOptions
{
    // Open state belongs to the window registration entry, not to element lifetime.
    bool open = true;
    bool modal = true;
    bool close_on_cancel = true;
    bool close_on_outside_click = true;
    UiOverlayPlacement placement = UiOverlayPlacement::Center;
    UiOverlayTransition transition = UiOverlayTransition::None;
    // Used only when the overlay element has no usable explicit size.
    elysia::core::Vector2 fallback_size{ 360.0f,220.0f };
    int order = 1000;
};
}
