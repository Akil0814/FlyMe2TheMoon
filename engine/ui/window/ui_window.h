#pragma once

#include "../core/ui_child_host.h"
#include "../focus/ui_focus_scope.h"
#include "../core/ui_control.h"
#include "../style/ui_style.h"
#include "../style/ui_visual_styles.h"
#include "ui_overlay.h"
#include "ui_transient_popup.h"
#include "../../input/input_types.h"

#include <functional>
#include <vector>

namespace elysia::ui
{
class UiScrollContainer;
using UiWindowCancelCallback = std::function<void()>;

class UiWindow : public UiChildHost
{
public:
    explicit UiWindow(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiWindow(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiWindow(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;
    ~UiWindow() override;

    void reset() noexcept override;

    void set_base_style(const UiWindowStyle& style) noexcept;
    void set_style_overrides(const UiWindowStyleOverrides& overrides) noexcept;
    [[nodiscard]] const UiWindowStyle& style() const noexcept;
    [[nodiscard]] const UiWindowStyleOverrides& style_overrides() const noexcept;
    [[nodiscard]] bool has_style_overrides() const noexcept;
    void clear_style_overrides() noexcept;
    void set_hover_focus_enabled(bool enabled) noexcept;
    [[nodiscard]] bool hover_focus_enabled() const noexcept;
    void set_on_cancel(UiWindowCancelCallback on_cancel);

    // Registers a focus scope that the window may route hover or navigation focus into.
    void register_focus_scope(UiFocusScope& scope,const UiFocusScopeNeighbors& neighbors = {});
    void unregister_focus_scope(UiFocusScope& scope);
    // Replaces directional navigation links for a registered focus scope.
    void set_scope_neighbors(UiFocusScope& scope,const UiFocusScopeNeighbors& neighbors);
    // Moves window-level focus to the requested scope when it is still usable.
    void set_focused_scope(UiFocusScope* scope);
    [[nodiscard]] UiFocusScope* focused_scope() const noexcept;
    // Chooses the first usable registered scope when the window gains focus.
    bool focus_first_available_scope();
    [[nodiscard]] elysia::input::InputDevice focus_input_device() const noexcept { return _focus_input_device; }
    // Passive target used only when gamepad or page-scroll input has no focused scroll scope.
    [[nodiscard]] const UiScrollContainer* gamepad_scroll_target() const noexcept;

    // Registers an owned child element as a window-managed overlay surface.
    [[nodiscard]] bool register_overlay(UiElement& element,UiOverlayOptions options = {});
    void unregister_overlay(UiElement& element);
    // Opens or closes a registered overlay and updates focus restoration state.
    void open_overlay(UiElement& element);
    void close_overlay(UiElement& element);
    [[nodiscard]] bool is_overlay_open(const UiElement& element) const noexcept;
    [[nodiscard]] UiOverlayOptions* overlay_options(UiElement& element) noexcept;

    // Registers a non-modal popup that the window draws above ordinary child content.
    void register_transient_popup(UiTransientPopup& popup);
    void unregister_transient_popup(UiTransientPopup& popup);
    // Makes one registered popup active and closes the previously active popup.
    void activate_transient_popup(UiTransientPopup& popup);
    // Registers a passive tooltip rendered above ordinary and transient popup content.
    void register_tooltip(class UiTooltip& tooltip);
    void unregister_tooltip(class UiTooltip& tooltip) noexcept;
    [[nodiscard]] elysia::core::Rect content_bounds() const noexcept;

    elysia::input::InputCapture input_capture() const noexcept override;
    void update(double delta) override;
    void on_ui_input_frame(const UiInputFrame& input) override;
    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

protected:
    // Rebuilds child layout and reapplies overlay placement after size changes.
    void rebuild_layout() override;

private:
    friend class UiTooltip;
    void attach_tooltip_content(UiElement& content);
    void detach_tooltip_content(UiElement& content) noexcept;
    [[nodiscard]] elysia::core::Rect tooltip_trigger_visible_rect(const UiElement& trigger) const noexcept;
    [[nodiscard]] bool tooltip_pointer_reaches_trigger(
        const UiElement& trigger,
        int mouse_x,
        int mouse_y
    ) const noexcept;
    [[nodiscard]] bool tooltip_focus_reaches_trigger(const UiElement& trigger) const noexcept;
    // Stores one registered focus scope plus its directional window neighbors.
    struct ScopeEntry
    {
        UiFocusScope* scope = nullptr;
        UiFocusScopeNeighbors neighbors;
    };

    // Stores overlay behavior and the scope that should regain focus after close.
    struct OverlayEntry
    {
        UiElement* element = nullptr;
        UiOverlayWindowClient* client = nullptr;
        UiOverlayOptions options;
        UiFocusScope* restore_focus_scope = nullptr;
    };

    struct TransientPopupEntry
    {
        UiTransientPopup* popup = nullptr;
        UiElement* owner = nullptr;
    };

private:
    // Removes registered scopes that no longer point at live window children.
    void prune_focus_scopes();
    // Removes overlay entries whose elements are no longer live window children.
    void prune_overlays();
    void prune_transient_popups();
    void prune_tooltips();
    void prune_gamepad_scroll_target() noexcept;
    // Breaks every reverse registration before reset/destruction releases children.
    void detach_window_registrations() noexcept;
    // Repairs scope focus after scope removal, disablement, or overlay changes.
    void ensure_valid_scope_focus();
    // Pushes window-level focus state into registered scopes.
    void apply_scope_focus();
    // Tracks the device that most recently drove window focus decisions.
    void update_focus_input_device(elysia::input::InputDevice device) noexcept;
    // Restores the previously focused scope when it is still usable.
    bool restore_preferred_scope_focus();
    // Returns the topmost open overlay entry.
    [[nodiscard]] OverlayEntry* active_overlay() noexcept;
    [[nodiscard]] const OverlayEntry* active_overlay() const noexcept;
    // Returns the topmost open modal overlay that should block base content input.
    [[nodiscard]] OverlayEntry* active_modal_overlay() noexcept;
    [[nodiscard]] const OverlayEntry* active_modal_overlay() const noexcept;
    [[nodiscard]] OverlayEntry* find_overlay(UiElement& element) noexcept;
    [[nodiscard]] const OverlayEntry* find_overlay(const UiElement& element) const noexcept;

    void set_overlay_open_state(UiElement& element,bool open);
    // Treats an overlay element as a focus scope when it can directly own focus.
    [[nodiscard]] UiFocusScope* overlay_focus_scope(OverlayEntry& entry) noexcept;
    [[nodiscard]] const UiFocusScope* overlay_focus_scope(const OverlayEntry& entry) const noexcept;
    // Captures the current focus scope before opening a modal overlay.
    void remember_overlay_restore_focus(OverlayEntry& entry) noexcept;
    // Restores focus after closing an overlay when the previous scope is still valid.
    [[nodiscard]] bool restore_focus_after_overlay_close(OverlayEntry& entry);
    // Gives focus to an overlay or its delegated scope after open.
    [[nodiscard]] bool focus_overlay(OverlayEntry& entry);
    // Routes an event directly into the active overlay before window content sees it.
    [[nodiscard]] bool dispatch_to_overlay(OverlayEntry& entry,const UiInputEvent& event);
    // Keeps overlay visibility flags aligned with open/closed window state.
    void sync_overlay_visibility(OverlayEntry& entry) noexcept;
    void sync_overlay_visibility_all() noexcept;
    // Reapplies placement rules for every registered open overlay.
    void apply_overlay_placements() noexcept;
    // Positions one overlay relative to the current window rect and placement mode.
    void apply_overlay_placement(OverlayEntry& entry) noexcept;
    // Applies overlay dismissal policy for cancel and outside-click events.
    [[nodiscard]] bool should_close_overlay_from_event(const OverlayEntry& entry,const UiInputEvent& event) const noexcept;
    // Checks whether a pointer event landed inside the overlay's current bounds.
    [[nodiscard]] bool contains_overlay_point(const OverlayEntry& entry,int mouse_x,int mouse_y) const noexcept;
    [[nodiscard]] UiTransientPopup* active_transient_popup() noexcept;
    [[nodiscard]] const UiTransientPopup* active_transient_popup() const noexcept;
    [[nodiscard]] bool dispatch_to_transient_popup(UiTransientPopup& popup,const UiInputEvent& event);
    void submit_active_transient_popup_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const;
    void submit_tooltip_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const;
    // Routes gamepad wheel events through logically focused nested scroll containers, deepest first.
    [[nodiscard]] bool dispatch_gamepad_scroll_to_focused_containers(UiElement& root,const UiInputEvent& event);
    [[nodiscard]] bool dispatch_passive_scroll_input(UiElement& root,const UiInputEvent& event);
    void promote_scroll_target_at(UiElement& root,int mouse_x,int mouse_y) noexcept;
    [[nodiscard]] UiScrollContainer* resolve_passive_scroll_target(UiElement& root) noexcept;
    // Verifies that an element is still an owned child before window bookkeeping uses it.
    [[nodiscard]] bool is_live_child_element(const UiElement* element) const noexcept;
    // Finds the registered focus scope under a pointer position for hover focus.
    [[nodiscard]] UiFocusScope* find_registered_scope_at(int mouse_x,int mouse_y) const;
    // Returns the neighbor scope to navigate to for a directional action.
    [[nodiscard]] UiFocusScope* find_neighbor(const UiFocusScope& scope,UiAction action) const;
    [[nodiscard]] bool is_registered_scope(const UiFocusScope& scope) const noexcept;
    // Performs scope focus assignment without re-entering public validation paths.
    [[nodiscard]] bool set_focused_scope_internal(UiFocusScope* scope) noexcept;
    // Routes an input event into one registered scope when it is still usable.
    [[nodiscard]] bool dispatch_to_scope(UiFocusScope* scope,const UiInputEvent& event) const;
    [[nodiscard]] static bool is_scope_usable(const UiFocusScope* scope) noexcept;
    [[nodiscard]] static bool uses_pointer_focus_policy(elysia::input::InputDevice device) noexcept;

private:
    std::vector<ScopeEntry> _scope_entries;
    std::vector<OverlayEntry> _overlay_entries;
    std::vector<TransientPopupEntry> _transient_popups;
    std::vector<class UiTooltip*> _tooltips;
    UiTransientPopup* _active_transient_popup = nullptr;
    UiFocusScope* _focused_scope = nullptr;
    UiFocusScope* _last_focused_scope = nullptr;
    UiScrollContainer* _gamepad_scroll_target = nullptr;
    UiStyleState<UiWindowStyle> _style_state;
    bool _hover_focus_enabled = true;
    bool _transient_popup_pointer_active = false;
    elysia::input::InputDevice _focus_input_device = elysia::input::InputDevice::Unknown;
    UiWindowCancelCallback _on_cancel;
};
}
