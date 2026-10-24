#include "ui_confirmation_dialog.h"

#include "../containers/ui_chrome_container.h"
#include "../containers/ui_list_container.h"
#include "../containers/ui_panel.h"
#include "../widgets/label/ui_label.h"
#include "../widgets/text/ui_text_block.h"
#include "../widgets/ui_button.h"
#include "../window/ui_window.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace elysia::ui
{
using elysia::typography::UiTypographyRole;

namespace
{
constexpr float kHeaderHeight = 48.0f;
constexpr float kButtonHeight = 56.0f;
constexpr float kButtonSpacing = 20.0f;
constexpr float kMessageButtonSpacing = 16.0f;

UiLayoutChildOptions anchored_options(UiLayoutAnchor anchor,const elysia::core::Vector2& size)
{
    return UiLayoutChildOptions{
        ._anchor = anchor,
        ._margin = UiLayoutMargin{},
        ._cross_align = UiLayoutAlign::Start,
        ._size_override = size,
        ._use_custom_cross_align = false,
        ._fill_cross_axis = false,
        ._use_size_override = true
    };
}
}

UiConfirmationDialog::UiConfirmationDialog(const elysia::core::Rect& rect,int order) noexcept
    : UiControlFocusScopeHost(rect,order)
{
    reset();
}

UiConfirmationDialog::UiConfirmationDialog(
    const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order
) noexcept : UiConfirmationDialog(elysia::core::Rect(position.x,position.y,size.x,size.y),order) {}

UiConfirmationDialog::UiConfirmationDialog(
    const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order
) noexcept : UiConfirmationDialog(elysia::core::Rect::from_center(center,size),order) {}

UiConfirmationDialog::~UiConfirmationDialog()
{
    unregister_from_window();
}

void UiConfirmationDialog::reset() noexcept
{
    unregister_from_window();
    UiControlFocusScopeHost::reset();
    reset_delegated_focus_state();
    _chrome = nullptr;
    _title_label = nullptr;
    _close_button = nullptr;
    _body_panel = nullptr;
    _message_text = nullptr;
    _action_row = nullptr;
    _cancel_button = nullptr;
    _confirm_button = nullptr;
    _registered_window = nullptr;
    _config = UiConfirmationDialogConfig{};
    _on_confirm = {};

    create_internal_children();
    sync_config_to_children();
}

void UiConfirmationDialog::update(double delta)
{
    UiControlFocusScopeHost::update(delta);
    sync_delegated_focus();
}

void UiConfirmationDialog::on_ui_input_frame(const UiInputFrame& input)
{
    UiControlFocusScopeHost::on_ui_input_frame(input);
    sync_delegated_focus();
}

bool UiConfirmationDialog::on_ui_input_event(const UiInputEvent& event)
{
    // The outer dialog flattens Chrome controls for focus ownership, but Chrome owns
    // the directional links between its body action buttons. Route action input there
    // so keyboard and gamepad navigation stay inside the modal action row.
    const bool is_action_navigation = event.type == UiInputEventType::ActionPressed
        && is_navigation_action(event.action);
    const bool is_confirm_action = event.action == UiAction::Confirm
        && (event.type == UiInputEventType::ActionPressed || event.type == UiInputEventType::ActionReleased);
    if (_chrome && (is_action_navigation || is_confirm_action))
    {
        const bool handled = _chrome->on_ui_input_event(event);
        sync_host_delegated_focus_target(*this);
        sync_delegated_focus();
        return handled;
    }

    const bool handled = UiControlFocusScopeHost::on_ui_input_event(event);
    sync_host_delegated_focus_target(*this);
    sync_delegated_focus();
    return handled;
}

void UiConfirmationDialog::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;

    auto* self = const_cast<UiConfirmationDialog*>(this);
    self->cleanup_destroyed_children();
    self->update_layout_if_dirty();
    self->refresh_focus_registry();
    self->ensure_valid_focus();
    self->sync_host_delegated_focus_target(*self);
    self->sync_delegated_focus();
    self->apply_focus_state();
    UiControlFocusScopeHost::submit_ui_render_commands(out_commands);
}

