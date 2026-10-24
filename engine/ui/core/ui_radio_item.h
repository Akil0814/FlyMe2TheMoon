#pragma once

namespace elysia::ui
{
class UiElement;

// Minimal selection contract shared by atomic and labeled radio controls.
class UiRadioItem
{
public:
    virtual ~UiRadioItem() = default;
    [[nodiscard]] virtual UiElement& radio_item_element() noexcept = 0;
    [[nodiscard]] virtual const UiElement& radio_item_element() const noexcept = 0;
    virtual void set_selected(bool selected) noexcept = 0;
    [[nodiscard]] virtual bool is_selected() const noexcept = 0;
};
}
