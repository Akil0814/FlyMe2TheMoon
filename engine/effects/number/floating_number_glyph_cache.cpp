#include "floating_number_glyph_cache.h"

#include "../../core/render/color.h"
#include "../../core/render/sdl_convert.h"

#include <utility>

namespace elysia::effects
{
bool FloatingNumberGlyphCache::configure(
    SDL_Renderer* renderer,
    TTF_Font* font,
    std::uint64_t font_generation) noexcept
{
    if (!renderer || !font)
    {
        reset();
        return false;
    }

    if (_renderer == renderer
        && _font == font
        && _font_generation == font_generation)
        return true;

    reset();
    _renderer = renderer;
    _font = font;
    _font_generation = font_generation;
    return true;
}

std::optional<FloatingNumberGlyph> FloatingNumberGlyphCache::glyph(
    FloatingNumberColor color,
    char ch
)
{
    const std::size_t index = color_index(color);
    if (!_renderer || !_font || index >= _glyphs.size() || !supports(ch))
        return std::nullopt;

    const auto found = _glyphs[index].find(ch);
    if (found != _glyphs[index].end())
        return found->second;

    std::optional<FloatingNumberGlyph> created = create_glyph(color,ch);
    if (!created.has_value())
        return std::nullopt;

    _glyphs[index].emplace(ch,*created);
    return created;
}

std::optional<std::vector<FloatingNumberGlyph>> FloatingNumberGlyphCache::resolve(
    std::string_view text,
    FloatingNumberColor color
)
{
    std::vector<FloatingNumberGlyph> result;
    result.reserve(text.size());
    for (const char ch : text)
    {
        std::optional<FloatingNumberGlyph> resolved = glyph(color,ch);
        if (!resolved.has_value())
            return std::nullopt;
        result.push_back(std::move(*resolved));
    }
    return result;
}

bool FloatingNumberGlyphCache::supports(char ch) noexcept
{
    return (ch >= '0' && ch <= '9')
        || ch == '-'
        || ch == '.'
        || ch == '/'
        || ch == '%';
}

void FloatingNumberGlyphCache::reset() noexcept
{
    for (auto& color_glyphs : _glyphs)
        color_glyphs.clear();
    _renderer = nullptr;
    _font = nullptr;
    _font_generation = 0;
}

elysia::core::Color FloatingNumberGlyphCache::color_value(FloatingNumberColor color) noexcept
{
    switch (color)
    {
    case FloatingNumberColor::White: return { 255,255,255 };
    case FloatingNumberColor::Black: return { 0,0,0 };
    case FloatingNumberColor::Yellow: return { 255,220,48 };
    case FloatingNumberColor::Green: return { 80,220,100 };
    case FloatingNumberColor::Red: return { 230,70,70 };
    case FloatingNumberColor::Blue: return { 70,130,255 };
    case FloatingNumberColor::LightBlue: return { 110,220,255 };
    case FloatingNumberColor::Orange: return { 255,145,45 };
    case FloatingNumberColor::Purple: return { 185,105,255 };
    case FloatingNumberColor::Count: break;
    }
    return {};
}

std::optional<FloatingNumberGlyph> FloatingNumberGlyphCache::create_glyph(
    FloatingNumberColor color,
    char ch
) const
{
    const char glyph_text[2] = { ch,'\0' };
    const SDL_Color text_color = elysia::core::to_sdl_color(color_value(color));
    SDL_Surface* surface = TTF_RenderText_Blended(_font, glyph_text, 0, text_color);
    if (!surface)
        return std::nullopt;

    const elysia::core::Vector2 source_size(
        static_cast<float>(surface->w),
        static_cast<float>(surface->h)
    );
    SDL_Texture* texture = SDL_CreateTextureFromSurface(_renderer,surface);
    SDL_DestroySurface(surface);
    if (!texture || source_size.x <= 0.0f || source_size.y <= 0.0f)
    {
        if (texture)
            SDL_DestroyTexture(texture);
        return std::nullopt;
    }

    return FloatingNumberGlyph{
        FloatingNumberTexturePtr(texture,SDL_DestroyTexture),
        source_size
    };
}
}
