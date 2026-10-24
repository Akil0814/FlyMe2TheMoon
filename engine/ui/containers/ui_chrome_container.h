#pragma once

#include "../focus/ui_control_focus_scope_host.h"
#include "../focus/ui_delegated_focus_mixin.h"
#include "../style/ui_style.h"
#include "../style/ui_visual_styles.h"
#include "ui_list_container.h"

#include <memory>
#include <vector>

namespace elysia::ui
{
enum class UiChromeActionInsertPosition
{
    Front,
    Back
};

class UiChromeContainer : public UiControlFocusScopeHost, private UiDelegatedFocusMixin
{
private:
    class SlotHost : public UiChildHost
    {
    public:
        explicit SlotHost(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
        // Chooses whether this slot stretches its child to fill the slot bounds.
        void set_fill_children(bool fill_children) noexcept;

    protected:
        // Rebuilds the slot around either stretched or intrinsic child layout.
        void rebuild_layout() override;

    private:
        bool _fill_children = false;
    };

public:
    explicit UiChromeContainer(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiChromeContainer(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiChromeContainer(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;
    ~UiChromeContainer() override = default;

    void reset() noexcept override;
    void update(double delta) override;
    void on_ui_input_frame(const UiInputFrame& input) override;
    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;
    [[nodiscard]] elysia::core::Vector2 content_extent() const noexcept override;
    void set_scope_focused(bool focused) noexcept override;
    [[nodiscard]] bool is_scope_focused() const noexcept override;
    [[nodiscard]] bool has_focusable_target() const noexcept override;
    bool focus_first_available() override;
    // Enters the delegated body scope and focuses its first available control.
    bool focus_body_first_available();
    [[nodiscard]] UiControl* focused_target() const noexcept override;
    [[nodiscard]] bool can_navigate(UiAction action) const noexcept override;

    // Adds an action item inside the header's left action region at the requested position.
    UiElement* add_left_action(std::unique_ptr<UiElement> action,UiChromeActionInsertPosition position = UiChromeActionInsertPosition::Back);
    void clear_left_actions();
    // Adds a title-region child without exposing the internal slot host directly.
    UiElement* add_title_child(std::unique_ptr<UiElement> child,UiLayoutChildOptions options = {});
    void clear_title_children();
    // Adds an action item inside the header's right action region at the requested position.
    UiElement* add_right_action(std::unique_ptr<UiElement> action,UiChromeActionInsertPosition position = UiChromeActionInsertPosition::Back);
    void clear_right_actions();

    // Replaces the body payload while keeping header slots intact.
    UiElement* set_body(std::unique_ptr<UiElement> body);
    [[nodiscard]] const UiElement* body_content() const noexcept;
    void clear_body();

    void set_header_visible(bool visible) noexcept;
    [[nodiscard]] bool header_visible() const noexcept;
    // Sets the minimum header height; intrinsic header content can expand it.
    void set_header_height(float height) noexcept;
    [[nodiscard]] float header_height() const noexcept;
    void set_header_padding(const UiLayoutPadding& padding) noexcept;
    [[nodiscard]] const UiLayoutPadding& header_padding() const noexcept;
    void set_body_padding(const UiLayoutPadding& padding) noexcept;
    [[nodiscard]] const UiLayoutPadding& body_padding() const noexcept;

    void set_base_style(const UiChromeContainerStyle& style) noexcept;
    void set_style_overrides(const UiChromeContainerStyleOverrides& overrides) noexcept;
    [[nodiscard]] const UiChromeContainerStyle& style() const noexcept;
    [[nodiscard]] const UiChromeContainerStyleOverrides& style_overrides() const noexcept;
    [[nodiscard]] bool has_style_overrides() const noexcept;
    void clear_style_overrides() noexcept;

protected:
    // Rebuilds header and body slot geometry from the current chrome settings.
    void rebuild_layout() override;
    // Rebuilds focus navigation across header controls and delegated body scope.
    void rebuild_focus_registry() override;

private:
    // Creates the owned header/body slot hosts used by the chrome container.
    void create_internal_hosts();
    // Clears raw pointers after the internal hosts are removed during reset.
    void clear_internal_host_pointers() noexcept;
    // Collects controls from a subtree, optionally crossing nested scope boundaries.
    void collect_controls_from(const UiElement& element,std::vector<UiControl*>& out_controls,bool recurse_into_nested_scopes = true) const;
    // Returns the body payload as a delegated focus scope when supported.
    [[nodiscard]] UiFocusScope* delegated_body_scope() noexcept;
    [[nodiscard]] const UiFocusScope* delegated_body_scope() const noexcept;
    [[nodiscard]] UiElement* body_content_mutable() noexcept;
    [[nodiscard]] static UiElement* add_action(UiListContainer* actions,std::unique_ptr<UiElement> child,UiChromeActionInsertPosition position);
    static void clear_children(UiChildHost* host) noexcept;
    // Finds the first focusable control in the header chrome.
    [[nodiscard]] UiControl* first_header_focusable() const noexcept;
    [[nodiscard]] bool header_has_focusable_target() const noexcept;
    // Transfers focus from the header into the delegated body scope.
    [[nodiscard]] bool enter_body_scope(bool focus_first_available);
    // Returns focus from the body scope back to the header chrome.
    [[nodiscard]] bool leave_body_scope();
    // Mirrors the container focus state into the delegated body scope.
    void sync_body_scope_focus() noexcept;
    // Detects pointer and action events that should stay in the header region.
    [[nodiscard]] bool event_targets_header(const UiInputEvent& event) const noexcept;
    // Detects events that should be delegated into the body focus scope.
    [[nodiscard]] bool event_targets_body_scope(const UiInputEvent& event) const noexcept;
    // Resolves the minimum header size shared by desired measurement and placement.
    [[nodiscard]] elysia::core::Vector2 desired_header_extent() const noexcept;
    // Returns the current header draw and hit-test rect.
    [[nodiscard]] elysia::core::Rect header_rect() const noexcept;
    // Returns the current body draw and hit-test rect.
    [[nodiscard]] elysia::core::Rect body_rect() const noexcept;

private:
    UiListContainer* _left_actions = nullptr;
    SlotHost* _title_slot = nullptr;
    UiListContainer* _right_actions = nullptr;
    SlotHost* _body = nullptr;
    UiStyleState<UiChromeContainerStyle> _style_state;
    UiLayoutPadding _header_padding{};
    UiLayoutPadding _body_padding{};
    float _header_height = 48.0f;
    bool _header_visible = true;
    bool _scope_focused = false;
    bool _body_scope_active = false;
};
}
