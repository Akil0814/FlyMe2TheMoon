#pragma once

#include "../tools/singleton.h"

#include <cstddef>

#define ELYSIA_TIME (::elysia::core::Time::instance())

namespace elysia::core
{
class Time : public elysia::tools::Singleton<Time>
{
    friend elysia::tools::Singleton<Time>;

public:
    void begin_frame(double raw_delta_seconds);
    void reset();
    void set_time_scale(double scale);

    [[nodiscard]] double raw_delta() const;
    [[nodiscard]] double delta() const;
    [[nodiscard]] double time_scale() const;
    [[nodiscard]] double total_time() const;
    [[nodiscard]] double scaled_total_time() const;
    [[nodiscard]] std::size_t frame_count() const;

private:
    Time() = default;

private:
    double _raw_delta_seconds = 0.0;
    double _scaled_delta_seconds = 0.0;
    double _time_scale = 1.0;
    double _total_time_seconds = 0.0;
    double _scaled_total_time_seconds = 0.0;
    std::size_t _frame_count = 0;
};

}
