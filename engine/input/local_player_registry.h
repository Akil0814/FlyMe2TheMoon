#pragma once
#include "input_snapshot.h"
#include <map>
#include <set>
#include <string>
#include <expected>
#include <functional>
#include <utility>
#include <limits>
namespace elysia::gameplay { class ControllerManager; }
namespace elysia::input
{
struct KeyboardPartition
{
    KeyboardPartitionId id;
    std::string name;
    std::set<RawInputControl> keys;
    auto operator<=>(const KeyboardPartition &) const = default;
};
struct PlayerDeviceBinding
{
    KeyboardPartitionId keyboard;
    bool mouse = false;
    InputSourceId gamepad;
    auto operator<=>(const PlayerDeviceBinding &) const = default;
};
struct PlayerInputConfiguration
{
    std::map<KeyboardPartitionId, KeyboardPartition> partitions;
    std::map<LocalPlayerId, PlayerDeviceBinding> bindings;
};
enum class InputBindingError
{
    InvalidPlayer,
    InvalidPartition,
    InvalidSource,
    Overlap,
    InUse,
    InvalidMap,
    PartitionIdsExhausted
};
class LocalPlayerRegistry
{
  public:
    LocalPlayerRegistry()
    {
        build_defaults();
    }
    void clear_for_shutdown() noexcept
    {
        _players.clear();
        _configuration.partitions.clear();
        _configuration.bindings.clear();
        _versions.clear();
        _required_keys.clear();
        _next = 1;
        _next_partition = 1;
        _revision = 0;
    }
    void swap_state(LocalPlayerRegistry& other) noexcept
    {
        using std::swap;
        swap(_players, other._players);
        swap(_configuration, other._configuration);
        swap(_versions, other._versions);
        swap(_required_keys, other._required_keys);
        swap(_next, other._next);
        swap(_next_partition, other._next_partition);
        swap(_revision, other._revision);
    }
    void reset_defaults()
    {
        LocalPlayerRegistry prepared(EmptyTag{});
        prepared.build_defaults();
        swap_state(prepared);
    }
    LocalPlayerId create_player()
    {
        LocalPlayerId id{_next};
        auto players = _players;
        auto bindings = _configuration.bindings;
        auto versions = _versions;
        players.push_back(id);
        bindings[id] = {};
        ++versions[id];
        _players.swap(players);
        _configuration.bindings.swap(bindings);
        _versions.swap(versions);
        ++_next;
        ++_revision;
        return id;
    }
  private:
    struct EmptyTag {};
    explicit LocalPlayerRegistry(EmptyTag) noexcept {}
    void build_defaults()
    {
        auto player = create_player();
        std::set<RawInputControl> keys;
        for (int i = 1; i < int(RawInputControl::Count); ++i)
            if (is_keyboard_control(static_cast<RawInputControl>(i)))
                keys.insert(static_cast<RawInputControl>(i));
        auto partition = create_partition("Full keyboard", std::move(keys));
        _configuration.bindings[player] = {*partition, true, {}};
        ++_versions[player];
    }
  public:
    bool contains(LocalPlayerId id) const
    {
        return std::ranges::find(_players, id) != _players.end();
    }
    bool remove_player(LocalPlayerId id)
    {
        if (id == PrimaryLocalPlayer || !contains(id))
            return false;
        std::erase(_players, id);
        _configuration.bindings.erase(id);
        _required_keys.erase(id);
        ++_versions[id];
        ++_revision;
        return true;
    }
    bool accepts_keyboard_map(LocalPlayerId player, const std::set<RawInputControl> &keys) const
    {
        auto it = _configuration.bindings.find(player);
        if (it == _configuration.bindings.end() || !it->second.keyboard.value)
            return true;
        const auto &allowed = _configuration.partitions.at(it->second.keyboard).keys;
        return std::ranges::all_of(keys, [&](auto key) { return allowed.contains(key); });
    }
  private:
    friend class elysia::gameplay::ControllerManager;
    void set_keyboard_requirement(LocalPlayerId player, std::set<RawInputControl> keys)
    {
        _required_keys[player] = std::move(keys);
    }
    void clear_keyboard_requirement(LocalPlayerId player)
    {
        _required_keys.erase(player);
    }
  public:
    const PlayerInputConfiguration &configuration() const
    {
        return _configuration;
    }
    std::expected<void, InputBindingError> apply_configuration(PlayerInputConfiguration next)
    {
        std::set<RawInputControl> used;
        std::set<InputSourceId> pads;
        bool mouse = false;
        for (const auto &[id, p] : next.partitions)
        {
            if (!id.value || p.id != id || p.keys.empty())
                return std::unexpected(InputBindingError::InvalidPartition);
            for (auto key : p.keys)
                if (!is_keyboard_control(key))
                    return std::unexpected(InputBindingError::InvalidPartition);
        }
        for (auto &[player, binding] : next.bindings)
        {
            if (!contains(player))
                return std::unexpected(InputBindingError::InvalidPlayer);
            if (binding.keyboard.value)
            {
                auto p = next.partitions.find(binding.keyboard);
                if (p == next.partitions.end())
                    return std::unexpected(InputBindingError::InvalidPartition);
                if (auto required = _required_keys.find(player); required != _required_keys.end())
                    for (auto key : required->second)
                        if (!p->second.keys.contains(key))
                            return std::unexpected(InputBindingError::InvalidMap);
                for (auto key : p->second.keys)
                    if (!used.insert(key).second)
                        return std::unexpected(InputBindingError::Overlap);
            }
            if (binding.mouse && std::exchange(mouse, true))
                return std::unexpected(InputBindingError::Overlap);
            if (binding.gamepad.value &&
                (!binding.gamepad.is_gamepad() || !valid_source(binding.gamepad) || !pads.insert(binding.gamepad).second))
                return std::unexpected(InputBindingError::InvalidSource);
        }
        auto versions = _versions;
        for (auto player : _players)
        {
            auto &binding = next.bindings[player];
            auto old = _configuration.bindings[player];
            bool changed = old != binding;
            if (binding.keyboard.value && old.keyboard == binding.keyboard)
                changed = changed ||
                          _configuration.partitions.at(old.keyboard) != next.partitions.at(binding.keyboard);
            if (changed)
                ++versions[player];
        }
        _configuration = std::move(next);
        _versions.swap(versions);
        ++_revision;
        return {};
    }
    std::expected<KeyboardPartitionId, InputBindingError> create_partition(std::string name,
                                                                           std::set<RawInputControl> keys)
    {
        const std::uint64_t maximum_id = std::numeric_limits<std::uint64_t>::max();
        if (_next_partition == 0)
            return std::unexpected(InputBindingError::PartitionIdsExhausted);
        KeyboardPartitionId id{_next_partition};
        auto next = _configuration;
        while (next.partitions.contains(id))
        {
            if (id.value == maximum_id)
                return std::unexpected(InputBindingError::PartitionIdsExhausted);
            ++id.value;
        }
        next.partitions.emplace(id, KeyboardPartition{id, std::move(name), std::move(keys)});
        auto result = apply_configuration(std::move(next));
        if (!result)
            return std::unexpected(result.error());
        _next_partition = id.value == maximum_id ? 0 : id.value + 1;
        return id;
    }
    std::expected<void, InputBindingError> update_partition(KeyboardPartition partition)
    {
        if (!_configuration.partitions.contains(partition.id))
            return std::unexpected(InputBindingError::InvalidPartition);
        auto next = _configuration;
        next.partitions[partition.id] = std::move(partition);
        return apply_configuration(std::move(next));
    }
    std::expected<void, InputBindingError> remove_partition(KeyboardPartitionId id)
    {
        for (auto &[player, binding] : _configuration.bindings)
            if (binding.keyboard == id)
                return std::unexpected(InputBindingError::InUse);
        auto next = _configuration;
        if (!next.partitions.erase(id))
            return std::unexpected(InputBindingError::InvalidPartition);
        return apply_configuration(std::move(next));
    }
    std::expected<void, InputBindingError> bind_keyboard(LocalPlayerId player, KeyboardPartitionId id)
    {
        if (!contains(player))
            return std::unexpected(InputBindingError::InvalidPlayer);
        auto next = _configuration;
        next.bindings[player].keyboard = id;
        return apply_configuration(std::move(next));
    }
    std::expected<void, InputBindingError> bind_source(LocalPlayerId player, InputSourceId source)
    {
        if (!contains(player)) return std::unexpected(InputBindingError::InvalidPlayer);
        if (!valid_source(source)) return std::unexpected(InputBindingError::InvalidSource);
        if (owner(source).value && owner(source) != player) return std::unexpected(InputBindingError::InUse);
        auto next = _configuration;
        auto &binding = next.bindings[player];
        if (source.is_mouse()) binding.mouse = true;
        else if (!binding.gamepad.value || binding.gamepad == source) binding.gamepad = source;
        else return std::unexpected(InputBindingError::InUse);
        return apply_configuration(std::move(next));
    }
    std::expected<void, InputBindingError> unbind_source(InputSourceId source)
    {
        if (!valid_source(source)) return std::unexpected(InputBindingError::InvalidSource);
        auto player = owner(source);
        if (!player.value) return {};
        auto next = _configuration;
        if (source.is_mouse()) next.bindings[player].mouse = false;
        else next.bindings[player].gamepad = {};
        return apply_configuration(std::move(next));
    }
    std::expected<void, InputBindingError> transfer_source(LocalPlayerId player, InputSourceId source)
    {
        if (!contains(player)) return std::unexpected(InputBindingError::InvalidPlayer);
        if (!valid_source(source)) return std::unexpected(InputBindingError::InvalidSource);
        auto next = _configuration;
        for (auto &[id, b] : next.bindings)
            if (source.is_mouse()) b.mouse = false;
            else if (b.gamepad == source) b.gamepad = {};
        if (source.is_mouse()) next.bindings[player].mouse = true;
        else next.bindings[player].gamepad = source;
        return apply_configuration(std::move(next));
    }
    std::expected<void, InputBindingError> replace_gamepad(LocalPlayerId player, InputSourceId source)
    {
        if (!contains(player)) return std::unexpected(InputBindingError::InvalidPlayer);
        if (!source.is_gamepad() || !valid_source(source)) return std::unexpected(InputBindingError::InvalidSource);
        if (owner(source).value && owner(source) != player) return std::unexpected(InputBindingError::InUse);
        return transfer_source(player, source);
    }
    LocalPlayerId owner(InputSourceId source) const
    {
        if (source.is_keyboard())
            return {};
        for (auto &[id, b] : _configuration.bindings)
            if ((source.is_mouse() && b.mouse) || (source.is_gamepad() && b.gamepad == source))
                return id;
        return {};
    }
    bool owns(LocalPlayerId player, InputSourceId source) const
    {
        auto it = _configuration.bindings.find(player);
        if (it == _configuration.bindings.end())
            return false;
        return source.is_keyboard() ? bool(it->second.keyboard.value) : owner(source) == player;
    }
    bool owns(LocalPlayerId player, const RawInputEvent &event) const
    {
        if (!event.source.is_keyboard())
            return owns(player, event.source);
        return allows_key(player, event.control);
    }
    bool allows_key(LocalPlayerId player, RawInputControl key) const
    {
        auto it = _configuration.bindings.find(player);
        if (it == _configuration.bindings.end() || !it->second.keyboard.value)
            return false;
        return _configuration.partitions.at(it->second.keyboard).keys.contains(key);
    }
    std::vector<InputSourceId> sources(LocalPlayerId player) const
    {
        std::vector<InputSourceId> out;
        auto it = _configuration.bindings.find(player);
        if (it == _configuration.bindings.end())
            return out;
        if (it->second.keyboard.value)
            out.push_back(InputSourceId::keyboard());
        if (it->second.mouse)
            out.push_back(InputSourceId::mouse());
        if (it->second.gamepad.value)
            out.push_back(it->second.gamepad);
        return out;
    }
    RawInputState filter_keyboard(LocalPlayerId player, RawInputState state) const
    {
        for (int i = 1; i < int(RawInputControl::Count); ++i)
        {
            auto key = static_cast<RawInputControl>(i);
            if (!allows_key(player, key))
                state.clear_control(key);
        }
        return state;
    }
    InputSnapshot for_player(LocalPlayerId player, const InputSnapshot &input) const
    {
        InputSnapshot result;
        for (auto source : input.sources)
            if (owns(player, source.source))
            {
                if (source.source.is_keyboard())
                {
                    source.initial_state = filter_keyboard(player, source.initial_state);
                    source.frame.state = filter_keyboard(player, source.frame.state);
                }
                result.sources.push_back(std::move(source));
            }
        for (auto event : input.events)
            if (owns(player, event))
                result.events.push_back(event);
        return result;
    }
    const std::vector<LocalPlayerId> &players() const
    {
        return _players;
    }
    std::uint64_t revision() const
    {
        return _revision;
    }
    std::uint64_t binding_version(LocalPlayerId player) const
    {
        auto it = _versions.find(player);
        return it == _versions.end() ? 0 : it->second;
    }

  private:
    static bool valid_source(InputSourceId source)
    {
        return source == InputSourceId::mouse() || (source.is_gamepad() && source.value > 0 && source.value <= UINT32_MAX);
    }
    std::map<LocalPlayerId, std::set<RawInputControl>> _required_keys;
    std::vector<LocalPlayerId> _players;
    PlayerInputConfiguration _configuration;
    std::map<LocalPlayerId, std::uint64_t> _versions;
    std::uint64_t _next = 1, _next_partition = 1, _revision = 0;
};
} // namespace elysia::input
