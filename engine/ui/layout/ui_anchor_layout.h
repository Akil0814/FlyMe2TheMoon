#pragma once

#include "ui_layout_geometry.h"
#include "../core/ui_child_host.h"

namespace elysia::ui::layout
{
// Repositions each child using its anchor and margin options inside the host bounds.
void layout_anchored_children(std::vector<UiChildHost::ChildEntry>& children,const elysia::core::Rect& bounds) noexcept;
}
