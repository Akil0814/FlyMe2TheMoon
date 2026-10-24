#pragma once

#include "../../ui/focus/ui_control_focus_scope_host.h"
#include "../../ui/focus/ui_delegated_focus_mixin.h"
#include "../../ui/text/ui_text_content.h"
#include "../../ui/window/ui_overlay.h"

#include <functional>
#include <string>

namespace elysia::ui
{
class UiButton;
class UiChromeContainer;
class UiLabel;
class UiListContainer;
class UiPanel;
class UiScrollContainer;
class UiTextBlock;
class UiWindow;
}

namespace elysia::builtin
{
struct ApplicationFailureDialogConfig
{
    elysia::ui::UiTextContent title;
    elysia::ui::UiTextContent body;
    elysia::ui::UiTextContent copy;
    elysia::ui::UiTextContent copied;
    elysia::ui::UiTextContent copy_failed;
    elysia::ui::UiTextContent exit;
    elysia::ui::UiTextContent close;
    std::string copy_report;
};

class ApplicationFailureDialog final : public elysia::ui::UiControlFocusScopeHost,
    public elysia::ui::UiOverlayWindowClient,
    private elysia::ui::UiDelegatedFocusMixin
{
    friend class ApplicationFailureDialogTestAccess;

public:
    explicit ApplicationFailureDialog(
        const elysia::core::Rect& rect = { 0,0,640,480 },int order = 0) noexcept;
    ~ApplicationFailureDialog() override;

    void reset() noexcept override;
    void update(double delta) override;
    void on_ui_input_frame(const elysia::ui::UiInputFrame& input) override;
    bool on_ui_input_event(const elysia::ui::UiInputEvent& event) override;
    void submit_ui_render_commands(
        std::vector<elysia::core::UiRenderCommand>& out_commands) const override;
    [[nodiscard]] elysia::core::Vector2 content_extent() const noexcept override;
    bool focus_first_available() override;

    void set_config(ApplicationFailureDialogConfig config);
    [[nodiscard]] const ApplicationFailureDialogConfig& config() const noexcept;
    void set_on_exit(std::function<void()> callback);

    [[nodiscard]] bool register_with_window(
        elysia::ui::UiWindow& window,elysia::ui::UiOverlayOptions options = {});
    void unregister_from_window() noexcept;
    void on_window_detached(elysia::ui::UiWindow& window) noexcept override;
    void open();
    void close();

protected:
    void rebuild_layout() override;
    void rebuild_focus_registry() override;

private:
    void create_internal_children();
    void sync_config();
    void sync_delegated_focus() noexcept;
    void copy_information();
    void exit_application();
    [[nodiscard]] static bool is_default_overlay_options(
        const elysia::ui::UiOverlayOptions& options) noexcept;

private:
    elysia::ui::UiChromeContainer* _chrome = nullptr;
    elysia::ui::UiLabel* _title = nullptr;
    elysia::ui::UiButton* _close = nullptr;
    elysia::ui::UiPanel* _body_panel = nullptr;
    elysia::ui::UiScrollContainer* _scroll = nullptr;
    elysia::ui::UiTextBlock* _body = nullptr;
    elysia::ui::UiListContainer* _actions = nullptr;
    elysia::ui::UiButton* _copy = nullptr;
    elysia::ui::UiButton* _exit = nullptr;
    elysia::ui::UiWindow* _window = nullptr;
    ApplicationFailureDialogConfig _config;
    std::function<void()> _on_exit;
};
}
