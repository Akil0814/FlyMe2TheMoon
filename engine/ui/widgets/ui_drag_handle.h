#pragma once

#include "../../core/render/colors.h"
#include "../style/ui_style.h"
#include "../style/ui_interaction_style.h"
#include "../core/ui_control.h"

#include <functional>
#include <optional>

struct SDL_Texture;

namespace elysia::ui
{
enum class UiDragAxis
{
    Free,
    Horizontal,
    Vertical
};

// Optional textures for a drag handle across idle, focus, dragging, and disabled states.
struct UiDragHandleTextures
{
    SDL_Texture* idle = nullptr;
    SDL_Texture* focused = nullptr;
    SDL_Texture* dragging = nullptr;
    SDL_Texture* disabled = nullptr;
};

// Visual styling and fixed size for a drag handle thumb.
struct UiDragHandleStyle
{
    elysia::core::Vector2 size{ 18.0f,18.0f };
    std::optional<UiDragHandleTextures> textures = std::nullopt;
    UiChromeStyle chrome{};
};
struct UiDragHandleStyleOverrides
{
    std::optional<elysia::core::Vector2> size;
    std::optional<std::optional<UiDragHandleTextures>> textures;
    UiChromeStyleOverrides chrome{};
};
template<> struct UiStyleOverrideTraits<UiDragHandleStyle> { using Overrides=UiDragHandleStyleOverrides; static bool empty(const Overrides& o) noexcept { return !o.size&&!o.textures&&elysia::ui::empty(o.chrome); } static void apply(UiDragHandleStyle& s,const Overrides& o) noexcept { apply_ui_style_override(s.size,o.size); apply_ui_style_override(s.textures,o.textures); apply_ui_style_overrides(s.chrome,o.chrome); } };

// Configures axis constraints, drag bounds, and visuals for a handle.
struct UiDragHandleConfig
{
    UiDragAxis axis = UiDragAxis::Free;
    std::optional<elysia::core::Rect> drag_bounds = std::nullopt;
    UiDragHandleStyleOverrides style_overrides{};
};

using UiDragHandleDraggedCallback = std::function<void(const elysia::core::Vector2& center)>;
using UiDragHandleDragEndedCallback = std::function<void(const elysia::core::Vector2& center)>;

class UiDragHandle : public UiControl
{
public:
    explicit UiDragHandle(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiDragHandle(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiDragHandle(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;
    UiDragHandle(const elysia::core::Rect& rect,const UiDragHandleConfig& config,int order = 0) noexcept;
    UiDragHandle(const elysia::core::Vector2& position,const elysia::core::Vector2& size,const UiDragHandleConfig& config,int order = 0) noexcept;
    UiDragHandle(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,const UiDragHandleConfig& config,int order = 0) noexcept;
    ~UiDragHandle() override = default;

    void cancel_input_interaction() noexcept override
    {
        cancel_drag();
    }
    void reset() noexcept override;
    void set_enabled(bool enabled) override;
    void set_focused(bool focused) override;

    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

    // Applies axis, bounds, and style as one drag-handle configuration update.
    void set_drag_handle_config(const UiDragHandleConfig& config);
    [[nodiscard]] const UiDragHandleConfig& drag_handle_config() const noexcept;
    void set_base_style(const UiDragHandleStyle& style) noexcept;
    void set_style_overrides(const UiDragHandleStyleOverrides& overrides);
    [[nodiscard]] const UiDragHandleStyle& style() const noexcept;
    [[nodiscard]] const UiDragHandleStyleOverrides& style_overrides() const noexcept;
    [[nodiscard]] bool has_style_overrides() const noexcept;
    void clear_style_overrides() noexcept;

    void set_drag_axis(UiDragAxis axis) noexcept;
    [[nodiscard]] UiDragAxis drag_axis() const noexcept;

    void set_drag_bounds(const elysia::core::Rect& bounds) noexcept;
    void clear_drag_bounds() noexcept;
    [[nodiscard]] const std::optional<elysia::core::Rect>& drag_bounds() const noexcept;

    void set_on_dragged(UiDragHandleDraggedCallback on_dragged);
    void set_on_drag_ended(UiDragHandleDragEndedCallback on_drag_ended);

    // Starts a drag session anchored to the current pointer position.
    void begin_drag_from_pointer(const elysia::core::Vector2& pointer) noexcept;
    // Cancels the current drag without emitting a drag-ended callback.
    void cancel_drag() noexcept;
    [[nodiscard]] bool is_dragging() const noexcept;

private:
    // Applies the config payload without exposing intermediate drag state.
    void apply_drag_handle_config(const UiDragHandleConfig& config);
    [[nodiscard]] bool can_receive_pointer() const noexcept;
    [[nodiscard]] bool contains_pointer(int mouse_x,int mouse_y) const noexcept;
    [[nodiscard]] bool is_primary_pointer_event(const UiInputEvent& event) const noexcept;
    // Moves the handle toward the pointer while honoring axis and bounds constraints.
    [[nodiscard]] bool drag_to_pointer(int mouse_x,int mouse_y);
    // Captures grab offset and enters dragging state for subsequent pointer moves.
    void begin_drag_session(const elysia::core::Vector2& pointer) noexcept;
    // Clamps a candidate handle rect to the configured drag bounds.
    [[nodiscard]] elysia::core::Rect clamped_rect(const elysia::core::Rect& rect) const noexcept;
    // Chooses the texture that matches the current interaction state.
    [[nodiscard]] SDL_Texture* current_state_texture() const noexcept;
    [[nodiscard]] elysia::core::Color current_background_color() const noexcept;
    [[nodiscard]] elysia::core::Color current_border_color() const noexcept;

private:
    UiDragHandleConfig _config{};
    UiStyleState<UiDragHandleStyle> _style_state;
    UiDragHandleDraggedCallback _on_dragged;
    UiDragHandleDragEndedCallback _on_drag_ended;
    elysia::core::Vector2 _grab_offset{};
    bool _is_dragging = false;
};
}
