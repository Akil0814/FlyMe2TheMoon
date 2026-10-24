#pragma once

#include "../focus/ui_control_focus_scope_host.h"
#include "../focus/ui_delegated_focus_mixin.h"

#include <optional>

namespace elysia::ui
{
class UiTabView final : public UiControlFocusScopeHost, private UiDelegatedFocusMixin
{
public:
    explicit UiTabView(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    void reset() noexcept override;
    void update(double delta) override;
    void on_ui_input_frame(const UiInputFrame& input) override;
    bool on_ui_input_event(const UiInputEvent& event) override;
    bool focus_first_available() override;

    UiElement* add_page(std::unique_ptr<UiElement> page);
    [[nodiscard]] std::unique_ptr<UiElement> extract_page(std::size_t index);
    void clear_pages();
    [[nodiscard]] std::size_t page_count() const noexcept { return child_count(); }
    [[nodiscard]] UiElement* page_at(std::size_t index) noexcept { return child_at(index); }
    [[nodiscard]] const UiElement* page_at(std::size_t index) const noexcept { return child_at(index); }
    [[nodiscard]] std::optional<std::size_t> selected_index() const noexcept { return _selected_index; }
    bool set_selected_index(std::size_t index);
    void clear_selection() noexcept;

protected:
    void rebuild_layout() override;
    void rebuild_focus_registry() override;

private:
    void sync_page_states() noexcept;
    std::optional<std::size_t> _selected_index;
};
}
