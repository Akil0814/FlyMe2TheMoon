#pragma once

#include "camera.h"
#include "camera_effect.h"
#include "camera_motion.h"
#include "follow_strategy.h"

#include <memory>
#include <optional>
#include <vector>

namespace elysia::camera
{
class CameraController
{
public:
    explicit CameraController(Camera& camera) noexcept;

    void set_follow_strategy(std::unique_ptr<IFollowStrategy> follow_strategy) noexcept;
    void set_focus(std::optional<CameraFocus> focus) noexcept;
    void set_world_bounds(std::optional<elysia::core::Rect> world_bounds) noexcept;
    void set_viewport_size(const elysia::core::Vector2& viewport_size) noexcept;
    void set_center(const elysia::core::Vector2& center) noexcept;
    void set_zoom(float zoom) noexcept;

    void snap_to_focus() noexcept;
    void start_shake(const CameraShakeParams& params);
    void start_motion(CameraMotionId id, const CameraMotionSpec& motion);
    [[nodiscard]] std::optional<CameraMotionState> motion_state(CameraMotionId id) const noexcept;
    bool pause_motion(CameraMotionId id) noexcept;
    bool resume_motion(CameraMotionId id) noexcept;
    bool cancel_motion(CameraMotionId id) noexcept;
    void cancel_motion() noexcept;
    void clear_effects() noexcept;
    void reset_scene_state() noexcept;
    std::optional<CameraMotionId> update(double delta_seconds);

private:
    [[nodiscard]] elysia::core::Vector2 clamp_center_to_world_bounds(
        const elysia::core::Vector2& center
    ) const noexcept;
    [[nodiscard]] bool update_motion(double delta_seconds, std::optional<CameraMotionId>& completion) noexcept;
    void write_final_camera_center(const elysia::core::Vector2& center) noexcept;

private:
    struct ActiveMotion
    {
        CameraMotionId id;
        std::vector<CameraPathNode> nodes;
        CameraMotionEndBehavior end_behavior = CameraMotionEndBehavior::ResumeFollow;
        std::size_t node_index = 0;
        double elapsed_seconds = 0.0;
        elysia::core::Vector2 segment_start_center{};
        float segment_start_zoom = Camera::kDefaultZoom;
        bool paused = false;
        bool holding = false;
    };

    Camera& _camera;
    elysia::core::Vector2 _logical_center{};
    std::unique_ptr<IFollowStrategy> _follow_strategy;
    std::optional<CameraFocus> _focus;
    std::optional<elysia::core::Rect> _world_bounds;
    std::unique_ptr<CameraEffect> _active_effect;
    std::optional<ActiveMotion> _motion;
    bool _has_initialized_focus = false;
};
}
