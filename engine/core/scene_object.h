#pragma once

namespace elysia::core
{
class SceneObject
{
public:
    SceneObject() = default;
    virtual ~SceneObject() = default;

    SceneObject(const SceneObject&) = delete;
    SceneObject& operator=(const SceneObject&) = delete;
    SceneObject(SceneObject&&) = delete;
    SceneObject& operator=(SceneObject&&) = delete;

    virtual void reset() noexcept
    {
        _visible = true;
        _active = true;
        _destroyed = false;
    }

    void destroy() noexcept { _destroyed = true; }
    [[nodiscard]] bool is_destroyed() const noexcept { return _destroyed; }

    void set_visible(bool visible) noexcept { _visible = visible; }
    [[nodiscard]] bool is_visible() const noexcept { return _visible; }

    void set_active(bool active) noexcept { _active = active; }
    [[nodiscard]] bool is_active() const noexcept { return _active; }

    [[nodiscard]] virtual bool update_when_paused() const { return false; }
    [[nodiscard]] virtual bool receive_input_when_paused() const { return false; }

private:
    bool _destroyed = false;
    bool _visible = true;
    bool _active = true;
};
}
