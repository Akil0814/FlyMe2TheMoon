#pragma once

#include "../../core/game_object.h"
#include "../../ui/core/ui_element.h"

#include <algorithm>
#include <functional>
#include <tuple>
#include <vector>

namespace elysia::scene
{
namespace scene_input_order
{
[[nodiscard]] inline bool receiver_priority_less(
    const elysia::core::SceneObject* lhs,
    const elysia::core::SceneObject* rhs
) noexcept
{
    if (lhs == rhs)
    {
        return false;
    }

    const elysia::ui::UiElement* lhs_ui = dynamic_cast<const elysia::ui::UiElement*>(lhs);
    const elysia::ui::UiElement* rhs_ui = dynamic_cast<const elysia::ui::UiElement*>(rhs);

    if (lhs_ui && rhs_ui)
    {
        return lhs_ui->order() > rhs_ui->order();
    }

    const elysia::core::GameObject* lhs_game = dynamic_cast<const elysia::core::GameObject*>(lhs);
    const elysia::core::GameObject* rhs_game = dynamic_cast<const elysia::core::GameObject*>(rhs);

    if (lhs_game && rhs_game)
    {
        return std::make_tuple(
            lhs_game->depth_layer(),
            lhs_game->order_in_layer()
        ) > std::make_tuple(
            rhs_game->depth_layer(),
            rhs_game->order_in_layer()
        );
    }

    if (lhs_ui && rhs_game)
    {
        return true;
    }

    if (lhs_game && rhs_ui)
    {
        return false;
    }

    return std::less<const elysia::core::SceneObject*>{}(lhs, rhs);
}

template <typename Entry>
void insert_receiver_entry_sorted(std::vector<Entry>& entries, Entry entry)
{
    const auto iter = std::upper_bound(
        entries.begin(),
        entries.end(),
        entry,
        [](const Entry& lhs, const Entry& rhs)
        {
            return receiver_priority_less(lhs.object, rhs.object);
        }
    );

    entries.insert(iter, entry);
}
} // namespace scene_input_order

}
