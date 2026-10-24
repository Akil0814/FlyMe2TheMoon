#pragma once
#include "../../input/input_suppression.h"

#include <cstdint>
#include <vector>

#include "../../core/scene_object.h"
#include "../../core/geometry/vector2.h"
#include "../../core/geometry/rect.h"
#include "../../core/render/render_command.h"
#include "../effects/ui_translation_animation_player.h"

#include <optional>
#include <memory>
#include <string>
#include <string_view>

namespace elysia::scene { class SceneInputRouter; }

namespace elysia::ui
{
class UiChildHost;
// Tag type for constructors that interpret the supplied position as a center point.
struct UiFromCenterTag
{
    explicit constexpr UiFromCenterTag() noexcept = default;
};

inline constexpr UiFromCenterTag from_center{};

class UiElement : public elysia::core::SceneObject
{
public:

    // Higher UI order values are rendered on top and receive input first.
  explicit UiElement(const elysia::core::Rect &rect = elysia::core::Rect::zero(), int order = 0) noexcept
      : _screen_rect(rect), _order(order)
  {
  }

    UiElement( const elysia::core::Vector2& position, const elysia::core::Vector2& size, int order = 0)
        noexcept : _screen_rect(position, size), _order(order) {}

    UiElement(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag, int order = 0 )
        noexcept : _screen_rect(elysia::core::Rect::from_center(center, size)), _order(order) {}

    ~UiElement() override;
    virtual void cancel_input_interaction()
    {
    }
    [[nodiscard]] virtual elysia::input::InputCapture input_capture() const noexcept
    {
        return elysia::input::InputCapture::None;
    }

    UiElement(const UiElement&) = delete;
    UiElement& operator=(const UiElement&) = delete;
    UiElement(UiElement&&) = delete;
    UiElement& operator=(UiElement&&) = delete;

    // Appends this element's draw commands in local visual order.
    virtual void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
    {
        (void)out_commands;
    }

    // Reports the size this element contributes to parent layout calculations.
    [[nodiscard]] virtual elysia::core::Vector2 content_extent() const noexcept
    {
        return size();
    }

    void reset() noexcept override
    {
        elysia::core::SceneObject::reset();
        _layout_parent = nullptr;
        _opacity = 255;
        _presentation_translation = {};
        _translation_animation_player.clear();
    }

    // Updates the screen-space bounds and invalidates intrinsic layout when the size changes.
    void set_screen_rect(const elysia::core::Rect& rect) noexcept
    {
        if (_screen_rect.nearly_equals(rect))
            return;

        const bool size_changed = !_screen_rect.size().nearly_equals(rect.size());
        _screen_rect = rect;
        if (size_changed)
            notify_layout_parent_of_intrinsic_layout_invalidation();
    }

    void set_position(const elysia::core::Vector2& position) noexcept
    {
        if (_screen_rect.position().nearly_equals(position))
            return;
        _screen_rect.set_position(position);
    }

    void set_center(const elysia::core::Vector2& center) noexcept
    {
        if (_screen_rect.center().nearly_equals(center))
            return;
        _screen_rect.set_center(center);
    }

    void set_size(const elysia::core::Vector2& size) noexcept
    {
        if (_screen_rect.size().nearly_equals(size))
            return;
        elysia::core::Rect next_rect = _screen_rect;
        next_rect.set_size(size);
        set_screen_rect(next_rect);
    }

    [[nodiscard]] const elysia::core::Rect& screen_rect() const noexcept { return _screen_rect; }
    // Returns the element's local layout-independent visual translation.
    void set_presentation_translation(const elysia::core::Vector2& translation) noexcept { _presentation_translation = translation; }
    [[nodiscard]] const elysia::core::Vector2& presentation_translation() const noexcept { return _presentation_translation; }
    // Includes this element and all layout ancestors' presentation translations.
    [[nodiscard]] elysia::core::Vector2 accumulated_presentation_translation() const noexcept;
    // Geometry used by pointer hit testing and external presentation anchors.
    [[nodiscard]] elysia::core::Rect presentation_screen_rect() const noexcept;
    [[nodiscard]] elysia::core::Vector2 presentation_to_layout_point(const elysia::core::Vector2& point) const noexcept
    {
        return point - accumulated_presentation_translation();
    }

