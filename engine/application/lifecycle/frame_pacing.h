#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace elysia::application::detail
{
// elapsed() includes present/VSync. Only bounded slices are converted to ns.
template <typename Elapsed, typename Interrupted, typename Sleep>
void wait_for_frame(double fps, Elapsed elapsed, Interrupted interrupted, Sleep sleep)
{
    if (!std::isfinite(fps) || fps <= 0.0)
        return;
    const double budget = 1.0 / fps;
    for (;;)
    {
        // Do not add another event-pump cost after reaching the deadline.
        const double before_events = elapsed();
        if (!std::isfinite(before_events) || before_events < 0.0 || before_events >= budget)
            return;
        if (interrupted())
            return;
        // Pumping events can itself consume part of the remaining budget.
        const double spent = elapsed();
        if (!std::isfinite(spent) || spent < 0.0)
            return;
        const double remaining = budget - spent;
        if (remaining <= 0.0)
            return;
        const bool precise = remaining <= 0.001;
        const double slice = precise ? remaining : std::min(0.008,remaining - 0.001);
        const auto ns = static_cast<std::uint64_t>(slice * 1'000'000'000.0);
        sleep(std::max<std::uint64_t>(1,ns),precise);
    }
}
}
