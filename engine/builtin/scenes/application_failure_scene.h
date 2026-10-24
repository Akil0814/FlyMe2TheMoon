#pragma once

#include "application_failure_scene_payload.h"
#include "application_failure_dialog.h"
#include "../../scene/scene.h"

#include <string>

namespace elysia::ui
{
class UiButton;
class UiWindow;
}

namespace elysia::builtin
{
class ApplicationFailureScene final : public elysia::scene::Scene
{
    friend class ApplicationFailureSceneTestAccess;

public:
    ApplicationFailureScene() = default;
    ~ApplicationFailureScene() override = default;

protected:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void on_reset() override;
    void on_after_update(double delta) override;

private:
    void apply_payload(const ApplicationFailureScenePayload& payload);
    void build_ui();
    void open_dialog();
    void sync_dialog_state();
    void confirm_exit();
    void destroy_ui() noexcept;
    [[nodiscard]] ApplicationFailureDialogConfig make_dialog_config() const;
    [[nodiscard]] static elysia::core::Vector2 dialog_size(
        float logical_width,float logical_height) noexcept;

private:
    ApplicationFailurePresentation _presentation =
        ApplicationFailurePresentation::RuntimeFatal;
    ApplicationFailureReason _reason = ApplicationFailureReason::RuntimeFatal;
    std::string _error_code;
    std::string _category;
    elysia::core::FailureDiagnostic _diagnostic;
    elysia::ui::UiWindow* _window = nullptr;
    ApplicationFailureDialog* _dialog = nullptr;
    elysia::ui::UiButton* _reopen_button = nullptr;
};
}
