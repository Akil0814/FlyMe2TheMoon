#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace elysia::audio
{
enum class SoundGroup : std::uint8_t
{
    Ui,
    Gameplay,
    Ambient,
    Extra,
    Count,
};

inline constexpr std::size_t kSoundGroupCount = static_cast<std::size_t>(SoundGroup::Count);
inline constexpr std::size_t kSoundChannelCount = 24;
inline constexpr std::array<std::size_t,kSoundGroupCount> kSoundGroupHardLimits{ 4,12,4,4 };

[[nodiscard]] constexpr std::size_t sound_group_index(SoundGroup group) noexcept
{
    return static_cast<std::size_t>(group);
}

[[nodiscard]] constexpr std::size_t sound_group_hard_limit(SoundGroup group) noexcept
{
    return kSoundGroupHardLimits[sound_group_index(group)];
}

enum class SoundOverflowPolicy
{
    IgnoreNew,
    ReplaceOldest,
};

struct SoundGroupConfig
{
    // nullopt uses the fixed maximum assigned to this group.
    std::optional<std::size_t> max_simultaneous = std::nullopt;
    // Applied per sound key after a successful start.
    std::chrono::milliseconds cooldown{ 0 };
    SoundOverflowPolicy overflow_policy = SoundOverflowPolicy::IgnoreNew;
};

struct SoundPlayOptions
{
    // nullopt is one-shot playback (SDL_mixer loops = 0).
    std::optional<int> loops = std::nullopt;
    SoundGroup group = SoundGroup::Extra;
    std::chrono::milliseconds start_delay{ 0 };
    std::chrono::milliseconds fade_in{ 0 };
};

using SoundHandle = std::uint64_t;

enum class SoundRequestStatus
{
    Started,
    Scheduled,
    Rejected,
};

struct SoundRequestResult
{
    SoundRequestStatus status = SoundRequestStatus::Rejected;
    std::optional<SoundHandle> handle = std::nullopt;
};
}
