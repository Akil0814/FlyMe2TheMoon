#include "world_text.h"

#include "../../core/render/sdl_texture_size.h"
#include "../../localization/localization_service.h"

#include <cmath>
#include <limits>

namespace elysia::gameplay::ui
{
void WorldText::set_text_content(elysia::ui::UiTextContent content)
{
    if (_content.kind == content.kind && _content.value == content.value)
        return;
    _content = std::move(content);
    if (_content.empty() || _content.kind != elysia::ui::UiTextContentKind::RawText)
    {
        _raw_texture.reset();
        _raw_key.reset();
    }
}

void WorldText::set_text_key(std::string key)
{
    set_text_content(elysia::ui::ui_text_key(std::move(key)));
}

void WorldText::set_raw_text(std::string text)
{
    set_text_content(elysia::ui::ui_raw_text(std::move(text)));
}

void WorldText::set_world_units_per_pixel(float scale) noexcept
{
    if (std::isfinite(scale) && scale > 0.0f)
        _world_units_per_pixel = scale;
}

void WorldText::set_max_width(float world_width) noexcept
{
    if (std::isfinite(world_width))
        _max_width = std::max(0.0f, world_width);
}

SDL_Texture* WorldText::resolve_texture() const
{
    if (_content.empty())
        return nullptr;
    auto* service = ELYSIA_LOCALIZATION;
    elysia::localization::LocalizedTextStyle style;
    style.typography_role = _role;
    style.font_source_override = _font_source;
    style.color = _color_mode == TextColorMode::Baked
        ? elysia::core::Color{_color.r, _color.g, _color.b, 255}
        : elysia::core::Color{255, 255, 255, 255};
    if (_max_width > 0.0f)
    {
        const double pixel_width = std::floor(static_cast<double>(_max_width) / _world_units_per_pixel);
        style.wrap_width = static_cast<int>(std::clamp(pixel_width, 1.0,
            static_cast<double>(std::numeric_limits<int>::max())));
    }
    if (_content.kind == elysia::ui::UiTextContentKind::TextKey)
        return service->get_text_texture(_content.value, style);
    if (_content.kind != elysia::ui::UiTextContentKind::RawText)
        return nullptr;

    elysia::localization::TextTextureCacheKey key;
    key.language = service->current_language();
    key.translation_key = _content.value;
    key.is_raw_text = true;
    key.typography_role = _role;
    key.font_source_override = _font_source;
    key.font_generation = service->font_generation();
    key.color = style.color;
    key.wrap_width = style.wrap_width;
    if (!_raw_texture || !_raw_key || !(*_raw_key == key))
    {
        auto texture = service->create_uncached_raw_text_texture(_content.value, style);
        _raw_texture = std::move(texture);
        _raw_key = std::move(key);
    }
    return _raw_texture.get();
}

elysia::core::Vector2 WorldText::texture_world_size(SDL_Texture* texture) const
{
    int width = 0;
    int height = 0;
    if (!texture || !elysia::core::texture_pixel_size(texture, &width, &height))
        return {};
    const elysia::core::Vector2 size{width * _world_units_per_pixel, height * _world_units_per_pixel};
    return std::isfinite(size.x) && std::isfinite(size.y) ? size : elysia::core::Vector2{};
}

elysia::core::Vector2 WorldText::content_size() const
{
    return texture_world_size(resolve_texture());
}

void WorldText::submit_render_commands(std::vector<elysia::core::RenderCommand>& commands,
    elysia::core::Vector2 position, elysia::core::Vector2 anchor) const
{
    if (!_visible || _color.a == 0 || !std::isfinite(position.x) || !std::isfinite(position.y)
        || !std::isfinite(anchor.x) || !std::isfinite(anchor.y))
        return;
    auto* texture = resolve_texture();
    const auto size = texture_world_size(texture);
    if (size.x <= 0.0f || size.y <= 0.0f)
        return;
    elysia::core::RenderCommand command;
    command.texture = texture;
    command.command_rect = {position.x - size.x * anchor.x, position.y - size.y * anchor.y, size.x, size.y};
    command.alpha = _color.a;
    if (_color_mode == TextColorMode::Tint)
        command.texture_color_modulation = elysia::core::TextureColorModulation{_color.r, _color.g, _color.b};
    commands.push_back(command);
}
}
