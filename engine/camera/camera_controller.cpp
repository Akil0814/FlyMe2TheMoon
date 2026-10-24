#include "camera_controller.h"

#include <algorithm>
#include <cmath>

namespace elysia::camera
{
CameraController::CameraController(Camera& camera) noexcept
    : _camera(camera),
      _logical_center(camera.center())
{
}

void CameraController::set_follow_strategy(
    std::unique_ptr<IFollowStrategy> follow_strategy
) noexcept
{
    _follow_strategy = std::move(follow_strategy);
    if (_follow_strategy) _follow_strategy->reset();
}

void CameraController::set_focus(std::optional<CameraFocus> focus) noexcept
{
    if (focus && (!valid_focus_rect(focus->bounds) || !valid_focus_rect(focus->primary)))
        focus.reset();
    if (!focus)
    {
        _has_initialized_focus = false;
        if (_follow_strategy) _follow_strategy->reset();
    }
    _focus = focus;
}

void CameraController::set_world_bounds(
    std::optional<elysia::core::Rect> world_bounds
) noexcept
{
    _world_bounds = world_bounds;
    _logical_center = clamp_center_to_world_bounds(_logical_center);
    write_final_camera_center(_logical_center);
}

void CameraController::set_viewport_size(
    const elysia::core::Vector2& viewport_size
) noexcept
{
    _camera.set_viewport_size(viewport_size);
    _logical_center = clamp_center_to_world_bounds(_logical_center);
    write_final_camera_center(_logical_center);
}

void CameraController::set_center(const elysia::core::Vector2& center) noexcept
{
    cancel_motion();
    _logical_center = clamp_center_to_world_bounds(center);
    write_final_camera_center(_logical_center);
}

void CameraController::set_zoom(float zoom) noexcept
{
    cancel_motion();
    _camera.set_zoom(zoom);
    _logical_center = clamp_center_to_world_bounds(_logical_center);
    write_final_camera_center(_logical_center);
}

void CameraController::snap_to_focus() noexcept
{
    cancel_motion();
    if (!_focus.has_value())
    {
        return;
    }

    _logical_center = clamp_center_to_world_bounds(_focus->bounds.center());
    write_final_camera_center(_logical_center);
}

void CameraController::start_shake(const CameraShakeParams& params)
{
    _active_effect = std::make_unique<CameraShakeEffect>(params);
}

void CameraController::start_motion(CameraMotionId id, const CameraMotionSpec& motion)
{
    validate_camera_motion(motion);
    _motion = ActiveMotion{
        .id = id,
        .nodes = motion.nodes,
        .end_behavior = motion.end_behavior,
        .segment_start_center = _logical_center,
        .segment_start_zoom = _camera.zoom()
    };
}

std::optional<CameraMotionState> CameraController::motion_state(CameraMotionId id) const noexcept
{
    if (!_motion || _motion->id != id)
        return std::nullopt;
    if (_motion->paused)
        return CameraMotionState::Paused;
    if (_motion->holding)
        return CameraMotionState::Holding;
    return CameraMotionState::Playing;
}

bool CameraController::pause_motion(CameraMotionId id) noexcept
{
    if (!_motion || _motion->id != id)
        return false;
    _motion->paused = true;
    return true;
}

bool CameraController::resume_motion(CameraMotionId id) noexcept
{
    if (!_motion || _motion->id != id)
        return false;
    _motion->paused = false;
    return true;
}

bool CameraController::cancel_motion(CameraMotionId id) noexcept
{
    if (!_motion || _motion->id != id)
        return false;
    _motion.reset();
    return true;
}

void CameraController::cancel_motion() noexcept
{
    _motion.reset();
}

void CameraController::clear_effects() noexcept
{
    _active_effect.reset();
    write_final_camera_center(_logical_center);
}

void CameraController::reset_scene_state() noexcept
{
    _follow_strategy.reset();
    _focus.reset();
    _world_bounds.reset();
    _active_effect.reset();
    _motion.reset();
    _has_initialized_focus = false;
    _camera.set_zoom(Camera::kDefaultZoom);
    _logical_center = elysia::core::Vector2::zero();
    write_final_camera_center(_logical_center);
}

std::optional<CameraMotionId> CameraController::update(double delta_seconds)
{
    // A paused pose also freezes its shake, including a terminal Hold pose.
    if (_motion && _motion->paused)
        return std::nullopt;
    std::optional<CameraMotionId> completion;
    const bool motion_controls_pose = update_motion(delta_seconds, completion);

    if (!motion_controls_pose && _focus)
    {
        const bool snap = !_has_initialized_focus
            && (!_follow_strategy || _follow_strategy->snap_on_acquisition());
        if (snap) snap_to_focus();
        else if (_follow_strategy)
        {
            const CameraFollowContext context{
                _logical_center, _camera.viewport_size(), _camera.zoom(), true};
            const auto result = _follow_strategy->update(context, *_focus, delta_seconds);
            if (result.zoom) _camera.set_zoom(*result.zoom);
            _logical_center = result.center;
        }
        _has_initialized_focus = true;
    }

    _logical_center = clamp_center_to_world_bounds(_logical_center);

    elysia::core::Vector2 resolved_center = _logical_center;
    if (_active_effect)
    {
        resolved_center += _active_effect->update(delta_seconds);

        if (_active_effect->is_finished())
        {
            _active_effect.reset();
        }
    }

    write_final_camera_center(resolved_center);
    return completion;
}

elysia::core::Vector2 CameraController::clamp_center_to_world_bounds(
    const elysia::core::Vector2& center
) const noexcept
{
    if (!_world_bounds.has_value())
    {
        return center;
    }

    const elysia::core::Rect& bounds = *_world_bounds;
    const elysia::core::Vector2 viewport_size = _camera.world_viewport_size();
    const elysia::core::Vector2 viewport_half = viewport_size * 0.5f;

    elysia::core::Vector2 clamped_center = center;

    if (bounds.width() <= viewport_size.x)
    {
        clamped_center.x = bounds.center().x;
    }
    else
    {
        clamped_center.x = std::clamp(
            clamped_center.x,
            bounds.left() + viewport_half.x,
            bounds.right() - viewport_half.x
        );
    }

    if (bounds.height() <= viewport_size.y)
    {
        clamped_center.y = bounds.center().y;
    }
    else
    {
        clamped_center.y = std::clamp(
            clamped_center.y,
            bounds.top() + viewport_half.y,
            bounds.bottom() - viewport_half.y
        );
    }

    return clamped_center;
}

bool CameraController::update_motion(
    double delta_seconds,
    std::optional<CameraMotionId>& completion
) noexcept
{
    if (!_motion)
        return false;

    ActiveMotion& motion = *_motion;
    if (motion.holding || motion.paused)
        return true;

    motion.elapsed_seconds += std::max(0.0, delta_seconds);
    while (true)
    {
        const CameraPathNode& node = motion.nodes[motion.node_index];
        const elysia::core::Vector2 target_center =
            node.target.center.value_or(motion.segment_start_center);
        const float target_zoom = Camera::clamp_zoom(
            node.target.zoom.value_or(motion.segment_start_zoom));
        const double progress = std::clamp(
            motion.elapsed_seconds / node.duration_seconds, 0.0, 1.0);
        const float eased = static_cast<float>(apply_camera_easing(node.easing, progress));
        _logical_center = motion.segment_start_center
            + (target_center - motion.segment_start_center) * eased;
        _camera.set_zoom(motion.segment_start_zoom
            + (target_zoom - motion.segment_start_zoom) * eased);

        if (progress < 1.0)
            return true;

        motion.elapsed_seconds -= node.duration_seconds;
        motion.segment_start_center = target_center;
        motion.segment_start_zoom = target_zoom;
        ++motion.node_index;
        if (motion.node_index < motion.nodes.size())
            continue;

        completion = motion.id;
        if (motion.end_behavior == CameraMotionEndBehavior::Hold)
        {
            motion.holding = true;
        }
        else
        {
            _motion.reset();
        }
        return true;
    }
}

void CameraController::write_final_camera_center(
    const elysia::core::Vector2& center
) noexcept
{
    _camera.set_center(center);
}
}
