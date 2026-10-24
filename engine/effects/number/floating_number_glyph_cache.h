#pragma once

#include "../../core/geometry/vector2.h"
#include "../../core/render/color.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace elysia::effects
{
enum class FloatingNumberColor : std::size_t
{
    White,
    Black,
    Yellow,
    Green,
    Red,
    Blue,
    LightBlue,
    Orange,
    Purple,
    Count
};

using FloatingNumberTexturePtr = std::shared_ptr<SDL_Texture>;

struct FloatingNumberGlyph
{
    FloatingNumberTexturePtr texture;
    elysia::core::Vector2 source_size;
};

class FloatingNumberGlyphCache
{
public:
    static constexpr std::size_t kColorCount = static_cast<std::size_t>(FloatingNumberColor::Count);

    [[nodiscard]] bool configure(
        SDL_Renderer* renderer,
        TTF_Font* font,
        std::uint64_t font_generation) noexcept;
    [[nodiscard]] std::optional<FloatingNumberGlyph> glyph(FloatingNumberColor color, char ch);
    [[nodiscard]] std::optional<std::vector<FloatingNumberGlyph>> resolve(
        std::string_view text,
        FloatingNumberColor color
    );
    [[nodiscard]] static bool supports(char ch) noexcept;
    void reset() noexcept;

private:
    [[nodiscard]] static constexpr std::size_t color_index(FloatingNumberColor color) noexcept
    {
        return static_cast<std::size_t>(color);
    }

    [[nodiscard]] static elysia::core::Color color_value(FloatingNumberColor color) noexcept;
    [[nodiscard]] std::optional<FloatingNumberGlyph> create_glyph(FloatingNumberColor color, char ch) const;

private:
    std::array<std::unordered_map<char,FloatingNumberGlyph>,kColorCount> _glyphs;
    SDL_Renderer* _renderer = nullptr;
    TTF_Font* _font = nullptr;
    std::uint64_t _font_generation = 0;
};
}
