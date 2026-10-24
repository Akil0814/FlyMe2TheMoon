#include "camera_motion.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace elysia::camera
{
namespace
{
[[nodiscard]] bool finite_vector(const elysia::core::Vector2& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}
}

double apply_camera_easing(CameraEasing easing, double progress) noexcept
{
    const double t = std::clamp(progress, 0.0, 1.0);
    switch (easing)
    {
    case CameraEasing::Linear:
        return t;
    case CameraEasing::SmoothStep:
        return t * t * (3.0 - 2.0 * t);
    case CameraEasing::EaseInOutCubic:
        return t < 0.5
            ? 4.0 * t * t * t
            : 1.0 - std::pow(-2.0 * t + 2.0, 3.0) * 0.5;
    }
    return t;
}

void validate_camera_motion(const CameraMotionSpec& motion)
{
    if (motion.nodes.empty())
        throw std::invalid_argument("Camera motion requires at least one path node.");

    for (const CameraPathNode& node : motion.nodes)
    {
        if (!std::isfinite(node.duration_seconds) || node.duration_seconds <= 0.0)
            throw std::invalid_argument("Camera path node duration must be finite and positive.");
        if (!node.target.center && !node.target.zoom)
            throw std::invalid_argument("Camera path node must target a center, a zoom, or both.");
        if (node.target.center && !finite_vector(*node.target.center))
            throw std::invalid_argument("Camera path node center must be finite.");
        if (node.target.zoom && !std::isfinite(*node.target.zoom))
            throw std::invalid_argument("Camera path node zoom must be finite.");
    }
}

void validate_camera_blend(const CameraBlendSpec& blend)
{
    if (!std::isfinite(blend.duration_seconds) || blend.duration_seconds <= 0.0)
        throw std::invalid_argument("Camera blend duration must be finite and positive.");
}
} // namespace elysia::camera
