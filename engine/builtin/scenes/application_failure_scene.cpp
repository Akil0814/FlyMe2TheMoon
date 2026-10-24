#include "application_failure_scene.h"
#include "application_failure_presentation.h"

#include "../../tools/termination_manager.h"
#include "../../typography/font_resolver.h"
#include "../../localization/localization_service.h"
#include "../../tools/logger.h"
#include "../../ui/widgets/ui_button.h"
#include "../../ui/window/ui_window.h"
#include "../../scene/runtime/scene_runtime_context.h"

#include <algorithm>
#include <stdexcept>
#include <sstream>

namespace elysia::builtin
{
using elysia::scene::Scene;
using elysia::scene::ScenePayload;
using elysia::scene::try_scene_payload;

namespace
{
constexpr const char* fallback_category = "application";
constexpr const char* startup_fallback_diagnostic_message ="Startup resource loading failed.";
constexpr const char* runtime_fallback_diagnostic_message ="A fatal application failure occurred.";

bool valid_presentation(ApplicationFailurePresentation presentation) noexcept
{
    return presentation == ApplicationFailurePresentation::StartupLoading
        || presentation == ApplicationFailurePresentation::RuntimeFatal;
}

const char* fallback_diagnostic_message(ApplicationFailurePresentation presentation) noexcept
{
    return presentation == ApplicationFailurePresentation::StartupLoading
        ? startup_fallback_diagnostic_message
        : runtime_fallback_diagnostic_message;
}
}

void ApplicationFailureScene::on_enter(const ScenePayload& payload)
{
    const ApplicationFailureScenePayload* failure_payload =try_scene_payload<ApplicationFailureScenePayload>(payload);

    if (!failure_payload)
        throw std::logic_error(
            "ApplicationFailureScene requires ApplicationFailureScenePayload.");

    if (!valid_presentation(failure_payload->presentation))
        throw std::logic_error(
            "ApplicationFailureScene received an invalid presentation.");

    apply_payload(*failure_payload);
    _paused = false;

    if (elysia::typography::FontResolver* font_resolver =runtime_context().font_resolver())
    {
        font_resolver->deactivate_project_fonts();
    }

    if (!_window || _window->is_destroyed())
        build_ui();

    _dialog->set_config(make_dialog_config());
    _window->set_visible(true);
    _window->set_active(true);
    open_dialog();
}

void ApplicationFailureScene::on_exit()
{
    _paused = false;
    if (_dialog)
        _dialog->close();
    if (_window && !_window->is_destroyed())
    {
        _window->set_active(false);
        _window->set_visible(false);
    }
}

void ApplicationFailureScene::on_reset()
{
    _paused = false;
    destroy_ui();
    _presentation = ApplicationFailurePresentation::RuntimeFatal;
    _reason = ApplicationFailureReason::RuntimeFatal;
    _error_code.clear();
    _category.clear();
    _diagnostic = {};
}

void ApplicationFailureScene::on_after_update(double delta)
{
    (void)delta;
    sync_dialog_state();
}

void ApplicationFailureScene::apply_payload(
    const ApplicationFailureScenePayload& payload)
{
    _presentation = payload.presentation;
    _reason = payload.reason;
    _error_code = payload.error_code.empty()
        ? "APPLICATION-UNKNOWN" : payload.error_code;
    _category = payload.category.empty()
        ? fallback_category
        : payload.category;
    _diagnostic = payload.diagnostic;
    if (_diagnostic.message.empty())
        _diagnostic.message = fallback_diagnostic_message(_presentation);
}

void ApplicationFailureScene::build_ui()
{
    const float logical_width = static_cast<float>(
        std::max(1,runtime_context().logical_width()));
    const float logical_height = static_cast<float>(
        std::max(1,runtime_context().logical_height()));

    _window = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{ 0,0,logical_width,logical_height },
        10);
    if (!_window)
        throw std::runtime_error(
            "ApplicationFailureScene could not create its UiWindow.");
    _window->set_on_cancel([this]() { open_dialog(); });

    const float reopen_width =
        std::min(280.0f,std::max(1.0f,logical_width - 32.0f));
    const float reopen_height = std::min(56.0f,std::max(1.0f,logical_height));
    _reopen_button = _window->create_child<elysia::ui::UiButton>(
        elysia::ui::UiLayoutChildOptions{
            ._anchor = elysia::ui::UiLayoutAnchor::BottomCenter,
            ._margin = elysia::ui::UiLayoutMargin{
                .bottom = 24.0f
            },
            ._size_override = elysia::core::Vector2{
                reopen_width,reopen_height
            },
            ._use_size_override = true
        },
        elysia::core::Rect{
            (logical_width - reopen_width) * 0.5f,
            std::max(0.0f,logical_height - reopen_height - 24.0f),
            reopen_width,
            reopen_height
        },
        5);
    if (!_reopen_button)
        throw std::runtime_error(
            "ApplicationFailureScene could not create its reopen button.");
    _reopen_button->set_text_content(
        elysia::ui::ui_text_key(
            "engine.application_failure.runtime.reopen"));
    _reopen_button->set_typography_role(
        elysia::typography::UiTypographyRole::ButtonCompact);
    _reopen_button->set_on_click([this]() { open_dialog(); });

