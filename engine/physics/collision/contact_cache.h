#pragma once

#include "collision_event.h"

#include <span>
#include <vector>

namespace elysia::physics
{
class ContactCache
{
public:
    void update(
        std::span<const CollisionContact> current_contacts,
        std::vector<CollisionEvent>& out_events);

    void collect_contacts(
        CollisionTarget target,
        std::vector<CollisionContact>& out_contacts) const;

    // Remove current state immediately; publish one End on the next step.
    void invalidate_target(CollisionTarget target);
    void clear() noexcept;

    [[nodiscard]] std::span<const CollisionContact> contacts() const noexcept;

private:
    std::vector<CollisionContact> _contacts;
    std::vector<CollisionContact> _invalidated;
};
}
