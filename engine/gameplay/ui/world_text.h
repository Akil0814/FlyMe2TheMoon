#pragma once

#include "../../core/render/render_command.h"
#include "../../localization/text_texture_cache.h"
#include "../../ui/text/ui_text_content.h"

#include <optional>

namespace elysia::gameplay::ui
{
enum class TextColorMode { Tint, Baked };

class WorldText
{
public:
    void set_text_content(elysia::ui::UiTextContent content);
    void set_text_key(std::string key);
    void set_raw_text(std::string text);
    [[nodiscard]] const elysia::ui::UiTextContent& text_content() const noexcept { return _content; }
    void set_typography_role(elysia::typography::UiTypographyRole role) noexcept { _role = role; }
    void set_font_source_override(std::optional<elysia::typography::FontSource> source) noexcept { _font_source = source; }
    void set_world_units_per_pixel(float scale) noexcept;
    void set_max_width(float world_width) noexcept;
    void set_color(elysia::core::Color color) noexcept { _color = color; }
    void set_color_mode(TextColorMode mode) noexcept { _color_mode = mode; }
    void set_visible(bool visible) noexcept { _visible = visible; }
    [[nodiscard]] bool is_visible() const noexcept { return _visible; }
    [[nodiscard]] elysia::core::Vector2 content_size() const;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands,
        elysia::core::Vector2 position, elysia::core::Vector2 anchor = {}) const;

private:
    [[nodiscard]] SDL_Texture* resolve_texture() const;
    [[nodiscard]] elysia::core::Vector2 texture_world_size(SDL_Texture* texture) const;

    elysia::ui::UiTextContent _content;
    elysia::typography::UiTypographyRole _role = elysia::typography::UiTypographyRole::Label;
    std::optional<elysia::typography::FontSource> _font_source;
    float _world_units_per_pixel = 1.0f;
    float _max_width = 0.0f;
    elysia::core::Color _color{};
    TextColorMode _color_mode = TextColorMode::Tint;
    bool _visible = true;
    mutable elysia::localization::CachedTexturePtr _raw_texture;
    mutable std::optional<elysia::localization::TextTextureCacheKey> _raw_key;
};
}