    const elysia::core::Vector2 failure_dialog_size =
        dialog_size(logical_width,logical_height);
    _dialog = _window->create_child<ApplicationFailureDialog>(
        elysia::core::Rect{ 0,0,failure_dialog_size.x,failure_dialog_size.y },
        10);
    if (!_dialog)
    {
        throw std::runtime_error(
            "ApplicationFailureScene could not create its confirmation dialog.");
    }

    _dialog->set_config(make_dialog_config());
    _dialog->set_on_exit([this]() { confirm_exit(); });
    if (!_dialog->register_with_window(*_window))
    {
        throw std::runtime_error(
            "ApplicationFailureScene could not register its confirmation dialog.");
    }
    sync_dialog_state();
}

void ApplicationFailureScene::open_dialog()
{
    if (_dialog)
        _dialog->open();
    sync_dialog_state();
}

void ApplicationFailureScene::sync_dialog_state()
{
    if (!_window || !_dialog || !_reopen_button)
        return;

    const bool dialog_open = _window->is_overlay_open(*_dialog);
    _reopen_button->set_visible(!dialog_open);
    _reopen_button->set_active(!dialog_open);
    _reopen_button->set_enabled(!dialog_open);
}

void ApplicationFailureScene::confirm_exit()
{
    const std::string category = _category.empty() ? fallback_category : _category;
    const std::string report = elysia::core::format_failure_diagnostic(
        _diagnostic,_error_code,category);
    elysia::tools::Logger::instance()->error(category,report,_diagnostic.origin);
    elysia::tools::TerminationManager::instance()->request_termination(
        elysia::tools::TerminationReason::FatalRuntimeFailure,
        category,report,_diagnostic.origin);
}

void ApplicationFailureScene::destroy_ui() noexcept
{
    if (_dialog)
        _dialog->unregister_from_window();
    if (_window)
        _window->destroy();
    _dialog = nullptr;
    _reopen_button = nullptr;
    _window = nullptr;
}

ApplicationFailureDialogConfig ApplicationFailureScene::make_dialog_config() const
{
    const bool startup = _presentation == ApplicationFailurePresentation::StartupLoading;
#ifndef NDEBUG
    constexpr bool include_diagnostics = true;
#else
    constexpr bool include_diagnostics = false;
#endif
    std::string log_file_name;
    if (const auto active_path = elysia::tools::Logger::instance()->active_file_path())
        log_file_name = active_path->filename().string();
    const ApplicationFailureScenePayload payload{
        .presentation = _presentation,
        .reason = _reason,
        .error_code = _error_code,
        .category = _category,
        .diagnostic = _diagnostic
    };
    const auto model = build_application_failure_presentation(
        payload,std::move(log_file_name),include_diagnostics);
    const auto tr = [](std::string_view key)
    {
        return std::string(ELYSIA_LOCALIZATION->tr(key));
    };
    std::ostringstream body;
    body << tr(model.summary_key) << "\n\n"
        << tr("engine.application_failure.labels.error_code") << ": "
        << model.error_code << '\n'
        << tr("engine.application_failure.labels.log") << ": "
        << (model.log_available
            ? model.log_file_name
            : tr("engine.application_failure.log_unavailable"));
    if (include_diagnostics)
    {
        body << "\n\n" << tr("engine.application_failure.labels.details")
            << ":\n" << model.diagnostic_details;
    }
    else
    {
        body << "\n\n" << tr("engine.application_failure.details_in_log");
    }

    return ApplicationFailureDialogConfig{
        .title = elysia::ui::ui_text_key(startup
            ? "engine.application_failure.startup.title"
            : "engine.application_failure.runtime.title"),
        .body = elysia::ui::ui_raw_text(body.str()),
        .copy = elysia::ui::ui_text_key("engine.application_failure.copy"),
        .copied = elysia::ui::ui_text_key("engine.application_failure.copied"),
        .copy_failed = elysia::ui::ui_text_key("engine.application_failure.copy_failed"),
        .exit = elysia::ui::ui_text_key(startup
            ? "engine.application_failure.startup.exit"
            : "engine.application_failure.runtime.exit"),
        .close = elysia::ui::ui_text_key("engine.common.close_x"),
        .copy_report = model.copy_report
    };
}

elysia::core::Vector2 ApplicationFailureScene::dialog_size(
    float logical_width,float logical_height) noexcept
{
    const float available_width = std::max(1.0f,logical_width - 32.0f);
    const float available_height = std::max(1.0f,logical_height - 32.0f);
    const float width = std::min(available_width,
        std::clamp(logical_width * 0.72f,560.0f,880.0f));
    const float height = std::min(available_height,
        std::clamp(logical_height * 0.68f,400.0f,600.0f));
    return { width,height };
}
}
