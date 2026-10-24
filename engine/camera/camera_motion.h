#pragma once

#include "camera_types.h"
#include "../core/geometry/vector2.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace elysia::camera
{
enum class CameraEasing : std::uint8_t
{
    Linear,
    SmoothStep,
    EaseInOutCubic
};

struct CameraPoseTarget
{
    std::optional<elysia::core::Vector2> center;
    std::optional<float> zoom;
};

struct CameraPathNode
{
    CameraPoseTarget target;
    double duration_seconds = 0.35;
    CameraEasing easing = CameraEasing::SmoothStep;
};

enum class CameraMotionEndBehavior : std::uint8_t
{
    ResumeFollow,
    Hold
};

struct CameraMotionSpec
{
    std::vector<CameraPathNode> nodes;
    CameraMotionEndBehavior end_behavior = CameraMotionEndBehavior::ResumeFollow;
};

struct CameraMotionId
{
    std::uint64_t value = 0;
    friend bool operator==(const CameraMotionId&, const CameraMotionId&) = default;
};

enum class CameraMotionState : std::uint8_t
{
    Playing,
    Paused,
    Holding
};

struct CameraMotionCompletion
{
    CameraMotionId id;
    CameraSlot slot = CameraSlot::Main;
};

struct CameraUpdateResult
{
    std::array<CameraMotionCompletion, static_cast<std::size_t>(CameraSlot::Count)> values{};
    std::size_t count = 0;

    void push(CameraMotionCompletion completion) noexcept { values[count++] = completion; }
    [[nodiscard]] std::size_t size() const noexcept { return count; }
    [[nodiscard]] bool empty() const noexcept { return count == 0; }
    [[nodiscard]] const CameraMotionCompletion& front() const noexcept { return values.front(); }
    [[nodiscard]] const CameraMotionCompletion* begin() const noexcept { return values.data(); }
    [[nodiscard]] const CameraMotionCompletion* end() const noexcept
    {
        return values.data() + count;
    }
};

struct CameraBlendSpec
{
    double duration_seconds = 0.35;
    CameraEasing easing = CameraEasing::SmoothStep;
};

struct CameraBlendId
{
    std::uint64_t value = 0;
    friend bool operator==(const CameraBlendId&, const CameraBlendId&) = default;
};

enum class CameraBlendState : std::uint8_t
{
    Playing,
    Paused
};

[[nodiscard]] double apply_camera_easing(CameraEasing easing, double progress) noexcept;
void validate_camera_motion(const CameraMotionSpec& motion);
void validate_camera_blend(const CameraBlendSpec& blend);
} // namespace elysia::camera
