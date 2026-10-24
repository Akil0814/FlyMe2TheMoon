#pragma once

#include "../../core/render/colors.h"
#include "../style/ui_style.h"
#include "../style/ui_visual_roles.h"
#include "../style/ui_interaction_style.h"
#include "../core/ui_control.h"
#include "../text/ui_text_content.h"
#include "../text/ui_typography.h"
#include "../../typography/font_settings.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

struct SDL_Texture;

namespace elysia::ui
{
// Optional textures used to skin button states instead of chrome rendering.
struct UiButtonTextures
{
    SDL_Texture* idle = nullptr;
    SDL_Texture* focused = nullptr;
    SDL_Texture* pushed = nullptr;
    SDL_Texture* disabled = nullptr;
};

// Icon-only button content rendered from a caller-owned texture.
struct UiButtonIconContent
{
    SDL_Texture* texture = nullptr;
};

// Explicit texture set used when the button renders entirely from textures.
struct UiButtonTextureSetContent
{
    UiButtonTextures textures{};
};

using UiButtonContent = std::variant<
    std::monostate,
    UiTextContent,
    UiButtonIconContent,
    UiButtonTextureSetContent
>;

// Sound keys played as button focus, press, and click state change.
struct UiButtonSounds
{
    std::string focus;
    std::string press;
    std::string click;
};

// Visual styling for button chrome and text.
struct UiButtonStyle
{
    UiChromeStyle chrome{};
    UiEnabledDisabledColors text{};
};
struct UiButtonStyleOverrides { UiChromeStyleOverrides chrome{}; UiEnabledDisabledColorsOverrides text{}; };
template<> struct UiStyleOverrideTraits<UiButtonStyle> { using Overrides=UiButtonStyleOverrides; static bool empty(const Overrides& o) noexcept { return elysia::ui::empty(o.chrome)&&elysia::ui::empty(o.text); } static void apply(UiButtonStyle& s,const Overrides& o) noexcept { apply_ui_style_overrides(s.chrome,o.chrome); apply_ui_style_overrides(s.text,o.text); } };

// Bundles content, sound, and style overrides for button construction or updates.
struct UiButtonConfig
{
    UiButtonContent content{};
    std::optional<UiButtonSounds> sounds;
    std::optional<UiButtonStyleOverrides> style_overrides = std::nullopt;
};

class UiButton : public UiControl
{
public:
    enum class UiButtonVisualMode
    {
        None,
        Text,
        Textured,
        Icon
    };

public:
    using ClickCallback = std::function<void()>;

    explicit UiButton(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiButton(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiButton(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;

    UiButton(const elysia::core::Rect& rect,const UiButtonConfig& config,int order = 0) noexcept;
    UiButton(const elysia::core::Vector2& position,const elysia::core::Vector2& size,const UiButtonConfig& config,int order = 0) noexcept;
    UiButton(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,const UiButtonConfig& config,int order = 0) noexcept;

    ~UiButton() override = default;

    void cancel_input_interaction() noexcept override
    {
        clear_pushed_state();
    }
    void reset() noexcept override;

    void set_enabled(bool enabled) override;
    void set_focused(bool focused) override;

    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

    // Applies content, sounds, and style as one atomic button configuration update.
    void set_button_config(const UiButtonConfig& config);

    void set_text_content(UiTextContent text_content);
    [[nodiscard]] const UiTextContent& text_content() const noexcept;

    void set_state_textures(const UiButtonTextures& textures);
    void clear_state_textures();
    [[nodiscard]] bool has_state_textures() const noexcept;
    [[nodiscard]] UiButtonVisualMode visual_mode() const noexcept;

    void set_sounds(const UiButtonSounds& sounds);
    void clear_sounds();
    [[nodiscard]] const UiButtonSounds& sounds() const noexcept;

    void set_on_click(ClickCallback on_click);
    // Adds a callback that runs before the current click callback.
    void prepend_on_click(ClickCallback on_click);

    void set_base_style(const UiButtonStyle& style) noexcept;
    void set_style_overrides(const UiButtonStyleOverrides& overrides);
    [[nodiscard]] const UiButtonStyle& style() const noexcept;
    [[nodiscard]] const UiButtonStyleOverrides& style_overrides() const noexcept;
    [[nodiscard]] bool has_style_overrides() const noexcept;
    void clear_style_overrides() noexcept;

    void set_visual_role(UiButtonVisualRole role) noexcept;
    [[nodiscard]] UiButtonVisualRole visual_role() const noexcept;

    void set_typography_role(
        elysia::typography::UiTypographyRole role) noexcept;
    [[nodiscard]] elysia::typography::UiTypographyRole
        typography_role() const noexcept;
    void set_font_source_override(
        elysia::typography::FontSource source) noexcept;
    void clear_font_source_override() noexcept;
    [[nodiscard]] std::optional<elysia::typography::FontSource>
        font_source_override() const noexcept;

    void set_padding(int padding);
    [[nodiscard]] int padding() const noexcept;

private:
    // Applies the config payload without exposing intermediate visual states.
    void apply_button_config(const UiButtonConfig& config);
    // Switches between text, icon, textured, or empty content modes.
    void apply_button_content(const UiButtonContent& content);
    void set_icon_texture(SDL_Texture* texture) noexcept;
    // Clears the active content payload and resets the visual mode.
    void clear_content() noexcept;

    // Returns true only when the button should react to confirm or pointer input.
    [[nodiscard]] bool can_interact() const noexcept;
    [[nodiscard]] bool can_receive_pointer() const noexcept;
    [[nodiscard]] elysia::core::Color current_background_color() const noexcept;
    [[nodiscard]] elysia::core::Color current_border_color() const noexcept;
    [[nodiscard]] elysia::core::Color current_text_color() const noexcept;
    // Chooses the texture that matches the current button interaction state.
    [[nodiscard]] SDL_Texture* current_state_texture() const noexcept;
    // Returns the padded interior used to place text or icons.
    [[nodiscard]] elysia::core::Rect content_rect() const noexcept;
    // Fits rendered text into the padded button interior using the active alignment.
    [[nodiscard]] elysia::core::Rect text_render_rect(SDL_Texture* text_texture) const noexcept;
    [[nodiscard]] bool contains_pointer(int mouse_x,int mouse_y) const noexcept;
    [[nodiscard]] bool is_primary_pointer_event(const UiInputEvent& event) const noexcept;
    // Clears any pressed state left behind by focus loss or input cancellation.
    void clear_pushed_state() noexcept;
    // Plays a configured sound only when the corresponding key is present.
    void play_sound_if_set(std::string_view sound_key) const;

private:
    UiTextContent _text_content;
    UiButtonSounds _sounds;
    UiButtonTextures _state_textures;
    ClickCallback _on_click;
    UiButtonVisualMode _visual_mode = UiButtonVisualMode::None;
    UiStyleState<UiButtonStyle> _style_state;
    UiButtonVisualRole _visual_role = UiButtonVisualRole::Default;
    elysia::typography::UiTypographyRole _typography_role =
        elysia::typography::UiTypographyRole::Button;
    std::optional<elysia::typography::FontSource> _font_source_override;

    int _padding = 5;
    bool _is_pushed = false;
};
}
