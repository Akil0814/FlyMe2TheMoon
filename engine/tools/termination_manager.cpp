#include "termination_manager.h"

#include <algorithm>
#include <thread>

namespace elysia::tools
{
void TerminationManager::initialize_lifecycle() noexcept
{
    bool expected = false;
    if (!_lifecycle_initialized.compare_exchange_strong(
        expected,
        true,
        std::memory_order_acq_rel,
        std::memory_order_acquire))
    {
        return;
    }
    reset();
}

void TerminationManager::reset_for_testing() noexcept
{
    reset();
}

void TerminationManager::request_termination(
    TerminationReason reason,
    std::string_view category,
    std::string_view message,
    std::source_location location
) noexcept
{
    State expected = State::Accepting;
    if (!_state.compare_exchange_strong(expected,State::Publishing,
        std::memory_order_acq_rel,std::memory_order_acquire))
    {
        return;
    }

    _record.reason = reason;
    _record.location = location;
    _record.category_size = copy_text(category,_record.category.data(),_record.category.size(),
        _record.category_truncated);
    _record.message_size = copy_text(message,_record.message.data(),_record.message.size(),
        _record.message_truncated);

    _state.store(State::Published,std::memory_order_release);
    _state.notify_all();
}

bool TerminationManager::termination_requested() const noexcept
{
    return _state.load(std::memory_order_acquire) == State::Published;
}

std::optional<TerminationInfo> TerminationManager::termination_info() const noexcept
{
    if (!termination_requested())
        return std::nullopt;

    return TerminationInfo{
        _record.reason,
        std::string_view(_record.category.data(),_record.category_size),
        std::string_view(_record.message.data(),_record.message_size),
        _record.location,
        _record.category_truncated,
        _record.message_truncated
    };
}

bool TerminationManager::seal_for_shutdown() noexcept
{
    State state = _state.load(std::memory_order_acquire);
    for (;;)
    {
        switch (state)
        {
        case State::Published:
            return true;

        case State::Closed:
            return false;

        case State::Accepting:
            if (_state.compare_exchange_weak(state,State::Closed,
                std::memory_order_acq_rel,std::memory_order_acquire))
            {
                return false;
            }
            break;

        case State::Publishing:
            _state.wait(State::Publishing,std::memory_order_acquire);
            state = _state.load(std::memory_order_acquire);
            break;
        }
    }
}

void TerminationManager::reset() noexcept
{
    _state.store(State::Publishing,std::memory_order_release);
    _record = {};
    _state.store(State::Accepting,std::memory_order_release);
    _state.notify_all();
}

std::size_t TerminationManager::copy_text(std::string_view source,char* destination,
    std::size_t capacity,bool& truncated) noexcept
{
    const std::size_t copied_size = capacity == 0 ? 0 : std::min(source.size(),capacity - 1);
    truncated = copied_size < source.size();
    if (copied_size > 0)
        std::copy_n(source.data(),copied_size,destination);
    if (capacity > 0)
        destination[copied_size] = '\0';
    constexpr std::string_view marker = " [truncated]";
    if (truncated && copied_size >= marker.size())
        std::copy(marker.begin(),marker.end(),destination + copied_size - marker.size());
    return copied_size;
}
}
