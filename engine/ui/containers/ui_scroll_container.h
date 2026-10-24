#pragma once

#include "../../core/render/render_command.h"
#include "../core/ui_child_host.h"
#include "../focus/ui_focus_scope.h"
#include "../scroll/ui_scroll_state.h"
#include "../style/ui_style.h"
#include "../widgets/ui_drag_handle.h"

namespace elysia::ui
{
class UiWindow;
enum class UiScrollBarVisibility
{
    Hidden,
    Auto,
    Always
};

// Visual settings for scrollbar tracks and thumbs owned by a scroll container.
struct UiScrollBarStyle
{
    float corner_radius = 0.0f;
    float thickness = 10.0f;
    float margin = 4.0f;
    float min_thumb_length = 24.0f;
    bool draw_track = true;
    elysia::core::Color track_idle_color{};
    elysia::core::Color track_focused_color{};
    elysia::core::Color track_dragging_color{};
    elysia::core::Color track_disabled_color{};
    elysia::core::Color thumb_idle_color{};
    elysia::core::Color thumb_focused_color{};
    elysia::core::Color thumb_dragging_color{};
    elysia::core::Color thumb_disabled_color{};
};
struct UiScrollBarStyleOverrides
{
    std::optional<float> corner_radius,thickness,margin,min_thumb_length;
    std::optional<bool> draw_track;
    std::optional<elysia::core::Color> track_idle_color,track_focused_color,track_dragging_color,track_disabled_color;
    std::optional<elysia::core::Color> thumb_idle_color,thumb_focused_color,thumb_dragging_color,thumb_disabled_color;
};

// Visual settings for the scroll viewport chrome plus its scrollbar theme.
struct UiScrollContainerStyle
{
    float corner_radius = 0.0f;
    elysia::core::UiStrokeWidth border_width{};
    UiScrollBarStyle scrollbar{};
    bool draw_background = true;
    elysia::core::Color background_color{};
    bool draw_border = true;
    elysia::core::Color border_color{};
};
struct UiScrollContainerStyleOverrides
{
    UiScrollBarStyleOverrides scrollbar{};
    std::optional<float> corner_radius;
    std::optional<elysia::core::UiStrokeWidth> border_width;
    std::optional<bool> draw_background,draw_border;
    std::optional<elysia::core::Color> background_color,border_color;
};
template<> struct UiStyleOverrideTraits<UiScrollContainerStyle> { using Overrides=UiScrollContainerStyleOverrides;
static bool scrollbar_empty(const UiScrollBarStyleOverrides& o) noexcept { return !o.corner_radius&&!o.thickness&&!o.margin&&!o.min_thumb_length&&!o.draw_track&&!o.track_idle_color&&!o.track_focused_color&&!o.track_dragging_color&&!o.track_disabled_color&&!o.thumb_idle_color&&!o.thumb_focused_color&&!o.thumb_dragging_color&&!o.thumb_disabled_color; }
static bool empty(const Overrides& o) noexcept { return scrollbar_empty(o.scrollbar)&&!o.corner_radius&&!o.border_width&&!o.draw_background&&!o.draw_border&&!o.background_color&&!o.border_color; }
static void apply_scrollbar(UiScrollBarStyle& s,const UiScrollBarStyleOverrides& o) noexcept { apply_ui_style_override(s.corner_radius,o.corner_radius); apply_ui_style_override(s.thickness,o.thickness); apply_ui_style_override(s.margin,o.margin); apply_ui_style_override(s.min_thumb_length,o.min_thumb_length); apply_ui_style_override(s.draw_track,o.draw_track); apply_ui_style_override(s.track_idle_color,o.track_idle_color); apply_ui_style_override(s.track_focused_color,o.track_focused_color); apply_ui_style_override(s.track_dragging_color,o.track_dragging_color); apply_ui_style_override(s.track_disabled_color,o.track_disabled_color); apply_ui_style_override(s.thumb_idle_color,o.thumb_idle_color); apply_ui_style_override(s.thumb_focused_color,o.thumb_focused_color); apply_ui_style_override(s.thumb_dragging_color,o.thumb_dragging_color); apply_ui_style_override(s.thumb_disabled_color,o.thumb_disabled_color); }
static void apply(UiScrollContainerStyle& s,const Overrides& o) noexcept { apply_scrollbar(s.scrollbar,o.scrollbar); apply_ui_style_override(s.corner_radius,o.corner_radius); apply_ui_style_override(s.border_width,o.border_width); apply_ui_style_override(s.draw_background,o.draw_background); apply_ui_style_override(s.draw_border,o.draw_border); apply_ui_style_override(s.background_color,o.background_color); apply_ui_style_override(s.border_color,o.border_color); }};

class UiScrollContainer : public UiChildHost, public UiFocusScope
{
    friend class UiWindow;
public:
    explicit UiScrollContainer(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiScrollContainer(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiScrollContainer(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;
    ~UiScrollContainer() override = default;

    void reset() noexcept override;
    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

    // Replaces any existing content child and keeps scroll behavior scoped to a single payload.
    UiElement* add_child(std::unique_ptr<UiElement> child,UiLayoutChildOptions options = {}) override;
    // Swaps the scrollable content while preserving viewport-owned layout state.
    UiElement* set_content(std::unique_ptr<UiElement> content);
    [[nodiscard]] const UiElement* content() const noexcept;
    void clear_content();

    void set_scroll_axis(UiScrollAxis axis) noexcept;
    [[nodiscard]] UiScrollAxis scroll_axis() const noexcept;
    [[nodiscard]] UiScrollAxis resolved_scroll_axis() const noexcept;

    void set_base_style(const UiScrollContainerStyle& style) noexcept;
    void set_style_overrides(const UiScrollContainerStyleOverrides& overrides) noexcept;
    [[nodiscard]] const UiScrollContainerStyle& style() const noexcept;
    [[nodiscard]] const UiScrollContainerStyleOverrides& style_overrides() const noexcept;
    [[nodiscard]] bool has_style_overrides() const noexcept;
    void clear_style_overrides() noexcept;

    void set_scrollbar_visibility(UiScrollBarVisibility visibility) noexcept;
    [[nodiscard]] UiScrollBarVisibility scrollbar_visibility() const noexcept;
    void set_scrollbar_style_overrides(const UiScrollBarStyleOverrides& overrides);
    [[nodiscard]] const UiScrollBarStyle& scrollbar_style() const noexcept;
    [[nodiscard]] elysia::core::Vector2 content_size() const noexcept;

    void set_scroll_offset(const elysia::core::Vector2& scroll_offset) noexcept;
    [[nodiscard]] elysia::core::Vector2 scroll_offset() const noexcept;
    void set_scroll_offset_x(float scroll_offset_x) noexcept;
    [[nodiscard]] float scroll_offset_x() const noexcept;
    void set_scroll_offset_y(float scroll_offset_y) noexcept;
    [[nodiscard]] float scroll_offset_y() const noexcept;
    [[nodiscard]] elysia::core::Vector2 max_scroll_offset() const noexcept;

    void set_scroll_step(const elysia::core::Vector2& scroll_step) noexcept;
    [[nodiscard]] elysia::core::Vector2 scroll_step() const noexcept;
    void set_scroll_step_x(float scroll_step_x) noexcept;
    [[nodiscard]] float scroll_step_x() const noexcept;
    void set_scroll_step_y(float scroll_step_y) noexcept;
    [[nodiscard]] float scroll_step_y() const noexcept;

    void scroll_by(const elysia::core::Vector2& delta) noexcept;
    void scroll_to_left() noexcept;
    void scroll_to_right() noexcept;
    void scroll_to_top() noexcept;
    void scroll_to_bottom() noexcept;
    // Scrolls just enough to reveal the requested content rect inside the viewport.
    void ensure_visible(const elysia::core::Rect& target_rect) noexcept;

    [[nodiscard]] UiElement& focus_scope_element() noexcept override;
    [[nodiscard]] const UiElement& focus_scope_element() const noexcept override;
    void set_scope_focused(bool focused) noexcept override;
    [[nodiscard]] bool is_scope_focused() const noexcept override;
    [[nodiscard]] bool has_focusable_target() const noexcept override;
    bool focus_first_available() override;
    [[nodiscard]] UiControl* focused_target() const noexcept override;
    [[nodiscard]] bool can_navigate(UiAction action) const noexcept override;
    bool clear_focus_for_gamepad_scroll() override;
    bool restore_focus_after_gamepad_scroll() override;
    [[nodiscard]] bool contains_focus_point(int mouse_x,int mouse_y) const noexcept override;

protected:
    // Rebuilds the viewport, content rect, and scrollbar geometry from current state.
    void rebuild_layout() override;
    // Keeps content measurement separate from viewport-only repositioning.
    void on_child_intrinsic_layout_invalidated(UiElement& child) noexcept override;

private:
    // Captures which scrollbars should currently be visible after Auto resolution.
    struct ScrollbarVisibilityState
    {
        bool horizontal = false;
        bool vertical = false;
    };

    void ensure_layout_current() noexcept;
    void reset_content_state() noexcept;
    void mark_layout_dirty_if_offset_changed(const elysia::core::Vector2& previous_offset) noexcept;
    // Returns the nested content focus scope when the payload participates in focus navigation.
    [[nodiscard]] UiFocusScope* content_scope() noexcept;
    [[nodiscard]] const UiFocusScope* content_scope() const noexcept;
    [[nodiscard]] UiElement* content_mutable() noexcept;
    // Swaps the content child without re-entering the public ownership path.
    [[nodiscard]] UiElement* set_content_internal(std::unique_ptr<UiElement> content,UiLayoutChildOptions options = {});
    // Routes an event into the content payload when viewport policy allows it.
    [[nodiscard]] bool dispatch_content_input_event(const UiInputEvent& event);
    // Filters events that should stay on the viewport rather than reaching content.
    [[nodiscard]] bool should_dispatch_content_input_event(const UiInputEvent& event) const noexcept;
    [[nodiscard]] bool should_dispatch_content_mouse_wheel(const UiInputEvent& event) const noexcept;
    // Consumes mouse-wheel input by translating it into scroll-state movement.
    [[nodiscard]] bool handle_mouse_wheel(const UiInputEvent& event);
    [[nodiscard]] bool apply_wheel_delta(const UiInputEvent& event);
    // Window-only path for a passive scroll target; never alters focus state.
    [[nodiscard]] bool handle_passive_scroll_input(const UiInputEvent& event);
    [[nodiscard]] bool is_passive_scroll_target_usable() const noexcept;
    // Gives scrollbar thumbs first chance to consume pointer drag interactions.
    [[nodiscard]] bool dispatch_to_scrollbars(const UiInputEvent& event);
    [[nodiscard]] bool supports_scroll_axis(UiScrollAxis axis) const noexcept;
    [[nodiscard]] bool can_scroll_axis(UiScrollAxis axis) const noexcept;
    [[nodiscard]] bool shows_scrollbar(UiScrollAxis axis) const noexcept;
    // Resolves Auto visibility into the current horizontal and vertical scrollbar state.
    [[nodiscard]] ScrollbarVisibilityState resolved_scrollbar_visibility() const noexcept;
    // Returns the outer interactive region used for hover focus and pointer capture.
    [[nodiscard]] elysia::core::Rect interactive_rect() const noexcept;
    // Returns the content viewport after subtracting visible scrollbar chrome.
    [[nodiscard]] elysia::core::Rect viewport_rect() const noexcept;
    [[nodiscard]] elysia::core::Rect viewport_rect(const ScrollbarVisibilityState& scrollbars) const noexcept;
    [[nodiscard]] bool is_pointer_in_interactive_rect(int mouse_x,int mouse_y) const noexcept;
    [[nodiscard]] bool is_pointer_in_viewport(int mouse_x,int mouse_y) const noexcept;
    // Computes track geometry for the requested scrollbar axis.
    [[nodiscard]] elysia::core::Rect scrollbar_track_rect(UiScrollAxis axis) const noexcept;
    // Computes thumb geometry from the visible viewport ratio and current offset.
    [[nodiscard]] elysia::core::Rect scrollbar_thumb_rect(UiScrollAxis axis,const elysia::core::Rect& track_rect) const noexcept;
    [[nodiscard]] elysia::core::Color current_track_color(const UiDragHandle& thumb) const noexcept;
    // Creates the internal drag handles used as scrollbar thumbs.
    void initialize_scrollbar_handles();
    // Updates scroll-state viewport metrics after host layout changes.
    void sync_scroll_state_to_viewport() noexcept;
    // Updates scroll-state content metrics after content or layout changes.
    void sync_scroll_state_to_content() noexcept;
    // Pushes scroll-state offsets back into the draggable thumb controls.
    void sync_scrollbar_handles() noexcept;
    // Reconfigures one scrollbar thumb to match the latest track and thumb geometry.
    void configure_scrollbar_thumb(
        UiDragHandle& thumb,
        UiScrollAxis axis,
        const elysia::core::Rect& track_rect,
        const elysia::core::Rect& thumb_rect
    ) noexcept;
    // Converts horizontal thumb dragging back into a content scroll offset.
    void update_horizontal_offset_from_thumb() noexcept;
    // Converts vertical thumb dragging back into a content scroll offset.
    void update_vertical_offset_from_thumb() noexcept;
    // Clears any stale scroll position after content ownership changes.
    void reset_scroll_offset() noexcept;
    // Drops pointer-active state that should not survive content replacement.
    void clear_content_pointer_state() noexcept;
    // Mirrors viewport focus state into the nested content focus scope.
    void sync_content_scope_focus() noexcept;
    // Temporarily blocks content focus while viewport-owned pointer interactions are active.
    void set_content_focus_suppressed(bool suppressed) noexcept;
    void update_content_focus_suppression(const UiInputEvent& event) noexcept;
    // Determines whether pointer activity should auto-focus the content scope.
    [[nodiscard]] bool should_auto_position_focus(const UiInputEvent& event) const noexcept;
    // Ensures input-driven focus changes remain visible inside the current viewport.
    void ensure_visible_focused_target_for_input(const UiInputEvent& event) noexcept;
    // Ensures the current focused target stays visible after scroll or focus changes.
    void ensure_visible_focused_target() noexcept;
    // Appends scrollbar track and thumb draw commands after content rendering.
    void submit_scrollbar_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const;
    // Measures scrollable content after layout so scroll ranges stay in sync.
    [[nodiscard]] elysia::core::Vector2 measured_content_size() const noexcept;

private:
    UiScrollState _scroll_state;
    UiScrollBarVisibility _scrollbar_visibility = UiScrollBarVisibility::Auto;
    UiStyleState<UiScrollContainerStyle> _style_state;
    UiLayoutChildOptions _content_layout{};
    UiDragHandle _horizontal_thumb;
    UiDragHandle _vertical_thumb;
    bool _scope_focused = false;
    bool _content_pointer_active = false;
    bool _content_size_dirty = true;
    bool _placing_content = false;
    bool _content_focus_suppressed = false;
    bool _gamepad_scroll_focus_restore_pending = false;
};
}


