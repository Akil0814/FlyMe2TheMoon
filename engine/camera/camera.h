#pragma once

#include "../core/geometry/rect.h"
#include "../core/geometry/vector2.h"

namespace elysia::camera
{
class Camera
{
public:
    static constexpr float kDefaultZoom = 1.0f;
    static constexpr float kMinZoom = 0.1f;
    static constexpr float kMaxZoom = 10.0f;

    Camera() = default;
    Camera(const elysia::core::Vector2& center,const elysia::core::Vector2& viewport_size,
        float zoom = kDefaultZoom) noexcept;

    void set_center(const elysia::core::Vector2& center) noexcept;
    void set_viewport_size(const elysia::core::Vector2& viewport_size) noexcept;
    void set_zoom(float zoom) noexcept;

    [[nodiscard]] const elysia::core::Vector2& center() const noexcept;
    [[nodiscard]] const elysia::core::Vector2& viewport_size() const noexcept;
    [[nodiscard]] float zoom() const noexcept;
    [[nodiscard]] elysia::core::Vector2 world_viewport_size() const noexcept;
    [[nodiscard]] static float clamp_zoom(float zoom) noexcept;

    [[nodiscard]] elysia::core::Rect view_rect() const noexcept;
    [[nodiscard]] elysia::core::Vector2 world_to_screen(
        const elysia::core::Vector2& world_position
    ) const noexcept;
    [[nodiscard]] elysia::core::Rect world_to_screen(
        const elysia::core::Rect& world_rect
    ) const noexcept;
    [[nodiscard]] elysia::core::Vector2 screen_to_world(
        const elysia::core::Vector2& screen_position
    ) const noexcept;
    [[nodiscard]] elysia::core::Rect screen_to_world(
        const elysia::core::Rect& screen_rect
    ) const noexcept;

private:
    elysia::core::Vector2 _center{};
    elysia::core::Vector2 _viewport_size{};
    float _zoom = kDefaultZoom;
};
}
