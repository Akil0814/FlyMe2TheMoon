#pragma once

#include <cstddef>
#include <cstdint>

namespace elysia::core
{
enum class DepthLayer
{
    Background,
    Terrain,
    EffectBack,
    Item,
    Character,
    EffectFront,
    Foreground,
    Count
};

class DepthLayerMask
{
public:
    using Storage = std::uint32_t;

    constexpr DepthLayerMask() noexcept = default;
    constexpr DepthLayerMask(DepthLayer layer) noexcept
        : _bits(bit_for(layer))
    {
    }

    [[nodiscard]] static constexpr DepthLayerMask none() noexcept
    {
        return {};
    }

    [[nodiscard]] static constexpr DepthLayerMask all() noexcept
    {
        return DepthLayerMask(valid_bits(),RawBitsTag{});
    }

    [[nodiscard]] constexpr bool contains(DepthLayer layer) const noexcept
    {
        const Storage bit = bit_for(layer);
        return bit != 0 && (_bits & bit) != 0;
    }

    friend constexpr DepthLayerMask operator|(
        DepthLayerMask first,
        DepthLayerMask second) noexcept
    {
        return DepthLayerMask(first._bits | second._bits,RawBitsTag{});
    }

    constexpr DepthLayerMask& operator|=(DepthLayerMask other) noexcept
    {
        _bits = (_bits | other._bits) & valid_bits();
        return *this;
    }

private:
    struct RawBitsTag {};

    constexpr DepthLayerMask(Storage bits,RawBitsTag) noexcept
        : _bits(bits & valid_bits())
    {
    }

    [[nodiscard]] static constexpr Storage valid_bits() noexcept
    {
        return (Storage{1} << static_cast<std::size_t>(DepthLayer::Count)) - 1u;
    }

    [[nodiscard]] static constexpr Storage bit_for(DepthLayer layer) noexcept
    {
        const std::size_t index = static_cast<std::size_t>(layer);
        return index < static_cast<std::size_t>(DepthLayer::Count)
            ? Storage{1} << index
            : Storage{0};
    }

    Storage _bits = 0;
};

[[nodiscard]] constexpr DepthLayerMask operator|(
    DepthLayer first,
    DepthLayer second) noexcept
{
    return DepthLayerMask(first) | DepthLayerMask(second);
}

[[nodiscard]] constexpr DepthLayerMask operator|(
    DepthLayerMask first,
    DepthLayer second) noexcept
{
    return first | DepthLayerMask(second);
}

[[nodiscard]] constexpr DepthLayerMask operator|(
    DepthLayer first,
    DepthLayerMask second) noexcept
{
    return DepthLayerMask(first) | second;
}

}
