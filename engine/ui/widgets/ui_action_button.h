#pragma once

#include "../core/ui_element.h"
#include "../input/contracts/ui_input_event_receiver.h"
#include "../style/ui_interaction_style.h"
#include "../text/ui_text_content.h"
#include "../../typography/font_settings.h"

#include <functional>
#include <optional>
#include <variant>

namespace elysia::ui
{
struct UiActionButtonIconContent
{
    SDL_Texture* texture = nullptr; // Caller-owned.
    std::optional<elysia::core::Rect> source_rect;
};

using UiActionButtonContent = std::variant<std::monostate,UiTextContent,UiActionButtonIconContent>;

enum class UiActionButtonInteractionPhase { Pressed,Released,Canceled };

struct UiActionButtonInteraction
{
    UiActionButtonInteractionPhase phase;
    bool clicked = false; // Only Released can represent a successful click.
};

struct UiActionButtonStyle
{
    UiChromeStyle chrome{};
    UiEnabledDisabledColors text{};
    elysia::core::Color overlay{ 0,0,0,160 };
    elysia::core::Color selected_border = UiPalette::accent;
    elysia::core::UiStrokeWidth selected_border_width{};
    float padding = 4.0f;
};

struct UiActionButtonStyleOverrides
{
    UiChromeStyleOverrides chrome{};
    UiEnabledDisabledColorsOverrides text{};
    std::optional<elysia::core::Color> overlay;
    std::optional<elysia::core::Color> selected_border;
    std::optional<elysia::core::UiStrokeWidth> selected_border_width;
    std::optional<float> padding;
};

template<> struct UiStyleOverrideTraits<UiActionButtonStyle>
{
    using Overrides = UiActionButtonStyleOverrides;
    static bool empty(const Overrides& o) noexcept
    {
        return elysia::ui::empty(o.chrome) && elysia::ui::empty(o.text)
            && !o.overlay && !o.selected_border && !o.selected_border_width && !o.padding;
    }
    static void apply(UiActionButtonStyle& s,const Overrides& o) noexcept
    {
        apply_ui_style_overrides(s.chrome,o.chrome);
        apply_ui_style_overrides(s.text,o.text);
        apply_ui_style_override(s.overlay,o.overlay);
        apply_ui_style_override(s.selected_border,o.selected_border);
        apply_ui_style_override(s.selected_border_width,o.selected_border_width);
        apply_ui_style_override(s.padding,o.padding);
    }
};

// Pointer-operated HUD slot. Gameplay bindings and behavior belong to its caller.
class UiActionButton : public UiElement, public UiInputEventReceiver
{
public:
    using InteractionCallback = std::function<void(const UiActionButtonInteraction&)>;

    explicit UiActionButton(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0);
    UiActionButton(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0);
    UiActionButton(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0);

    void reset() noexcept override;
    void cancel_input_interaction() override;
    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

    void set_content(UiActionButtonContent content) { _content = std::move(content); }
    [[nodiscard]] const UiActionButtonContent& content() const noexcept { return _content; }
    void set_key_hint(UiTextContent text) { _key_hint = std::move(text); }
    [[nodiscard]] const UiTextContent& key_hint() const noexcept { return _key_hint; }
    void set_badge_text(UiTextContent text) { _badge_text = std::move(text); }
    [[nodiscard]] const UiTextContent& badge_text() const noexcept { return _badge_text; }
    void set_selected(bool selected) noexcept { _selected = selected; }
    [[nodiscard]] bool is_selected() const noexcept { return _selected; }
    void set_overlay_ratio(float ratio) noexcept;
    [[nodiscard]] float overlay_ratio() const noexcept { return _overlay_ratio; }
    void set_enabled(bool enabled);
    [[nodiscard]] bool is_enabled() const noexcept { return _enabled; }
    void set_external_pressed(bool pressed) noexcept { _external_pressed = pressed; }
    [[nodiscard]] bool is_external_pressed() const noexcept { return _external_pressed; }
    [[nodiscard]] bool is_pointer_pressed() const noexcept { return _pointer_pressed; }
    [[nodiscard]] bool is_pressed() const noexcept { return _pointer_pressed || _external_pressed; }
    void set_on_interaction(InteractionCallback callback) { _on_interaction = std::move(callback); }

    void set_base_style(const UiActionButtonStyle& style) noexcept { _style_state.set_base_style(style); }
    void set_style_overrides(const UiActionButtonStyleOverrides& overrides) noexcept { _style_state.set_style_overrides(overrides); }
    [[nodiscard]] const UiActionButtonStyle& style() const noexcept { return _style_state.effective_style(); }
    [[nodiscard]] const UiActionButtonStyleOverrides& style_overrides() const noexcept { return _style_state.style_overrides(); }
    [[nodiscard]] bool has_style_overrides() const noexcept { return _style_state.has_style_overrides(); }
    void clear_style_overrides() noexcept { _style_state.clear_style_overrides(); }
    void set_font_source_override(elysia::typography::FontSource source) noexcept { _font_source_override = source; }
    void clear_font_source_override() noexcept { _font_source_override.reset(); }

private:
    enum class TextAnchor { Center,BottomLeft,TopRight };
    [[nodiscard]] bool can_interact() const noexcept;
    [[nodiscard]] elysia::core::Rect content_rect() const noexcept;
    void append_text(std::vector<elysia::core::UiRenderCommand>& out,const UiTextContent& text,
        elysia::typography::UiTypographyRole role,TextAnchor anchor) const;

    UiActionButtonContent _content;
    UiTextContent _key_hint,_badge_text;
    UiStyleState<UiActionButtonStyle> _style_state;
    std::optional<elysia::typography::FontSource> _font_source_override;
    InteractionCallback _on_interaction;
    float _overlay_ratio = 0.0f;
    bool _enabled = true,_selected = false,_external_pressed = false,_pointer_pressed = false,_hovered = false;
};
}
