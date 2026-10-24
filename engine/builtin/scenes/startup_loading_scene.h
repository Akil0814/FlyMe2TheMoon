#pragma once

#include "startup_loading_flow.h"
#include "startup_loading_scene_payload.h"
#include "../../scene/scene.h"
#include "../../loading/game_content_loader.h"

#include <string_view>
#include <source_location>

namespace elysia::ui
{
class UiBar;
class UiFadeImage;
class UiBlinkLabel;
struct UiTextContent;
}

namespace elysia::typography
{
class FontResolver;
}

namespace elysia::builtin
{
class StartupLoadingScene final : public elysia::scene::Scene
{
    friend class StartupLoadingSceneTestAccess;

public:
    StartupLoadingScene()
    {
        // The confirmation prompt deliberately accepts every button, including
        // mouse buttons.  Shortcuts exclude pointer input by default, so this
        // scene opts in explicitly.
        set_shortcut_devices(
            elysia::input::InputCapture::Keyboard
            | elysia::input::InputCapture::Pointer
            | elysia::input::InputCapture::Gamepad);
    }
    ~StartupLoadingScene() override = default;

protected:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void on_reset() override;
    void on_after_update(double delta) override;
    void on_shortcuts(const elysia::input::RawInputFrame &input,
                      const std::vector<elysia::input::RawInputEvent> &events) override;

private:
    [[nodiscard]] static elysia::ui::UiTextContent
        make_start_prompt_content();
    bool create_presentation();
    void create_loading_ui();
    void begin_logo_sequence();
    void on_engine_logo_finished();
    void on_project_logo_finished();
    void mark_intro_finished();
    void mark_loading_finished();
    bool activate_project_fonts(
        elysia::typography::FontResolver* font_resolver);
    void handle_logo_action(StartupLogoAction action);
    void handle_completion_action(StartupLoadingAction action);
    void transition_to_success();
    void handle_failure(std::string_view message,
        std::source_location origin = std::source_location::current());
    void handle_failure(const elysia::loading::ContentLoadFailure& failure);
    void destroy_ui();
    void clear_state() noexcept;

private:
    StartupLoadingScenePayload _startup_payload;
    elysia::loading::GameContentLoader _content_loader;

    elysia::ui::UiBar* _loading_bar = nullptr;
    elysia::ui::UiFadeImage* _engine_logo = nullptr;
    elysia::ui::UiFadeImage* _project_logo = nullptr;
    elysia::ui::UiBlinkLabel* _start_prompt = nullptr;

    StartupLogoSequence _logo_sequence;
    StartupLoadingCompletion _completion;
};
}
