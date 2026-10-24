#pragma once

#include "../widgets/ui_button.h"
#include "../widgets/label/ui_label.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace elysia::ui
{
enum class UiOptionSelectorOrientation { Horizontal, Vertical };

struct UiOptionSelectorOption
{
    UiTextContent content{};
    bool enabled = true;
};

struct UiOptionSelectorStyle
{
    UiChromeStyle chrome{};
    UiButtonStyle arrow{};
    UiEnabledDisabledColors text{};
    float arrow_extent = 40.0f;
    float spacing = 8.0f;
    float padding = 4.0f;
};

struct UiOptionSelectorStyleOverrides
{
    UiChromeStyleOverrides chrome{};
    UiButtonStyleOverrides arrow{};
    UiEnabledDisabledColorsOverrides text{};
    std::optional<float> arrow_extent;
    std::optional<float> spacing;
    std::optional<float> padding;
};

template<> struct UiStyleOverrideTraits<UiOptionSelectorStyle>
{
    using Overrides = UiOptionSelectorStyleOverrides;
    static bool empty(const Overrides& o) noexcept
    {
        return elysia::ui::empty(o.chrome) && UiStyleOverrideTraits<UiButtonStyle>::empty(o.arrow)
            && elysia::ui::empty(o.text) && !o.arrow_extent && !o.spacing && !o.padding;
    }
    static void apply(UiOptionSelectorStyle& s,const Overrides& o) noexcept
    {
        apply_ui_style_overrides(s.chrome,o.chrome);
        UiStyleOverrideTraits<UiButtonStyle>::apply(s.arrow,o.arrow);
        apply_ui_style_overrides(s.text,o.text);
        apply_ui_style_override(s.arrow_extent,o.arrow_extent);
        apply_ui_style_override(s.spacing,o.spacing);
        apply_ui_style_override(s.padding,o.padding);
    }
};

using UiOptionSelectorSelectionChangedCallback = std::function<void(std::size_t)>;

// One focus target; pointer arrows step directly, while Confirm toggles directional adjustment.
class UiOptionSelector final : public UiControl
{
public:
    explicit UiOptionSelector(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiOptionSelector(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiOptionSelector(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;

    void reset() noexcept override;
    void cancel_input_interaction() noexcept override;
    void set_enabled(bool enabled) override;
    void set_focused(bool focused) override;
    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out) const override;

    // Structural changes silently repair selection and end adjustment.
    void set_options(std::vector<UiOptionSelectorOption> options);
    [[nodiscard]] const std::vector<UiOptionSelectorOption>& options() const noexcept;
    void clear_options();
    [[nodiscard]] std::optional<std::size_t> selected_index() const noexcept;
    // Valid assignments return true; only actual changes notify, including programmatic changes.
    [[nodiscard]] bool set_selected_index(std::size_t index);
    [[nodiscard]] bool select_previous();
    [[nodiscard]] bool select_next();
    void set_on_selection_changed(UiOptionSelectorSelectionChangedCallback callback);

    void set_orientation(UiOptionSelectorOrientation orientation) noexcept;
    [[nodiscard]] UiOptionSelectorOrientation orientation() const noexcept;
    void set_wrap_selection(bool wrap) noexcept;
    [[nodiscard]] bool wraps_selection() const noexcept;
    [[nodiscard]] bool is_adjusting() const noexcept;

    void set_base_style(const UiOptionSelectorStyle& style) noexcept;
    void set_style_overrides(const UiOptionSelectorStyleOverrides& overrides) noexcept;
    [[nodiscard]] const UiOptionSelectorStyle& style() const noexcept;
    [[nodiscard]] const UiOptionSelectorStyleOverrides& style_overrides() const noexcept;
    [[nodiscard]] bool has_style_overrides() const noexcept;
    void clear_style_overrides() noexcept;

private:
    enum class Region { None,Previous,Center,Next };
    struct Layout { elysia::core::Rect previous,center,next; };
    [[nodiscard]] Layout compute_layout() const noexcept;
    [[nodiscard]] Region region_at(const UiInputEvent& event) const noexcept;
    [[nodiscard]] std::optional<std::size_t> adjacent_index(int direction) const noexcept;
    [[nodiscard]] bool can_receive_pointer() const noexcept;
    [[nodiscard]] bool can_interact() const noexcept;
    void toggle_adjustment() noexcept;
    void sync_children() const;
    void draw_arrow(std::vector<elysia::core::UiRenderCommand>& out,const elysia::core::Rect& rect,
        bool previous,bool enabled) const;

    std::vector<UiOptionSelectorOption> _options;
    std::optional<std::size_t> _selected_index;
    UiOptionSelectorSelectionChangedCallback _on_selection_changed;
    UiStyleState<UiOptionSelectorStyle> _style_state;
    UiOptionSelectorOrientation _orientation = UiOptionSelectorOrientation::Horizontal;
    bool _wrap_selection = false;
    bool _is_adjusting = false;
    Region _pressed_region = Region::None;
    Region _hovered_region = Region::None;
    mutable UiButton _previous;
    mutable UiButton _next;
    mutable UiLabel _label;
};
}
