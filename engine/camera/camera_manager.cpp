#include "camera_manager.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace elysia::camera
{
CameraManager::CameraRig::CameraRig() noexcept
    : controller(camera)
{
}

const Camera& CameraManager::camera(CameraSlot slot) const noexcept
{
    return rig(slot).camera;
}

void CameraManager::set_center(
    CameraSlot slot,
    const elysia::core::Vector2& center
) noexcept
{
    rig(slot).controller.set_center(center);
}

void CameraManager::set_viewport_size(
    CameraSlot slot,
    const elysia::core::Vector2& viewport_size
) noexcept
{
    rig(slot).controller.set_viewport_size(viewport_size);
}

void CameraManager::set_zoom(CameraSlot slot, float zoom) noexcept
{
    rig(slot).controller.set_zoom(zoom);
}

void CameraManager::set_focus(CameraSlot slot, std::optional<CameraFocus> focus) noexcept
{
    rig(slot).controller.set_focus(focus);
}

void CameraManager::set_world_bounds(
    CameraSlot slot,
    std::optional<elysia::core::Rect> world_bounds
) noexcept
{
    rig(slot).controller.set_world_bounds(world_bounds);
}

void CameraManager::set_follow_strategy(
    CameraSlot slot,
    std::unique_ptr<IFollowStrategy> follow_strategy
) noexcept
{
    rig(slot).controller.set_follow_strategy(std::move(follow_strategy));
}

void CameraManager::request_shake(
    CameraSlot slot,
    const CameraShakeParams& params
)
{
    _requests.push_back(CameraRequest{ slot, ShakeRequest{ params } });
}

void CameraManager::request_snap_to_focus(CameraSlot slot)
{
    _requests.push_back(CameraRequest{ slot, SnapToFocusRequest{} });
}

void CameraManager::request_clear_effects(CameraSlot slot)
{
    _requests.push_back(CameraRequest{ slot, ClearEffectsRequest{} });
}

CameraMotionId CameraManager::move_camera_to(
    CameraSlot slot,
    const CameraPoseTarget& target,
    double duration_seconds,
    CameraEasing easing,
    CameraMotionEndBehavior end_behavior
)
{
    return play_camera_path(slot, CameraMotionSpec{
        .nodes = {CameraPathNode{
            .target = target,
            .duration_seconds = duration_seconds,
            .easing = easing
        }},
        .end_behavior = end_behavior
    });
}

CameraMotionId CameraManager::play_camera_path(CameraSlot slot, const CameraMotionSpec& motion)
{
    if (!valid_camera_slot(slot))
        throw std::invalid_argument("Camera motion target slot is invalid.");
    validate_camera_motion(motion);
    const CameraMotionId id{_next_motion_id++};
    rig(slot).controller.start_motion(id, motion);
    return id;
}

std::optional<CameraMotionState> CameraManager::camera_motion_state(CameraMotionId id) const noexcept
{
    if (id.value == 0)
        return std::nullopt;
    for (const CameraRig& camera_rig : _rigs)
        if (const auto state = camera_rig.controller.motion_state(id))
            return state;
    return std::nullopt;
}

bool CameraManager::pause_camera_motion(CameraMotionId id) noexcept
{
    for (CameraRig& camera_rig : _rigs)
        if (camera_rig.controller.pause_motion(id))
            return true;
    return false;
}

bool CameraManager::resume_camera_motion(CameraMotionId id) noexcept
{
    for (CameraRig& camera_rig : _rigs)
        if (camera_rig.controller.resume_motion(id))
            return true;
    return false;
}

bool CameraManager::cancel_camera_motion(CameraMotionId id) noexcept
{
    for (CameraRig& camera_rig : _rigs)
        if (camera_rig.controller.cancel_motion(id))
            return true;
    return false;
}

void CameraManager::cancel_camera_motions(CameraSlotSet slots) noexcept
{
    for (std::size_t index = 0; index < _rigs.size(); ++index)
        if (slots.contains(static_cast<CameraSlot>(index)))
            _rigs[index].controller.cancel_motion();
}

CameraUpdateResult CameraManager::update(
    CameraSlotSet slots,
    double delta_seconds
)
{
    CameraUpdateResult result;
    process_requests();

    for (std::size_t index = 0; index < _rigs.size(); ++index)
    {
        const CameraSlot slot = static_cast<CameraSlot>(index);
        if (!slots.contains(slot))
            continue;
        if (const auto completed = _rigs[index].controller.update(delta_seconds))
            result.push(CameraMotionCompletion{*completed, slot});
    }
    return result;
}

void CameraManager::reset(CameraSlot slot) noexcept
{
    std::erase_if(_requests, [slot](const CameraRequest& request)
    {
        return request.slot == slot;
    });

    rig(slot).controller.reset_scene_state();
}

void CameraManager::reset(CameraSlotSet slots) noexcept
{
    for (std::size_t index = 0; index < _rigs.size(); ++index)
        if (slots.contains(static_cast<CameraSlot>(index)))
            reset(static_cast<CameraSlot>(index));
}

void CameraManager::reset_all() noexcept
{
    _requests.clear();

    for (CameraRig& camera_rig : _rigs)
    {
        camera_rig.controller.reset_scene_state();
    }
}

std::size_t CameraManager::slot_index(CameraSlot slot) noexcept
{
    const std::size_t index = static_cast<std::size_t>(slot);
    const std::size_t slot_count = static_cast<std::size_t>(CameraSlot::Count);
    assert(index < slot_count);
    return index < slot_count ? index : static_cast<std::size_t>(CameraSlot::Main);
}

CameraManager::CameraRig& CameraManager::rig(CameraSlot slot) noexcept
{
    return _rigs[slot_index(slot)];
}

const CameraManager::CameraRig& CameraManager::rig(CameraSlot slot) const noexcept
{
    return _rigs[slot_index(slot)];
}

void CameraManager::process_requests()
{
    while (!_requests.empty())
    {
        CameraRequest request = std::move(_requests.front());
        _requests.pop_front();

        CameraController& target = rig(request.slot).controller;
        std::visit(
            [&target](auto&& payload)
            {
                using Payload = std::remove_cvref_t<decltype(payload)>;

                if constexpr (std::is_same_v<Payload, ShakeRequest>)
                    target.start_shake(payload.params);
                else if constexpr (std::is_same_v<Payload, SnapToFocusRequest>)
                    target.snap_to_focus();
                else if constexpr (std::is_same_v<Payload, ClearEffectsRequest>)
                    target.clear_effects();
            },
            request.payload
        );
    }
}
}
