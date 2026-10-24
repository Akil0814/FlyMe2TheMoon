#include "ui_anchor_layout.h"

namespace elysia::ui::layout
{
void layout_anchored_children(std::vector<UiChildHost::ChildEntry>& children,const elysia::core::Rect& bounds) noexcept
{
    for (UiChildHost::ChildEntry& child : children)
    {
        if (!child.element)
            continue;
        const elysia::core::Vector2 size = child.layout._use_size_override
            ? clamp_size(child.layout._size_override)
            : clamp_size(child.element->size());
        child.element->set_screen_rect(anchored_rect(bounds,size,child.layout._anchor,child.layout._margin));
    }
}
}
