#include "scene_camera_runtime.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace elysia::scene
{
namespace
{
[[nodiscard]] std::size_t slot_index(elysia::camera::CameraSlot slot) noexcept
{
    return static_cast<std::size_t>(slot);
}
} // namespace

SceneCameraRuntime::SceneCameraRuntime(CameraSceneConfig config)
    : _config(std::move(config)), _presented_slot(_config.initial_slot)
{
    if (!elysia::camera::valid_camera_slot(_config.initial_slot))
        throw std::invalid_argument("Scene initial camera slot is invalid.");
    if (!_config.owned_slots.valid())
        throw std::invalid_argument("Scene owned camera slots contain an invalid slot.");
    if (_config.owned_slots.empty())
        throw std::invalid_argument("Scene must own at least one camera slot.");
    if (!_config.owned_slots.contains(_config.initial_slot))
        throw std::invalid_argument("Scene must own its initial camera slot.");
}

const elysia::camera::Camera& SceneCameraRuntime::presented_camera() const noexcept
{
    if (_blend)
        return _blended_camera;
    return elysia::camera::CameraManager::instance()->camera(_presented_slot);
}

const elysia::camera::Camera& SceneCameraRuntime::slot_camera(
    elysia::camera::CameraSlot slot) const
{
    validate_owned_slot(slot);
    return elysia::camera::CameraManager::instance()->camera(slot);
}

void SceneCameraRuntime::validate_owned_slot(elysia::camera::CameraSlot slot) const
{
    if (!elysia::camera::valid_camera_slot(slot))
        throw std::invalid_argument("Camera slot is invalid.");
    if (!_config.owned_slots.contains(slot))
        throw std::logic_error("Scene does not own the requested camera slot.");
}

void SceneCameraRuntime::set_center(
    elysia::camera::CameraSlot slot, const elysia::core::Vector2& center)
{
    validate_owned_slot(slot);
    elysia::camera::CameraManager::instance()->set_center(slot, center);
    _motions[slot_index(slot)].reset();
}

void SceneCameraRuntime::set_zoom(elysia::camera::CameraSlot slot, float zoom)
{
    validate_owned_slot(slot);
    elysia::camera::CameraManager::instance()->set_zoom(slot, zoom);
    _motions[slot_index(slot)].reset();
}

void SceneCameraRuntime::set_focus(
    elysia::camera::CameraSlot slot,
    std::optional<elysia::camera::CameraFocus> focus)
{
    validate_owned_slot(slot);
    elysia::camera::CameraManager::instance()->set_focus(slot, std::move(focus));
}

void SceneCameraRuntime::set_world_bounds(
    elysia::camera::CameraSlot slot,
    std::optional<elysia::core::Rect> world_bounds)
{
    validate_owned_slot(slot);
    elysia::camera::CameraManager::instance()->set_world_bounds(slot, std::move(world_bounds));
}

void SceneCameraRuntime::set_follow_strategy(
    elysia::camera::CameraSlot slot,
    std::unique_ptr<elysia::camera::IFollowStrategy> follow_strategy)
{
    validate_owned_slot(slot);
    elysia::camera::CameraManager::instance()->set_follow_strategy(
        slot, std::move(follow_strategy));
}

void SceneCameraRuntime::request_shake(
    elysia::camera::CameraSlot slot,
    const elysia::camera::CameraShakeParams& params)
{
    validate_owned_slot(slot);
    elysia::camera::CameraManager::instance()->request_shake(slot, params);
}

void SceneCameraRuntime::snap_to_focus(elysia::camera::CameraSlot slot)
{
    validate_owned_slot(slot);
    elysia::camera::CameraManager::instance()->request_snap_to_focus(slot);
}

void SceneCameraRuntime::clear_effects(elysia::camera::CameraSlot slot)
{
    validate_owned_slot(slot);
    elysia::camera::CameraManager::instance()->request_clear_effects(slot);
}

void SceneCameraRuntime::cut_to(elysia::camera::CameraSlot slot)
{
    validate_owned_slot(slot);
    _blend.reset();
    _presented_slot = slot;
}

std::optional<elysia::camera::CameraBlendId> SceneCameraRuntime::blend_to(
    elysia::camera::CameraSlot slot,
    const elysia::camera::CameraBlendSpec& spec)
{
    validate_owned_slot(slot);
    elysia::camera::validate_camera_blend(spec);
    if (!_blend && slot == _presented_slot)
        return std::nullopt;

    const elysia::camera::Camera source = presented_camera();
    const elysia::camera::CameraBlendId id{_next_blend_id++};
    _blend = ActiveBlend{.id = id, .target = slot, .source = source, .spec = spec};
    _blended_camera = source;
    return id;
}

std::optional<elysia::camera::CameraBlendState> SceneCameraRuntime::blend_state(
    elysia::camera::CameraBlendId id) const noexcept
{
    if (!_blend || _blend->id != id)
        return std::nullopt;
    return _blend->paused
        ? elysia::camera::CameraBlendState::Paused
        : elysia::camera::CameraBlendState::Playing;
}

bool SceneCameraRuntime::pause_blend(elysia::camera::CameraBlendId id) noexcept
{
    if (!_blend || _blend->id != id)
        return false;
    _blend->paused = true;
    return true;
}

bool SceneCameraRuntime::resume_blend(elysia::camera::CameraBlendId id) noexcept
{
    if (!_blend || _blend->id != id)
        return false;
    _blend->paused = false;
    return true;
}

bool SceneCameraRuntime::cancel_blend(elysia::camera::CameraBlendId id) noexcept
{
    if (!_blend || _blend->id != id)
        return false;
    _blend.reset();
    return true;
}

elysia::camera::CameraMotionId SceneCameraRuntime::move_to(
    elysia::camera::CameraSlot slot,
    const elysia::camera::CameraPoseTarget& target,
    double duration_seconds,
    elysia::camera::CameraEasing easing,
    elysia::camera::CameraMotionEndBehavior end_behavior)
{
    validate_owned_slot(slot);
    const auto id = elysia::camera::CameraManager::instance()->move_camera_to(
        slot, target, duration_seconds, easing, end_behavior);
    _motions[slot_index(slot)] = id;
    return id;
}

elysia::camera::CameraMotionId SceneCameraRuntime::play_path(
    elysia::camera::CameraSlot slot,
    const elysia::camera::CameraMotionSpec& motion)
{
    validate_owned_slot(slot);
    const auto id = elysia::camera::CameraManager::instance()->play_camera_path(slot, motion);
    _motions[slot_index(slot)] = id;
    return id;
}

std::optional<std::size_t> SceneCameraRuntime::owned_motion_index(
    elysia::camera::CameraMotionId id) const noexcept
{
    if (id.value == 0)
        return std::nullopt;
    for (std::size_t index = 0; index < _motions.size(); ++index)
        if (_motions[index] && *_motions[index] == id)
            return index;
    return std::nullopt;
}

std::optional<elysia::camera::CameraMotionState> SceneCameraRuntime::motion_state(
    elysia::camera::CameraMotionId id) const noexcept
{
    if (!owned_motion_index(id))
        return std::nullopt;
    return elysia::camera::CameraManager::instance()->camera_motion_state(id);
}

bool SceneCameraRuntime::pause_motion(elysia::camera::CameraMotionId id) noexcept
{
    return owned_motion_index(id)
        && elysia::camera::CameraManager::instance()->pause_camera_motion(id);
}

bool SceneCameraRuntime::resume_motion(elysia::camera::CameraMotionId id) noexcept
{
    return owned_motion_index(id)
        && elysia::camera::CameraManager::instance()->resume_camera_motion(id);
}

bool SceneCameraRuntime::cancel_motion(elysia::camera::CameraMotionId id) noexcept
{
    const auto index = owned_motion_index(id);
    if (!index)
        return false;
    const bool cancelled =
        elysia::camera::CameraManager::instance()->cancel_camera_motion(id);
    _motions[*index].reset();
    return cancelled;
}

SceneCameraUpdateResult SceneCameraRuntime::advance(double delta_seconds)
{
    SceneCameraUpdateResult result;
    auto* manager = elysia::camera::CameraManager::instance();
    result.motions = manager->update(_config.owned_slots, delta_seconds);
    for (const auto& completion : result.motions)
    {
        const auto index = slot_index(completion.slot);
        if (_motions[index] && *_motions[index] == completion.id
            && !manager->camera_motion_state(completion.id))
            _motions[index].reset();
    }
    for (auto& motion : _motions)
        if (motion && !manager->camera_motion_state(*motion))
            motion.reset();

    if (_blend && !_blend->paused)
    {
        ActiveBlend& blend = *_blend;
        blend.elapsed_seconds += std::max(0.0, delta_seconds);
        const double progress = std::clamp(
            blend.elapsed_seconds / blend.spec.duration_seconds, 0.0, 1.0);
        const float eased = static_cast<float>(
            elysia::camera::apply_camera_easing(blend.spec.easing, progress));
        const elysia::camera::Camera& target = manager->camera(blend.target);
        _blended_camera.set_center(
            blend.source.center() + (target.center() - blend.source.center()) * eased);
        _blended_camera.set_zoom(
            blend.source.zoom() + (target.zoom() - blend.source.zoom()) * eased);
        _blended_camera.set_viewport_size(target.viewport_size());

        if (progress >= 1.0)
        {
            result.blend = CameraBlendCompletion{blend.id, blend.target};
            _presented_slot = blend.target;
            _blend.reset();
        }
    }
    return result;
}

void SceneCameraRuntime::advance(
    double delta_seconds,
    bool scene_paused,
    SceneCameraRuntimeHost& host)
{
    if (scene_paused && !_config.advance_when_paused)
        return;

    if (_config.focus_mode == CameraFocusMode::ResolveEachFrame)
    {
        for (std::size_t index = 0;
             index < static_cast<std::size_t>(elysia::camera::CameraSlot::Count);
             ++index)
        {
            const auto slot = static_cast<elysia::camera::CameraSlot>(index);
            if (_config.owned_slots.contains(slot))
                set_focus(slot, host.resolve_camera_focus(slot));
        }
    }

    const SceneCameraUpdateResult result = advance(delta_seconds);
    for (const auto& completion : result.motions)
        host.on_camera_motion_completed(completion.id, completion.slot);
    if (result.blend)
        host.on_camera_blend_completed(result.blend->id, result.blend->slot);
}

void SceneCameraRuntime::cancel_activity() noexcept
{
    _blend.reset();
    for (auto& motion : _motions)
    {
        if (motion)
            (void)elysia::camera::CameraManager::instance()->cancel_camera_motion(*motion);
        motion.reset();
    }
}

void SceneCameraRuntime::reset() noexcept
{
    cancel_activity();
    _presented_slot = _config.initial_slot;
    elysia::camera::CameraManager::instance()->reset(_config.owned_slots);
}
} // namespace elysia::scene
