#pragma once

#include <cstdint>

namespace elysia::core
{
struct Color
{
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;

    constexpr Color() noexcept = default;

    constexpr Color(std::uint8_t red,std::uint8_t green,std::uint8_t blue,std::uint8_t alpha = 255) noexcept
        : r(red),g(green),b(blue),a(alpha){}

    [[nodiscard]] constexpr bool operator==(const Color& other) const noexcept = default;
};

}
