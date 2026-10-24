#pragma once

#include "../focus/ui_control_focus_scope_host.h"
#include "../focus/ui_delegated_focus_mixin.h"
#include "../text/ui_text_content.h"

#include <functional>
#include <memory>
#include <optional>

namespace elysia::ui
{
class UiTabBar;
class UiTabView;

struct UiTabAddResult
{
    bool added = false;
    std::unique_ptr<UiElement> rejected_page;
};

class UiTabContainer final : public UiControlFocusScopeHost, private UiDelegatedFocusMixin
{
public:
    using IndexChangedCallback = std::function<void(std::optional<std::size_t>)>;

    explicit UiTabContainer(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    void reset() noexcept override;
    void update(double delta) override;
    void on_ui_input_frame(const UiInputFrame& input) override;
    bool on_ui_input_event(const UiInputEvent& event) override;
    bool focus_first_available() override;

    UiTabAddResult add_tab(UiTextContent label,std::unique_ptr<UiElement> page);
    [[nodiscard]] std::unique_ptr<UiElement> remove_tab(std::size_t index);
    void clear_tabs();
    [[nodiscard]] std::size_t tab_count() const noexcept;
    [[nodiscard]] std::size_t page_count() const noexcept;
    [[nodiscard]] std::optional<std::size_t> focused_index() const noexcept;
    [[nodiscard]] std::optional<std::size_t> selected_index() const noexcept;
    bool set_focused_index(std::size_t index);
    bool set_selected_index(std::size_t index);
    void set_on_focus_changed(IndexChangedCallback callback);
    void set_on_selection_changed(IndexChangedCallback callback);

protected:
    void rebuild_layout() override;
    void rebuild_focus_registry() override;

private:
    UiElement* add_child(std::unique_ptr<UiElement> child,UiLayoutChildOptions options = {}) override;
    void create_internal_children();
    void assert_invariant() const noexcept;
    void handle_selected_changed(std::optional<std::size_t> index);
    void handle_focused_changed(std::optional<std::size_t> index);

    UiTabBar* _tab_bar = nullptr;
    UiTabView* _tab_view = nullptr;
    IndexChangedCallback _on_focused_changed;
    IndexChangedCallback _on_selected_changed;
    bool _mutating = false;
};
}
