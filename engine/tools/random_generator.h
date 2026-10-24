#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <random>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace elysia::tools
{
class RandomGenerator
{
public:
    using Seed = std::uint64_t;

    static constexpr Seed TestSeedOne = 0x0BADC0FFEE0DDF00ULL;
    static constexpr Seed TestSeedTwo = 0x123456789ABCDEF0ULL;
    static constexpr Seed TestSeedThree = 0xC001D00DC0FFEE11ULL;

    RandomGenerator();
    explicit RandomGenerator(Seed seed);
    ~RandomGenerator() = default;

    RandomGenerator(const RandomGenerator&) = delete;
    RandomGenerator& operator=(const RandomGenerator&) = delete;
    RandomGenerator(RandomGenerator&&) noexcept = default;
    RandomGenerator& operator=(RandomGenerator&&) noexcept = default;

    void set_seed(Seed seed);
    void set_fixed_values(std::span<const std::uint64_t> values);
    void clear_fixed_values() noexcept;

    template <std::integral Integer>
    [[nodiscard]] Integer int_inclusive(Integer minimum, Integer maximum)
    {
        if (minimum > maximum)
            throw std::invalid_argument("Random integer minimum must not exceed maximum.");

        using Unsigned = std::make_unsigned_t<Integer>;
        const Unsigned lower = static_cast<Unsigned>(minimum);
        const Unsigned upper = static_cast<Unsigned>(maximum);
        const Unsigned span = static_cast<Unsigned>(upper - lower + Unsigned{ 1 });

        Unsigned offset = 0;
        if (span == 0)
        {
            offset = static_cast<Unsigned>(next_raw());
        }
        else
        {
            offset = static_cast<Unsigned>(uniform_below(static_cast<std::uint64_t>(span)));
        }

        return static_cast<Integer>(static_cast<Unsigned>(lower + offset));
    }

    [[nodiscard]] double real(double minimum, double maximum);
    [[nodiscard]] bool chance(double probability);

    template <std::ranges::random_access_range Range>
        requires std::ranges::sized_range<Range>
    [[nodiscard]] decltype(auto) pick(Range& values)
    {
        const std::size_t count = static_cast<std::size_t>(std::ranges::size(values));
        if (count == 0)
            throw std::invalid_argument("Cannot pick from an empty container.");

        const std::size_t index = int_inclusive<std::size_t>(0,count - 1);
        return *(std::ranges::begin(values) + static_cast<std::ranges::range_difference_t<Range>>(index));
    }

private:
    [[nodiscard]] static Seed make_entropy_seed();
    [[nodiscard]] std::uint64_t next_raw();
    [[nodiscard]] std::uint64_t uniform_below(std::uint64_t bound);

private:
    std::mt19937_64 _engine;
    std::vector<std::uint64_t> _fixed_values;
    std::size_t _fixed_value_index = 0;
};
}
