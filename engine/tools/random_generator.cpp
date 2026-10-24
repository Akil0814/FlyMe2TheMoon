#include "random_generator.h"

#include <cmath>
#include <limits>

namespace elysia::tools
{
RandomGenerator::RandomGenerator()
    : _engine(make_entropy_seed())
{
}

RandomGenerator::RandomGenerator(Seed seed)
    : _engine(seed)
{
}

void RandomGenerator::set_seed(Seed seed)
{
    _engine.seed(seed);
    clear_fixed_values();
}

void RandomGenerator::set_fixed_values(std::span<const std::uint64_t> values)
{
    _fixed_values.assign(values.begin(),values.end());
    _fixed_value_index = 0;
}

void RandomGenerator::clear_fixed_values() noexcept
{
    _fixed_values.clear();
    _fixed_value_index = 0;
}

double RandomGenerator::real(double minimum,double maximum)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum >= maximum)
        throw std::invalid_argument("Random real minimum must be finite and less than maximum.");

    constexpr double inverse_two_to_53 = 1.0 / 9007199254740992.0;
    const double unit = static_cast<double>(next_raw() >> 11) * inverse_two_to_53;
    const double value = minimum + (maximum - minimum) * unit;
    return value < maximum ? value : std::nextafter(maximum,minimum);
}

bool RandomGenerator::chance(double probability)
{
    if (!std::isfinite(probability) || probability < 0.0 || probability > 1.0)
        throw std::invalid_argument("Random chance probability must be between zero and one.");

    if (probability == 0.0)
        return false;
    if (probability == 1.0)
        return true;

    return real(0.0,1.0) < probability;
}

RandomGenerator::Seed RandomGenerator::make_entropy_seed()
{
    std::random_device device;
    return (static_cast<Seed>(device()) << 32) ^ static_cast<Seed>(device());
}

std::uint64_t RandomGenerator::next_raw()
{
    if (_fixed_value_index < _fixed_values.size())
        return _fixed_values[_fixed_value_index++];

    return _engine();
}

std::uint64_t RandomGenerator::uniform_below(std::uint64_t bound)
{
    const std::uint64_t threshold = -bound % bound;

    std::uint64_t value = 0;
    do
    {
        value = next_raw();
    }
    while (value < threshold);

    return value % bound;
}
}
