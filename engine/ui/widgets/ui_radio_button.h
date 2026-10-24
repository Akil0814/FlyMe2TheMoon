#pragma once

#include "../core/ui_control.h"
#include "../core/ui_radio_item.h"
#include "../style/ui_style.h"
#include "../style/ui_interaction_style.h"

#include <functional>
#include <optional>
#include <string>

namespace elysia::ui
{
struct UiRadioButtonSounds
{
    std::string focus;
    std::string press;
    std::string select;
};

struct UiRadioButtonStyle
{
    UiChromeStyle chrome{};
    UiEnabledDisabledColors mark{};
};
struct UiRadioButtonStyleOverrides { UiChromeStyleOverrides chrome{}; UiEnabledDisabledColorsOverrides mark{}; };
template<> struct UiStyleOverrideTraits<UiRadioButtonStyle> { using Overrides=UiRadioButtonStyleOverrides; static bool empty(const Overrides& o) noexcept { return elysia::ui::empty(o.chrome)&&elysia::ui::empty(o.mark); } static void apply(UiRadioButtonStyle& s,const Overrides& o) noexcept { apply_ui_style_overrides(s.chrome,o.chrome); apply_ui_style_overrides(s.mark,o.mark); } };

struct UiRadioButtonConfig
{
    std::optional<UiRadioButtonSounds> sounds;
    std::optional<UiRadioButtonStyleOverrides> style_overrides;
    int padding = 4;
};

using UiRadioButtonSelectedCallback = std::function<void()>;

class UiRadioButton final : public UiControl, public UiRadioItem
{
public:
    explicit UiRadioButton(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiRadioButton(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiRadioButton(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;
    UiRadioButton(const elysia::core::Rect& rect,const UiRadioButtonConfig& config,int order = 0) noexcept;

    void cancel_input_interaction() noexcept override
    {
        _pushed = false;
    }
    void reset() noexcept override;
    void set_enabled(bool enabled) override;
    void set_focused(bool focused) override;
    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

    void set_radio_button_config(const UiRadioButtonConfig& config);
    void set_selected(bool selected) noexcept override;
    [[nodiscard]] bool is_selected() const noexcept override;
    void select();
    void set_on_selection_changed(UiRadioButtonSelectedCallback callback);
    void set_sounds(const UiRadioButtonSounds& sounds);
    void clear_sounds() noexcept;
    void set_base_style(const UiRadioButtonStyle& style) noexcept;
    void set_style_overrides(const UiRadioButtonStyleOverrides& overrides) noexcept;
    [[nodiscard]] const UiRadioButtonStyle& style() const noexcept;
    [[nodiscard]] const UiRadioButtonStyleOverrides& style_overrides() const noexcept;
    [[nodiscard]] bool has_style_overrides() const noexcept;
    void clear_style_overrides() noexcept;
    void set_padding(int padding) noexcept;
    [[nodiscard]] int padding() const noexcept;

    [[nodiscard]] UiElement& radio_item_element() noexcept override { return *this; }
    [[nodiscard]] const UiElement& radio_item_element() const noexcept override { return *this; }

private:
    [[nodiscard]] bool select_internal(bool notify);
    [[nodiscard]] bool contains_pointer(int x,int y) const noexcept;
    [[nodiscard]] bool can_interact() const noexcept;
    [[nodiscard]] bool is_primary_pointer_event(const UiInputEvent& event) const noexcept;
    void play_sound_if_set(const std::string& key) const;
    [[nodiscard]] elysia::core::Color background_color() const noexcept;
    [[nodiscard]] elysia::core::Color border_color() const noexcept;
    [[nodiscard]] elysia::core::Color mark_color() const noexcept;

    UiRadioButtonSelectedCallback _on_selected;
    std::optional<UiRadioButtonSounds> _sounds;
    UiStyleState<UiRadioButtonStyle> _style_state;
    int _padding = 4;
    bool _selected = false;
    bool _pushed = false;
};
}