elysia::core::Vector2 UiConfirmationDialog::content_extent() const noexcept
{
    const elysia::core::Vector2 explicit_size = size();
    if (!_chrome)
        return explicit_size;

    const elysia::core::Vector2 chrome_extent = _chrome->content_extent();
    return elysia::core::Vector2(
        std::max(explicit_size.x,chrome_extent.x),
        std::max(explicit_size.y,chrome_extent.y));
}

bool UiConfirmationDialog::focus_first_available()
{
    // Prime the outer graph, then enter Chrome's body so Cancel wins over header Close.
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

void UiConfirmationDialog::set_config(const UiConfirmationDialogConfig& config)
{
    _config = config;
    sync_config_to_children();
    notify_base_style_invalidated();
    mark_layout_dirty();
}

const UiConfirmationDialogConfig& UiConfirmationDialog::config() const noexcept
{
    return _config;
}

void UiConfirmationDialog::set_on_confirm(UiConfirmationDialogCallback on_confirm)
{
    _on_confirm = std::move(on_confirm);
}

bool UiConfirmationDialog::register_with_window(UiWindow& window,UiOverlayOptions options)
{
    if (is_destroyed() || layout_parent() != &window)
        return false;
    if (_registered_window && _registered_window != &window)
        _registered_window->unregister_overlay(*this);
    // UiWindow owns overlay state; this pointer only forwards later open/close requests.
    if (is_default_overlay_options(options))
    {
        options.open = false;
        options.modal = true;
        options.close_on_cancel = true;
        options.close_on_outside_click = false;
        options.placement = UiOverlayPlacement::Center;
        options.transition = UiOverlayTransition::None;
        options.order = 1000;
    }
    options.fallback_size = size();
    if (!window.register_overlay(*this,options))
        return false;
    _registered_window = &window;
    return true;
}

void UiConfirmationDialog::unregister_from_window() noexcept
{
    UiWindow* window = _registered_window;
    _registered_window = nullptr;
    if (window)
        window->unregister_overlay(*this);
}

void UiConfirmationDialog::on_window_detached(UiWindow& window) noexcept
{
    if (_registered_window == &window)
        _registered_window = nullptr;
}

void UiConfirmationDialog::open()
{
    if (_registered_window && !_registered_window->is_destroyed())
        _registered_window->open_overlay(*this);
}

void UiConfirmationDialog::close()
{
    if (_registered_window && !_registered_window->is_destroyed())
        _registered_window->close_overlay(*this);
}

void UiConfirmationDialog::rebuild_layout()
{
    if (!_chrome || !_body_panel || !_message_text || !_action_row)
        return;

    _chrome->set_screen_rect(content_rect());
    _chrome->update_layout_if_dirty();

    const elysia::core::Rect panel_rect = _body_panel->screen_rect();
    const float action_width = std::max(0.0f,panel_rect.width());
    const float message_height = std::max(
        0.0f,panel_rect.height() - kButtonHeight - kMessageButtonSpacing);
    _body_panel->set_child_layout_options(
        0,anchored_options(UiLayoutAnchor::TopCenter,elysia::core::Vector2(panel_rect.width(),message_height)));
    _body_panel->set_child_layout_options(
        1,anchored_options(UiLayoutAnchor::BottomCenter,elysia::core::Vector2(action_width,kButtonHeight)));
    _body_panel->update_layout_if_dirty();
    _action_row->update_layout_if_dirty();
}

void UiConfirmationDialog::rebuild_focus_registry()
{
    std::vector<DelegatedRegionEntry> regions;
    for (UiElement* region : delegated_focus_regions(*this))
        regions.push_back(DelegatedRegionEntry{ region,nullptr,nullptr,nullptr,nullptr });

    std::vector<FocusEntry> entries;
    build_delegated_focus_entries(regions,entries);
    set_focus_entries(std::move(entries));
}


void UiConfirmationDialog::create_internal_children()
{
    auto chrome = std::make_unique<UiChromeContainer>(screen_rect());
    chrome->set_header_height(kHeaderHeight);
    chrome->set_header_padding(UiLayoutPadding{ 12.0f,6.0f,12.0f,6.0f });
    chrome->set_body_padding(UiLayoutPadding{ 20.0f,20.0f,20.0f,20.0f });
    _chrome = chrome.get();
    UiChildHost::add_child(std::move(chrome));

    auto title = std::make_unique<UiLabel>(elysia::core::Rect{ 0,0,280,36 });
    title->set_visual_role(UiLabelVisualRole::Title);
    title->set_typography_role(UiTypographyRole::DialogTitle);
    title->set_vertical_align(TextVerticalAlign::Center);
    _title_label = title.get();
    _chrome->add_title_child(std::move(title));

    auto close_button = std::make_unique<UiButton>(elysia::core::Rect{ 0,0,36,36 });
    close_button->set_typography_role(UiTypographyRole::ButtonCompact);
    close_button->set_on_click([this]() { close(); });
    _close_button = close_button.get();
    _chrome->add_right_action(std::move(close_button));

    auto body = std::make_unique<UiPanel>(elysia::core::Rect{ 0,0,380,152 });
    auto body_style = body->style();
    body_style.draw_border = false;
    body->set_base_style(body_style);
    _body_panel = body.get();

    auto message = std::make_unique<UiTextBlock>(
        elysia::core::Rect{ 0,0,380,0 });
    message->set_typography_role(UiTypographyRole::DialogBody);
    message->set_horizontal_align(TextHorizontalAlign::Center);
    _message_text = message.get();
    _body_panel->add_child(std::move(message),anchored_options(
        UiLayoutAnchor::TopCenter,elysia::core::Vector2(380.0f,0.0f)));

    auto action_row = std::make_unique<UiListContainer>(elysia::core::Rect{ 0,0,380,kButtonHeight });
    action_row->set_direction(UiListDirection::Horizontal);
    action_row->set_item_spacing(kButtonSpacing);
    _action_row = action_row.get();

    auto cancel_button = std::make_unique<UiButton>(elysia::core::Rect{ 0,0,180,kButtonHeight });
    cancel_button->set_typography_role(UiTypographyRole::DialogAction);
    cancel_button->set_on_click([this]() { close(); });
    _cancel_button = cancel_button.get();
    _action_row->add_back(std::move(cancel_button));

    auto confirm_button = std::make_unique<UiButton>(elysia::core::Rect{ 0,0,180,kButtonHeight });
    confirm_button->set_typography_role(UiTypographyRole::DialogAction);
    confirm_button->set_on_click([this]() { confirm(); });
    _confirm_button = confirm_button.get();
    _action_row->add_back(std::move(confirm_button));

    _body_panel->add_child(std::move(action_row),anchored_options(
        UiLayoutAnchor::BottomCenter,elysia::core::Vector2(380.0f,kButtonHeight)));
    _chrome->set_body(std::move(body));
}

void UiConfirmationDialog::sync_config_to_children()
{
    if (_title_label)
        _title_label->set_text_content(_config.title);
    if (_message_text)
        _message_text->set_text_content(_config.message);
    if (_confirm_button)
    {
        _confirm_button->set_text_content(_config.confirm);
        _confirm_button->set_visual_role(_config.confirm_visual_role);
    }
    if (_cancel_button)
    {
        _cancel_button->set_text_content(_config.cancel);
        _cancel_button->set_visual_role(_config.cancel_visual_role);
    }
    if (_close_button)
    {
        _close_button->set_text_content(_config.close);
        const bool show_close = !_config.close.empty();
        _close_button->set_visible(show_close);
        _close_button->set_active(show_close);
        _close_button->set_enabled(show_close);
    }
}


void UiConfirmationDialog::sync_delegated_focus() noexcept
{
    sync_delegated_scope_focus(
        UiControlFocusScopeHost::focused_target(),is_scope_focused(),delegated_focus_regions(*this));
}

void UiConfirmationDialog::confirm()
{
    UiConfirmationDialogCallback callback = _on_confirm;
    close();
    if (callback)
        callback();
}

bool UiConfirmationDialog::is_default_overlay_options(const UiOverlayOptions& options) noexcept
{
    const UiOverlayOptions defaults{};
    return options.open == defaults.open
        && options.modal == defaults.modal
        && options.close_on_cancel == defaults.close_on_cancel
        && options.close_on_outside_click == defaults.close_on_outside_click
        && options.placement == defaults.placement
        && options.transition == defaults.transition
        && options.fallback_size.nearly_equals(defaults.fallback_size)
        && options.order == defaults.order;
}
}
