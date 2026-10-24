#include "application_failure_dialog.h"

#include "../../tools/logger.h"
#include "../../ui/containers/ui_chrome_container.h"
#include "../../ui/containers/ui_list_container.h"
#include "../../ui/containers/ui_panel.h"
#include "../../ui/containers/ui_scroll_container.h"
#include "../../ui/widgets/label/ui_label.h"
#include "../../ui/widgets/text/ui_text_block.h"
#include "../../ui/widgets/ui_button.h"
#include "../../ui/window/ui_window.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <memory>
#include <utility>

namespace elysia::builtin
{
namespace
{
constexpr float kHeaderHeight = 48.0f;
constexpr float kActionHeight = 56.0f;
constexpr float kBodyActionSpacing = 20.0f;
constexpr float kActionSpacing = 20.0f;

elysia::ui::UiLayoutChildOptions anchored(
    elysia::ui::UiLayoutAnchor anchor,const elysia::core::Vector2& size)
{
    return elysia::ui::UiLayoutChildOptions{
        ._anchor = anchor,
        ._margin = {},
        ._cross_align = elysia::ui::UiLayoutAlign::Start,
        ._size_override = size,
        ._use_custom_cross_align = false,
        ._fill_cross_axis = false,
        ._use_size_override = true
    };
}
}

ApplicationFailureDialog::ApplicationFailureDialog(
    const elysia::core::Rect& rect,int order) noexcept
    : UiControlFocusScopeHost(rect,order)
{
    reset();
}

ApplicationFailureDialog::~ApplicationFailureDialog()
{
    unregister_from_window();
}

void ApplicationFailureDialog::reset() noexcept
{
    unregister_from_window();
    UiControlFocusScopeHost::reset();
    reset_delegated_focus_state();
    _chrome = nullptr;
    _title = nullptr;
    _close = nullptr;
    _body_panel = nullptr;
    _scroll = nullptr;
    _body = nullptr;
    _actions = nullptr;
    _copy = nullptr;
    _exit = nullptr;
    _window = nullptr;
    _config = {};
    _on_exit = {};
    create_internal_children();
    sync_config();
}

void ApplicationFailureDialog::update(double delta)
{
    UiControlFocusScopeHost::update(delta);
    sync_delegated_focus();
    if (_scroll)
        _scroll->set_scope_focused(is_scope_focused());
}

void ApplicationFailureDialog::on_ui_input_frame(
    const elysia::ui::UiInputFrame& input)
{
    UiControlFocusScopeHost::on_ui_input_frame(input);
    sync_delegated_focus();
}

bool ApplicationFailureDialog::on_ui_input_event(
    const elysia::ui::UiInputEvent& event)
{
    const bool navigation = event.type == elysia::ui::UiInputEventType::ActionPressed
        && elysia::ui::is_navigation_action(event.action);
    const bool confirm = event.action == elysia::ui::UiAction::Confirm
        && (event.type == elysia::ui::UiInputEventType::ActionPressed
            || event.type == elysia::ui::UiInputEventType::ActionReleased);
    const bool handled = _chrome && (navigation || confirm)
        ? _chrome->on_ui_input_event(event)
        : UiControlFocusScopeHost::on_ui_input_event(event);
    sync_host_delegated_focus_target(*this);
    sync_delegated_focus();
    return handled;
}

void ApplicationFailureDialog::submit_ui_render_commands(
    std::vector<elysia::core::UiRenderCommand>& commands) const
{
    if (!is_visible())
        return;
    auto* self = const_cast<ApplicationFailureDialog*>(this);
    self->cleanup_destroyed_children();
    self->update_layout_if_dirty();
    self->refresh_focus_registry();
    self->ensure_valid_focus();
    self->sync_host_delegated_focus_target(*self);
    self->sync_delegated_focus();
    self->apply_focus_state();
    UiControlFocusScopeHost::submit_ui_render_commands(commands);
}

elysia::core::Vector2 ApplicationFailureDialog::content_extent() const noexcept
{
    const auto explicit_size = size();
    if (!_chrome)
        return explicit_size;
    const auto extent = _chrome->content_extent();
    return { std::max(explicit_size.x,extent.x),std::max(explicit_size.y,extent.y) };
}

bool ApplicationFailureDialog::focus_first_available()
{
    cleanup_destroyed_children();
    update_layout_if_dirty();
    refresh_focus_registry();
    const bool focused = UiControlFocusScopeHost::focus_first_available();
    if (_chrome && _chrome->focus_body_first_available())
    {
        sync_host_delegated_focus_target(*this);
        sync_delegated_focus();
        apply_focus_state();
        return true;
    }
    return focused;
}

void ApplicationFailureDialog::set_config(ApplicationFailureDialogConfig config)
{
    _config = std::move(config);
    sync_config();
    mark_layout_dirty();
}

const ApplicationFailureDialogConfig& ApplicationFailureDialog::config() const noexcept
{
    return _config;
}

void ApplicationFailureDialog::set_on_exit(std::function<void()> callback)
{
    _on_exit = std::move(callback);
}

bool ApplicationFailureDialog::register_with_window(
    elysia::ui::UiWindow& window,elysia::ui::UiOverlayOptions options)
{
    if (is_destroyed() || layout_parent() != &window)
        return false;
    if (_window && _window != &window)
        _window->unregister_overlay(*this);
    if (is_default_overlay_options(options))
    {
        options.open = false;
        options.modal = true;
        options.close_on_cancel = true;
        options.close_on_outside_click = false;
        options.placement = elysia::ui::UiOverlayPlacement::Center;
        options.transition = elysia::ui::UiOverlayTransition::None;
        options.order = 1000;
    }
    options.fallback_size = size();
    if (!window.register_overlay(*this,options))
        return false;
    _window = &window;
    return true;
}

void ApplicationFailureDialog::unregister_from_window() noexcept
{
    auto* window = _window;
    _window = nullptr;
    if (window)
        window->unregister_overlay(*this);
}

void ApplicationFailureDialog::on_window_detached(elysia::ui::UiWindow& window) noexcept
{
    if (_window == &window)
        _window = nullptr;
}

void ApplicationFailureDialog::open()
{
    if (_window && !_window->is_destroyed())
        _window->open_overlay(*this);
}

void ApplicationFailureDialog::close()
{
    if (_window && !_window->is_destroyed())
        _window->close_overlay(*this);
}

void ApplicationFailureDialog::rebuild_layout()
{
    if (!_chrome || !_body_panel || !_scroll || !_body || !_actions)
        return;
    _chrome->set_screen_rect(content_rect());
    _chrome->update_layout_if_dirty();
    const auto panel = _body_panel->screen_rect();
    const float body_height = std::max(0.0f,panel.height() - kActionHeight - kBodyActionSpacing);
    const float action_width = std::max(1.0f,(panel.width() - kActionSpacing) * 0.5f);
    if (_copy) _copy->set_size({ action_width,kActionHeight });
    if (_exit) _exit->set_size({ action_width,kActionHeight });
    _body_panel->set_child_layout_options(0,anchored(
        elysia::ui::UiLayoutAnchor::TopLeft,{ panel.width(),body_height }));
    _body_panel->set_child_layout_options(1,anchored(
        elysia::ui::UiLayoutAnchor::BottomCenter,{ panel.width(),kActionHeight }));
    _body_panel->update_layout_if_dirty();
    _scroll->update_layout_if_dirty();
    _body->set_size({ std::max(0.0f,_scroll->screen_rect().width()),0.0f });
    _scroll->mark_layout_dirty();
    _scroll->update_layout_if_dirty();
    _actions->update_layout_if_dirty();
}

void ApplicationFailureDialog::rebuild_focus_registry()
{
    std::vector<DelegatedRegionEntry> regions;
    for (auto* region : delegated_focus_regions(*this))
        regions.push_back({ region,nullptr,nullptr,nullptr,nullptr });
    std::vector<FocusEntry> entries;
    build_delegated_focus_entries(regions,entries);
    set_focus_entries(std::move(entries));
}

void ApplicationFailureDialog::create_internal_children()
{
    auto chrome = std::make_unique<elysia::ui::UiChromeContainer>(screen_rect());
    chrome->set_header_height(kHeaderHeight);
    chrome->set_header_padding({ 12,6,12,6 });
    chrome->set_body_padding({ 20,20,20,20 });
    _chrome = chrome.get();
    UiChildHost::add_child(std::move(chrome));

    auto title = std::make_unique<elysia::ui::UiLabel>(elysia::core::Rect{ 0,0,400,36 });
    title->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
    title->set_typography_role(elysia::typography::UiTypographyRole::DialogTitle);
    title->set_vertical_align(elysia::ui::TextVerticalAlign::Center);
    _title = title.get();
    _chrome->add_title_child(std::move(title));

    auto close = std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,36,36 });
    close->set_typography_role(elysia::typography::UiTypographyRole::ButtonCompact);
    close->set_on_click([this]() { this->close(); });
    _close = close.get();
    _chrome->add_right_action(std::move(close));

    auto panel = std::make_unique<elysia::ui::UiPanel>(elysia::core::Rect{ 0,0,600,360 });
    auto panel_style = panel->style();
    panel_style.draw_border = false;
    panel->set_base_style(panel_style);
    _body_panel = panel.get();

    auto scroll = std::make_unique<elysia::ui::UiScrollContainer>(elysia::core::Rect{ 0,0,600,280 });
    scroll->set_scroll_axis(elysia::ui::UiScrollAxis::Vertical);
    scroll->set_scrollbar_visibility(elysia::ui::UiScrollBarVisibility::Auto);
    scroll->set_scroll_step({ 20,28 });
    _scroll = scroll.get();
    auto body = std::make_unique<elysia::ui::UiTextBlock>(elysia::core::Rect{ 0,0,600,0 });
    body->set_typography_role(elysia::typography::UiTypographyRole::DialogBody);
    body->set_horizontal_align(elysia::ui::TextHorizontalAlign::Left);
    body->set_padding(8);
    _body = body.get();
    _scroll->set_content(std::move(body));

    auto actions = std::make_unique<elysia::ui::UiListContainer>(elysia::core::Rect{ 0,0,600,kActionHeight });
    actions->set_direction(elysia::ui::UiListDirection::Horizontal);
    actions->set_item_spacing(kActionSpacing);
    _actions = actions.get();
    auto copy = std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,240,kActionHeight });
    copy->set_typography_role(elysia::typography::UiTypographyRole::DialogAction);
    copy->set_on_click([this]() { copy_information(); });
    _copy = copy.get();
    _actions->add_back(std::move(copy));
    auto exit = std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,240,kActionHeight });
    exit->set_typography_role(elysia::typography::UiTypographyRole::DialogAction);
    exit->set_visual_role(elysia::ui::UiButtonVisualRole::Danger);
    exit->set_on_click([this]() { exit_application(); });
    _exit = exit.get();
    _actions->add_back(std::move(exit));

    _body_panel->add_child(std::move(scroll),anchored(
        elysia::ui::UiLayoutAnchor::TopLeft,{ 600,280 }));
    _body_panel->add_child(std::move(actions),anchored(
        elysia::ui::UiLayoutAnchor::BottomCenter,{ 600,kActionHeight }));
    _chrome->set_body(std::move(panel));
}

