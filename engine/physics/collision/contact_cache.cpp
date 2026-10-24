#include "contact_cache.h"

#include <algorithm>

namespace elysia::physics
{
void ContactCache::update(
    std::span<const CollisionContact> current_contacts,
    std::vector<CollisionEvent>& out_events)
{
    out_events.clear();
    for (const auto& contact : _invalidated)
        out_events.push_back({CollisionEventPhase::End, contact});
    _invalidated.clear();
    std::size_t previous_index = 0;
    std::size_t current_index = 0;
    while (previous_index < _contacts.size()
        || current_index < current_contacts.size())
    {
        if (previous_index >= _contacts.size())
        {
            out_events.push_back(CollisionEvent{
                CollisionEventPhase::Begin,
                current_contacts[current_index++]
            });
            continue;
        }
        if (current_index >= current_contacts.size())
        {
            out_events.push_back(CollisionEvent{
                CollisionEventPhase::End,
                _contacts[previous_index++]
            });
            continue;
        }

        const CollisionContact& previous = _contacts[previous_index];
        const CollisionContact& current = current_contacts[current_index];
        if (previous.pair < current.pair)
        {
            out_events.push_back(CollisionEvent{CollisionEventPhase::End, previous});
            ++previous_index;
        }
        else if (current.pair < previous.pair)
        {
            out_events.push_back(CollisionEvent{CollisionEventPhase::Begin, current});
            ++current_index;
        }
        else
        {
            out_events.push_back(CollisionEvent{CollisionEventPhase::Stay, current});
            ++previous_index;
            ++current_index;
        }
    }
    // For a teleport/rebind at the same pair, the old End precedes the new Begin.
    std::ranges::stable_sort(out_events, {}, [](const CollisionEvent& event) { return event.contact.pair; });
    _contacts.assign(current_contacts.begin(), current_contacts.end());
}

void ContactCache::collect_contacts(
    CollisionTarget target,
    std::vector<CollisionContact>& out_contacts) const
{
    out_contacts.clear();
    if (!target.is_valid())
        return;
    for (const CollisionContact& contact : _contacts)
    {
        if (contact.pair.first == target || contact.pair.second == target)
            out_contacts.push_back(contact);
    }
}

void ContactCache::invalidate_target(CollisionTarget target)
{
    _invalidated.reserve(_invalidated.size() + _contacts.size());
    std::erase_if(_contacts, [&](const CollisionContact& contact)
    {
        const bool remove = contact.pair.first == target || contact.pair.second == target;
        if (remove) _invalidated.push_back(contact);
        return remove;
    });
}

void ContactCache::clear() noexcept
{
    _contacts.clear();
    _invalidated.clear();
}

std::span<const CollisionContact> ContactCache::contacts() const noexcept
{
    return _contacts;
}
}
