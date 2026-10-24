#pragma once

#include "../core/geometry/vector2.h"

namespace elysia::camera
{
struct CameraShakeParams
{
    elysia::core::Vector2 amplitude = elysia::core::Vector2(8.0f, 8.0f);
    double duration_seconds = 0.25;
    double frequency_hz = 24.0;
};

class CameraEffect
{
public:
    virtual ~CameraEffect() = default;

    [[nodiscard]] virtual elysia::core::Vector2 update(double delta_seconds) = 0;
    [[nodiscard]] virtual bool is_finished() const noexcept = 0;
};

class CameraShakeEffect final : public CameraEffect
{
public:
    explicit CameraShakeEffect(const CameraShakeParams& params) noexcept;

    [[nodiscard]] elysia::core::Vector2 update(double delta_seconds) override;
    [[nodiscard]] bool is_finished() const noexcept override;

private:
    CameraShakeParams _params;
    double _elapsed_seconds = 0.0;
};
}
