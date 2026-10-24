#pragma once

#include "../focus/ui_control_focus_scope_host.h"
#include "../focus/ui_delegated_focus_mixin.h"
#include "../style/ui_style.h"
#include "../style/ui_visual_roles.h"
#include "../style/ui_visual_styles.h"
#include "../text/ui_text_content.h"
#include "../window/ui_overlay.h"

namespace elysia::ui
{
class UiWindow;
class UiChromeContainer;
class UiPanel;
class UiScrollContainer;
class UiButton;
class UiLabel;
class UiTextBlock;

// Reading-oriented overlay composite with a scrollable body and one dismiss action.
// Reading-oriented composite whose owned controls are styled by the registered container tree.
class UiDialog : public UiControlFocusScopeHost, public UiOverlayWindowClient, private UiDelegatedFocusMixin
{
public:
    explicit UiDialog(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiDialog(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiDialog(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;
    ~UiDialog() override;

    void reset() noexcept override;
    void update(double delta) override;
    void on_ui_input_frame(const UiInputFrame& input) override;
    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;
    [[nodiscard]] elysia::core::Vector2 content_extent() const noexcept override;

    void set_title_content(UiTextContent title_content);
    [[nodiscard]] const UiTextContent& title_content() const noexcept;

    void set_body_content(UiTextContent body_content);
    [[nodiscard]] const UiTextContent& body_content() const noexcept;

    void set_action_content(UiTextContent action_content);
    [[nodiscard]] const UiTextContent& action_content() const noexcept;

    void set_body_scroll_enabled(bool enabled) noexcept;
    [[nodiscard]] bool body_scroll_enabled() const noexcept;
    void set_header_visible(bool visible) noexcept;
    [[nodiscard]] bool header_visible() const noexcept;

    void set_base_style(const UiDialogStyle& style) noexcept;
    void set_style_overrides(const UiDialogStyleOverrides& overrides) noexcept;
    [[nodiscard]] const UiDialogStyle& style() const noexcept;
    [[nodiscard]] const UiDialogStyleOverrides& style_overrides() const noexcept;
    [[nodiscard]] bool has_style_overrides() const noexcept;
    void clear_style_overrides() noexcept;

    void set_visual_role(UiDialogVisualRole role) noexcept;
    [[nodiscard]] UiDialogVisualRole visual_role() const noexcept;

    // The parent UiWindow must already own this dialog before overlay registration.
    [[nodiscard]] bool register_with_window(UiWindow& window,UiOverlayOptions options = {});
    void unregister_from_window() noexcept;
    void on_window_detached(UiWindow& window) noexcept override;
    void open();
    void close();

protected:
    void rebuild_layout() override;
    void rebuild_focus_registry() override;

private:
    // Keeps the reading viewport logically focused for gamepad scrolling without moving Close focus.
    void sync_body_scroll_gamepad_focus() noexcept;
    // Builds the stable child tree once; content changes update existing controls in place.
    void create_internal_children();
    void sync_sources_to_children();
    void sync_style_to_children();
    [[nodiscard]] static bool is_default_overlay_options(const UiOverlayOptions& options) noexcept;

private:
    UiChromeContainer* _chrome = nullptr;
    UiLabel* _title_label = nullptr;
    UiPanel* _body_panel = nullptr;
    UiScrollContainer* _body_scroll = nullptr;
    UiTextBlock* _body_text = nullptr;
    UiButton* _close_button = nullptr;
    UiWindow* _registered_window = nullptr;
    UiTextContent _title_content{};
    UiTextContent _body_content{};
    UiTextContent _action_content = ui_text_key("menu_scene.exit_confirm.cancel");
    UiStyleState<UiDialogStyle> _style_state;
    UiDialogVisualRole _visual_role = UiDialogVisualRole::Default;
    bool _body_scroll_enabled = true;
};
}
