#include "camera_effect.h"

#include <algorithm>
#include <cmath>

namespace elysia::camera
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
}

CameraShakeEffect::CameraShakeEffect(const CameraShakeParams& params) noexcept
    : _params(params)
{
    _params.amplitude.x = std::max(0.0f, _params.amplitude.x);
    _params.amplitude.y = std::max(0.0f, _params.amplitude.y);
    _params.duration_seconds = std::max(0.0, _params.duration_seconds);
    _params.frequency_hz = std::max(0.0, _params.frequency_hz);
}

elysia::core::Vector2 CameraShakeEffect::update(double delta_seconds)
{
    if (is_finished())
    {
        return elysia::core::Vector2::zero();
    }

    _elapsed_seconds += std::max(0.0, delta_seconds);

    if (is_finished())
    {
        return elysia::core::Vector2::zero();
    }

    const double progress = _params.duration_seconds <= 0.0
        ? 1.0
        : std::clamp(_elapsed_seconds / _params.duration_seconds, 0.0, 1.0);
    const float envelope = static_cast<float>(1.0 - progress);
    const double angle = _elapsed_seconds * _params.frequency_hz * (kPi * 2.0);

    return elysia::core::Vector2(
        _params.amplitude.x * static_cast<float>(std::sin(angle)) * envelope,
        _params.amplitude.y * static_cast<float>(std::cos(angle * 1.17)) * envelope
    );
}

bool CameraShakeEffect::is_finished() const noexcept
{
    return _params.duration_seconds <= 0.0
        || _elapsed_seconds >= _params.duration_seconds;
}
}
