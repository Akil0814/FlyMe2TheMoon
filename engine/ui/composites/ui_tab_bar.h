#pragma once

#include "../containers/ui_list_container.h"
#include "../text/ui_text_content.h"

#include <functional>
#include <optional>

namespace elysia::ui
{
class UiButton;

class UiTabBar final : public UiListContainer
{
public:
    using IndexChangedCallback = std::function<void(std::optional<std::size_t>)>;

    explicit UiTabBar(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    void reset() noexcept override;
    void update(double delta) override;
    bool on_ui_input_event(const UiInputEvent& event) override;

    UiButton* add_tab(UiTextContent content);
    [[nodiscard]] std::unique_ptr<UiElement> extract_tab(std::size_t index);
    void clear_tabs();
    [[nodiscard]] std::size_t tab_count() const noexcept { return child_count(); }
    [[nodiscard]] std::optional<std::size_t> focused_index() const noexcept;
    [[nodiscard]] std::optional<std::size_t> selected_index() const noexcept;
    bool set_focused_index(std::size_t index);
    bool set_selected_index(std::size_t index);
    void clear_selection() noexcept;
    void set_on_focus_changed(IndexChangedCallback callback);
    void set_on_selection_changed(IndexChangedCallback callback);

private:
    [[nodiscard]] UiButton* button_at(std::size_t index) const noexcept;
    [[nodiscard]] std::optional<std::size_t> index_of(const UiButton* button) const noexcept;
    void sync_state(bool notify);
    void refresh_styles();

    UiButton* _selected = nullptr;
    std::optional<std::size_t> _last_focused;
    IndexChangedCallback _on_focused_changed;
    IndexChangedCallback _on_selected_changed;
    bool _suppress_callbacks = false;
};
}
