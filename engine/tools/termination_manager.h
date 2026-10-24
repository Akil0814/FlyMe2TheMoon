#pragma once

#include "singleton.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <source_location>
#include <string_view>

namespace elysia::tools
{
enum class TerminationReason
{
    None,
    FatalRuntimeFailure,
    UnhandledException
};

struct TerminationInfo
{
    TerminationReason reason{ TerminationReason::None };
    std::string_view category;
    std::string_view message;
    std::source_location location{};
    bool category_truncated = false;
    bool message_truncated = false;
};

class TerminationManager : public Singleton<TerminationManager>
{
    friend class Singleton<TerminationManager>;

public:
    void initialize_lifecycle() noexcept;
    void reset_for_testing() noexcept;

    void request_termination(
        TerminationReason reason,
        std::string_view category,
        std::string_view message,
        std::source_location location = std::source_location::current()
    ) noexcept;

    [[nodiscard]] bool termination_requested() const noexcept;
    [[nodiscard]] std::optional<TerminationInfo> termination_info() const noexcept;

    // Stops accepting requests and returns true only when a complete request was published.
    [[nodiscard]] bool seal_for_shutdown() noexcept;

private:
    static constexpr std::size_t category_capacity = 64;
    static constexpr std::size_t message_capacity = 1024;

    enum class State : unsigned char
    {
        Accepting,
        Publishing,
        Published,
        Closed
    };

    struct Record
    {
        TerminationReason reason{ TerminationReason::None };
        std::array<char,category_capacity> category{};
        std::array<char,message_capacity> message{};
        std::size_t category_size = 0;
        std::size_t message_size = 0;
        std::source_location location{};
        bool category_truncated = false;
        bool message_truncated = false;
    };

    TerminationManager() = default;

    void reset() noexcept;
    static std::size_t copy_text(std::string_view source,char* destination,std::size_t capacity,
        bool& truncated) noexcept;

private:
    std::atomic<State> _state{ State::Accepting };
    std::atomic<bool> _lifecycle_initialized = false;
    Record _record;
};
}