    void bind_translation_animation(std::string name,UiTranslationAnimation animation);
    [[nodiscard]] bool remove_translation_animation(std::string_view name);
    void clear_translation_animations() noexcept;
    [[nodiscard]] bool play_translation_animation(std::string_view name) noexcept;
    void stop_translation_animation() noexcept;
    [[nodiscard]] bool is_translation_animation_playing() const noexcept;
    [[nodiscard]] std::optional<std::string> active_translation_animation() const;
    // Scene and child hosts call this once per frame; it does not touch layout geometry.
    virtual void update_presentation_animations(double delta);
    [[nodiscard]] elysia::core::Vector2 position() const noexcept { return _screen_rect.position(); }
    [[nodiscard]] elysia::core::Vector2 center() const noexcept { return _screen_rect.center(); }
    [[nodiscard]] elysia::core::Vector2 size() const noexcept { return _screen_rect.size(); }
    [[nodiscard]] UiChildHost* layout_parent() noexcept { return _layout_parent; }
    [[nodiscard]] const UiChildHost* layout_parent() const noexcept { return _layout_parent; }

    void set_order(int order) noexcept;
    [[nodiscard]] int order() const noexcept { return _order; }

    void set_opacity(std::uint8_t opacity) noexcept { _opacity = opacity; }
    [[nodiscard]] std::uint8_t opacity() const noexcept { return _opacity; }

    [[nodiscard]] bool update_when_paused() const override{ return true;}

    [[nodiscard]] bool receive_input_when_paused() const override{ return true;}

protected:
    // Tells the owning layout host that this element's intrinsic size may have changed.
    void notify_layout_parent_of_intrinsic_layout_invalidation() noexcept;
    // Reports that this element's semantic visual role changed. The owning container decides
    // whether an external styling system needs to recompute its base style.
    void notify_base_style_invalidated() noexcept;

    void apply_opacity(elysia::core::UiRenderCommand& command) const noexcept
    {
        switch (command.type)
        {
        case elysia::core::UiRenderCommandType::Texture:
            command.alpha = multiply_alpha(command.alpha, _opacity);
            break;
        case elysia::core::UiRenderCommandType::FillRect:
        case elysia::core::UiRenderCommandType::DrawRect:
        case elysia::core::UiRenderCommandType::FillRoundedRect:
        case elysia::core::UiRenderCommandType::DrawRoundedRect:
        case elysia::core::UiRenderCommandType::DrawLine:
        case elysia::core::UiRenderCommandType::FillCircle:
        case elysia::core::UiRenderCommandType::DrawCircle:
            command.color = apply_opacity(command.color);
            break;
        default:
            break;
        }
    }

    [[nodiscard]] elysia::core::Color apply_opacity(elysia::core::Color color) const noexcept
    {
        color.a = multiply_alpha(color.a, _opacity);
        return color;
    }

private:
    friend class UiChildHost;
    friend class elysia::scene::SceneInputRouter;
    // Created only for root cancellation snapshots; destruction invalidates all snapshots.
    struct CancellationLifetime { UiElement* element = nullptr; };
    std::shared_ptr<CancellationLifetime> _cancellation_lifetime;

    static std::uint8_t multiply_alpha(std::uint8_t a, std::uint8_t b) noexcept
    {
        return static_cast<std::uint8_t>((static_cast<unsigned int>(a) * static_cast<unsigned int>(b)) / 255U);
    }

    void set_layout_parent(UiChildHost* parent) noexcept { _layout_parent = parent; }

    elysia::core::Rect _screen_rect{};
    UiChildHost* _layout_parent = nullptr;
    int _order = 0;
    std::uint8_t _opacity = 255;
    elysia::core::Vector2 _presentation_translation{};
    UiTranslationAnimationPlayer _translation_animation_player;
};
}
