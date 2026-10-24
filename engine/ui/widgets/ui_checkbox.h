#pragma once

#include "../../core/render/colors.h"
#include "../style/ui_style.h"
#include "../style/ui_interaction_style.h"
#include "../core/ui_control.h"

#include <functional>
#include <optional>
#include <string>

struct SDL_Texture;

namespace elysia::ui
{
enum class UiCheckboxState
{
    Unchecked,
    Checked,
    Indeterminate
};

enum class UiCheckboxMarkStyle
{
    Checkmark,
    FilledBox,
    RadioDot
};

// Optional textures for one checkbox visual state across interaction states.
struct UiCheckboxVisualStateTextures
{
    SDL_Texture* idle = nullptr;
    SDL_Texture* focused = nullptr;
    SDL_Texture* pushed = nullptr;
    SDL_Texture* disabled = nullptr;
};

// Complete texture set covering unchecked, checked, and indeterminate states.
struct UiCheckboxTextures
{
    UiCheckboxVisualStateTextures unchecked{};
    UiCheckboxVisualStateTextures checked{};
    UiCheckboxVisualStateTextures indeterminate{};
};

// Sound keys played as checkbox focus and toggle state change.
struct UiCheckboxSounds
{
    std::string focus;
    std::string press;
    std::string toggle;
};

// Visual styling for checkbox chrome and mark rendering.
struct UiCheckboxStyle
{
    UiChromeStyle chrome{};
    UiEnabledDisabledColors mark{};
    UiCheckboxMarkStyle mark_style = UiCheckboxMarkStyle::Checkmark;
};
struct UiCheckboxStyleOverrides { UiChromeStyleOverrides chrome{}; UiEnabledDisabledColorsOverrides mark{}; std::optional<UiCheckboxMarkStyle> mark_style; };
template<> struct UiStyleOverrideTraits<UiCheckboxStyle> { using Overrides=UiCheckboxStyleOverrides; static bool empty(const Overrides& o) noexcept { return elysia::ui::empty(o.chrome)&&elysia::ui::empty(o.mark)&&!o.mark_style; } static void apply(UiCheckboxStyle& s,const Overrides& o) noexcept { apply_ui_style_overrides(s.chrome,o.chrome); apply_ui_style_overrides(s.mark,o.mark); apply_ui_style_override(s.mark_style,o.mark_style); } };

// Bundles optional textures, sounds, and style overrides for a checkbox.
struct UiCheckboxConfig
{
    std::optional<UiCheckboxTextures> textures = std::nullopt;
    std::optional<UiCheckboxSounds> sounds = std::nullopt;
    std::optional<UiCheckboxStyleOverrides> style_overrides = std::nullopt;
};

using UiCheckboxToggledCallback = std::function<void(UiCheckboxState state)>;

class UiCheckbox : public UiControl
{
public:
    explicit UiCheckbox(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiCheckbox(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiCheckbox(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;

    UiCheckbox(const elysia::core::Rect& rect,const UiCheckboxConfig& config,int order = 0) noexcept;
    UiCheckbox(const elysia::core::Vector2& position,const elysia::core::Vector2& size,const UiCheckboxConfig& config,int order = 0) noexcept;
    UiCheckbox(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,const UiCheckboxConfig& config,int order = 0) noexcept;

    ~UiCheckbox() override = default;

    void cancel_input_interaction() noexcept override
    {
        clear_pushed_state();
    }
    void reset() noexcept override;

    void set_enabled(bool enabled) override;
    void set_focused(bool focused) override;

    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

    // Applies textures, sounds, and style as one checkbox configuration update.
    void set_checkbox_config(const UiCheckboxConfig& config);

    // Sets the tri-state value without requiring a pointer-style toggle.
    void set_state(UiCheckboxState state) noexcept;
    [[nodiscard]] UiCheckboxState state() const noexcept;
    void set_checked(bool checked) noexcept;
    [[nodiscard]] bool is_checked() const noexcept;
    [[nodiscard]] bool is_indeterminate() const noexcept;
    // Advances the checkbox through its supported toggle sequence.
    void toggle();

    void set_state_textures(const UiCheckboxTextures& textures);
    void clear_state_textures() noexcept;
    [[nodiscard]] const std::optional<UiCheckboxTextures>& state_textures() const noexcept;
    [[nodiscard]] bool has_complete_state_textures() const noexcept;

    void set_sounds(const UiCheckboxSounds& sounds);
    void clear_sounds() noexcept;
    [[nodiscard]] const std::optional<UiCheckboxSounds>& sounds() const noexcept;

    void set_on_toggled(UiCheckboxToggledCallback on_toggled);

    void set_base_style(const UiCheckboxStyle& style) noexcept;
    void set_style_overrides(const UiCheckboxStyleOverrides& overrides) noexcept;
    [[nodiscard]] const UiCheckboxStyle& style() const noexcept;
    [[nodiscard]] const UiCheckboxStyleOverrides& style_overrides() const noexcept;
    [[nodiscard]] bool has_style_overrides() const noexcept;
    void clear_style_overrides() noexcept;
    void set_mark_style(UiCheckboxMarkStyle mark_style) noexcept;
    [[nodiscard]] UiCheckboxMarkStyle mark_style() const noexcept;

    void set_padding(int padding) noexcept;
    [[nodiscard]] int padding() const noexcept;

private:
    // Applies the config payload without exposing intermediate visual states.
    void apply_checkbox_config(const UiCheckboxConfig& config);
    // Updates state and emits callbacks only when the value actually changes.
    [[nodiscard]] bool set_state_internal(UiCheckboxState state,bool notify);
    // Handles toggle transitions while optionally suppressing notifications or sounds.
    [[nodiscard]] bool toggle_internal(bool notify,bool play_toggle_sound);
    // Returns true only when the checkbox should react to confirm or pointer input.
    [[nodiscard]] bool can_interact() const noexcept;
    [[nodiscard]] bool can_receive_pointer() const noexcept;
    [[nodiscard]] bool contains_pointer(int mouse_x,int mouse_y) const noexcept;
    [[nodiscard]] bool is_primary_pointer_event(const UiInputEvent& event) const noexcept;
    // Clears any pressed state left behind by focus loss or input cancellation.
    void clear_pushed_state() noexcept;
    // Plays a configured sound only when the corresponding key is present.
    void play_sound_if_set(const std::string& sound_key) const;

protected:
    // Returns the padded interior that contains the clickable checkbox indicator.
    [[nodiscard]] elysia::core::Rect content_rect() const noexcept;
    // Returns the rect used to render and hit-test the checkbox indicator.
    [[nodiscard]] virtual elysia::core::Rect checkbox_rect() const noexcept;
    [[nodiscard]] elysia::core::Color current_background_color() const noexcept;
    [[nodiscard]] elysia::core::Color current_border_color() const noexcept;

private:
    // Chooses the texture bank for the current checkbox logical state.
    [[nodiscard]] const UiCheckboxVisualStateTextures* current_state_textures() const noexcept;
    // Chooses the texture that matches the current logical and interaction state.
    [[nodiscard]] SDL_Texture* current_state_texture() const noexcept;
    // Reports whether texture rendering should replace procedural checkbox drawing.
    [[nodiscard]] bool uses_texture_rendering() const noexcept;
    [[nodiscard]] elysia::core::Color current_checkmark_color() const noexcept;
    // Defines the next logical state reached by keyboard or pointer toggles.
    static UiCheckboxState toggled_state(UiCheckboxState state) noexcept;

private:
    std::optional<UiCheckboxTextures> _textures;
    std::optional<UiCheckboxSounds> _sounds;
    UiCheckboxToggledCallback _on_toggled;
    UiCheckboxState _state = UiCheckboxState::Unchecked;
    UiStyleState<UiCheckboxStyle> _style_state;
    int _padding = 4;
    bool _is_pushed = false;
};
}
