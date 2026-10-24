#pragma once

#include "../../../animation/animation.h"
#include "../../../core/render/color.h"
#include "../../../core/interface/updatable.h"
#include "../../core/ui_element.h"
#include "../../../builtin/resources/builtin_resource_ids.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace elysia::ui
{
// Displays a registered frame animation inside the UI render pipeline.
class UiAnimation final : public UiElement, public elysia::core::Updatable
{
public:
    UiAnimation(const elysia::core::Rect& rect, int order = 0);
    UiAnimation(std::string_view animation_key, const elysia::core::Vector2& position,
        const elysia::core::Vector2& size, int order = 0);
    UiAnimation(std::string_view animation_key, const elysia::core::Rect& rect, int order = 0);
    UiAnimation(std::string_view animation_key, const elysia::core::Vector2& center,
        const elysia::core::Vector2& size, UiFromCenterTag, int order = 0);

    // Binds a registered animation and immediately starts it from its first frame.
    // Returns false when the key is not registered.
    bool set_animation_key(std::string_view animation_key);
    // Binds a persistent built-in animation without using the project AnimationManager.
    bool set_engine_animation(
        elysia::builtin::BuiltinAnimationId animation_id);
    [[nodiscard]] const std::string& animation_key() const noexcept;

    // Overrides the registered animation's loop setting for this widget. The override is retained
    // when the animation key changes.
    void set_loop(bool loop);
    [[nodiscard]] bool is_looping() const noexcept;
    void set_color_overlay(
        std::optional<elysia::core::Color> color_overlay) noexcept;
    [[nodiscard]] const std::optional<elysia::core::Color>&
        color_overlay() const noexcept;

    void play();
    void pause();
    void resume();
    void reset() noexcept override;

    [[nodiscard]] bool is_finished() const noexcept;
    [[nodiscard]] bool is_paused() const noexcept;

    void update(double delta) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

private:
    std::string _animation_key;
    std::unique_ptr<elysia::animation::Animation> _animation;
    std::optional<bool> _loop_override;
    std::optional<bool> _default_loop;
    std::optional<elysia::core::Color> _color_overlay;
};
}
