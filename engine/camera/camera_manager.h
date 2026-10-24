#pragma once

#include "camera.h"
#include "camera_controller.h"
#include "camera_motion.h"
#include "camera_types.h"
#include "../tools/singleton.h"

#include <array>
#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <variant>

namespace elysia::camera
{
class CameraManager final : public elysia::tools::Singleton<CameraManager>
{
    friend class elysia::tools::Singleton<CameraManager>;

public:
    [[nodiscard]] const Camera& camera(CameraSlot slot) const noexcept;

    void set_center(CameraSlot slot, const elysia::core::Vector2& center) noexcept;
    void set_viewport_size(CameraSlot slot, const elysia::core::Vector2& viewport_size) noexcept;
    void set_zoom(CameraSlot slot, float zoom) noexcept;
    void set_focus(CameraSlot slot, std::optional<CameraFocus> focus) noexcept;
    void set_world_bounds(CameraSlot slot, std::optional<elysia::core::Rect> world_bounds) noexcept;
    void set_follow_strategy(CameraSlot slot, std::unique_ptr<IFollowStrategy> follow_strategy) noexcept;

    void request_shake(CameraSlot slot, const CameraShakeParams& params);
    void request_snap_to_focus(CameraSlot slot);
    void request_clear_effects(CameraSlot slot);

    [[nodiscard]] CameraMotionId move_camera_to(
        CameraSlot slot, const CameraPoseTarget& target, double duration_seconds,
        CameraEasing easing = CameraEasing::SmoothStep,
        CameraMotionEndBehavior end_behavior = CameraMotionEndBehavior::ResumeFollow);
    [[nodiscard]] CameraMotionId play_camera_path(CameraSlot slot, const CameraMotionSpec& motion);
    [[nodiscard]] std::optional<CameraMotionState> camera_motion_state(CameraMotionId id) const noexcept;
    bool pause_camera_motion(CameraMotionId id) noexcept;
    bool resume_camera_motion(CameraMotionId id) noexcept;
    bool cancel_camera_motion(CameraMotionId id) noexcept;
    void cancel_camera_motions(CameraSlotSet slots) noexcept;

    CameraUpdateResult update(CameraSlotSet slots, double delta_seconds);
    void reset(CameraSlot slot) noexcept;
    void reset(CameraSlotSet slots) noexcept;
    void reset_all() noexcept;

private:
    struct CameraRig
    {
        Camera camera;
        CameraController controller;

        CameraRig() noexcept;
        CameraRig(const CameraRig&) = delete;
        CameraRig& operator=(const CameraRig&) = delete;
        CameraRig(CameraRig&&) = delete;
        CameraRig& operator=(CameraRig&&) = delete;
    };

    struct ShakeRequest
    {
        CameraShakeParams params;
    };

    struct SnapToFocusRequest {};
    struct ClearEffectsRequest {};
    using RequestPayload = std::variant<
        ShakeRequest,
        SnapToFocusRequest,ClearEffectsRequest
    >;

    struct CameraRequest
    {
        CameraSlot slot = CameraSlot::Main;
        RequestPayload payload;
    };

    CameraManager() = default;

    [[nodiscard]] static std::size_t slot_index(CameraSlot slot) noexcept;
    [[nodiscard]] CameraRig& rig(CameraSlot slot) noexcept;
    [[nodiscard]] const CameraRig& rig(CameraSlot slot) const noexcept;
    void process_requests();

    std::array<CameraRig, static_cast<std::size_t>(CameraSlot::Count)> _rigs;
    std::deque<CameraRequest> _requests;
    std::uint64_t _next_motion_id = 1;
};
}