void ApplicationFailureDialog::sync_config()
{
    if (_title) _title->set_text_content(_config.title);
    if (_body) _body->set_text_content(_config.body);
    if (_copy) _copy->set_text_content(_config.copy);
    if (_exit) _exit->set_text_content(_config.exit);
    if (_close) _close->set_text_content(_config.close);
}

void ApplicationFailureDialog::sync_delegated_focus() noexcept
{
    sync_delegated_scope_focus(focused_target(),is_scope_focused(),
        delegated_focus_regions(*this));
}

void ApplicationFailureDialog::copy_information()
{
    if (SDL_SetClipboardText(_config.copy_report.c_str()))
    {
        if (_copy) _copy->set_text_content(_config.copied);
        return;
    }
    if (_copy) _copy->set_text_content(_config.copy_failed);
    elysia::tools::Logger::instance()->warn(
        "application_failure",std::string("Copy failure information failed: ") + SDL_GetError());
}

void ApplicationFailureDialog::exit_application()
{
    const auto callback = _on_exit;
    if (callback)
        callback();
}

bool ApplicationFailureDialog::is_default_overlay_options(
    const elysia::ui::UiOverlayOptions& options) noexcept
{
    const elysia::ui::UiOverlayOptions defaults{};
    return options.open == defaults.open && options.modal == defaults.modal
        && options.close_on_cancel == defaults.close_on_cancel
        && options.close_on_outside_click == defaults.close_on_outside_click
        && options.placement == defaults.placement
        && options.transition == defaults.transition
        && options.fallback_size.nearly_equals(defaults.fallback_size)
        && options.order == defaults.order;
}
}
