#pragma once

#include "../containers/ui_panel.h"
#include "../containers/ui_scroll_container.h"
#include "../core/ui_control.h"
#include "../style/ui_style.h"
#include "../style/ui_visual_roles.h"
#include "../style/ui_visual_styles.h"
#include "../text/ui_text_content.h"
#include "../window/ui_transient_popup.h"
#include "../widgets/ui_button.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace elysia::ui
{
class UiListContainer;
class UiWindow;

struct UiDropdownBaseStyle
{
    UiDropdownStyle layout{};
    UiButtonStyle trigger{};
    UiPanelStyle popup{};
    UiScrollContainerStyle scroll{};
    UiButtonStyle option{};
};

// One popup row; disabled entries remain visible but are skipped by focus navigation.
struct UiDropdownOption
{
    UiTextContent content{};
    bool enabled = true;
};

using UiDropdownSelectionChangedCallback = std::function<void(std::size_t selected_index)>;

// Dropdown composite with a trigger and window-rendered transient option popup.
class UiDropdown final : public UiControl, public UiTransientPopup
{
public:
    explicit UiDropdown(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiDropdown(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiDropdown(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;
    ~UiDropdown() override;

    void reset() noexcept override;
    void set_enabled(bool enabled) override;
    void set_focused(bool focused) override;
    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

    void set_options(std::vector<UiDropdownOption> options);
    [[nodiscard]] const std::vector<UiDropdownOption>& options() const noexcept;
    void add_option(UiDropdownOption option);
    void clear_options();

    [[nodiscard]] std::optional<std::size_t> selected_index() const noexcept;
    [[nodiscard]] bool set_selected_index(std::size_t index);
    void set_on_selection_changed(UiDropdownSelectionChangedCallback on_selection_changed);

    void open();
    void close() noexcept;
    void toggle();
    [[nodiscard]] bool is_open() const noexcept override;

    // Registration lets the window prioritize popup rendering and input without taking ownership.
    void register_with_window(UiWindow& window);
    void unregister_from_window() noexcept;

    void set_base_style(const UiDropdownBaseStyle& style) noexcept;
    void set_style_overrides(const UiDropdownStyleOverrides& overrides) noexcept;
    [[nodiscard]] const UiDropdownStyle& style() const noexcept;
    [[nodiscard]] const UiDropdownStyleOverrides& style_overrides() const noexcept;
    [[nodiscard]] bool has_style_overrides() const noexcept;
    void clear_style_overrides() noexcept;
    void set_visual_role(UiDropdownVisualRole role) noexcept;
    [[nodiscard]] UiDropdownVisualRole visual_role() const noexcept;

    [[nodiscard]] UiElement& popup_owner() noexcept override;
    [[nodiscard]] const UiElement& popup_owner() const noexcept override;
    [[nodiscard]] bool contains_popup_point(int mouse_x,int mouse_y) const noexcept override;
    void on_window_detached(UiWindow& window) noexcept override;
    bool on_popup_input_event(const UiInputEvent& event) override;
    void submit_popup_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

private:
    // Structural option changes rebuild rows; selection changes reuse the existing buttons.
    void create_popup_content();
    void rebuild_option_buttons();
    void sync_visual_state();
    void sync_popup_layout();
    [[nodiscard]] std::optional<std::size_t> first_enabled_option() const noexcept;
    [[nodiscard]] std::optional<std::size_t> next_enabled_option(int direction) const noexcept;
    [[nodiscard]] bool commit_selected_index(std::size_t index);
    void set_focused_option(std::optional<std::size_t> index);
    void sync_focused_option_from_popup_list();
    void ensure_focused_option_visible() noexcept;
    [[nodiscard]] UiButton* option_button_at(std::size_t index) noexcept;
    [[nodiscard]] const UiButton* option_button_at(std::size_t index) const noexcept;
    [[nodiscard]] bool contains_trigger_point(int mouse_x,int mouse_y) const noexcept;
    [[nodiscard]] bool is_pointer_event(const UiInputEvent& event) const noexcept;

private:
    UiButton _trigger;
    UiPanel _popup_panel;
    UiScrollContainer _popup_scroll;
    UiListContainer* _popup_list = nullptr;
    std::vector<UiDropdownOption> _options;
    UiDropdownSelectionChangedCallback _on_selection_changed;
    UiStyleState<UiDropdownStyle> _style_state;
    UiDropdownVisualRole _visual_role = UiDropdownVisualRole::Default;
    UiWindow* _window = nullptr;
    std::optional<std::size_t> _selected_index;
    std::optional<std::size_t> _focused_option;
    bool _expanded = false;
};
}
