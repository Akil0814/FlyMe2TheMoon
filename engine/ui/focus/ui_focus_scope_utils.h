#pragma once

#include "ui_focus_scope.h"
#include "../input/ui_input_types.h"

#include <vector>

namespace elysia::ui
{
class UiControl;
class UiElement;

// Returns true when the action represents directional focus navigation.
[[nodiscard]] bool is_navigation_action(UiAction action) noexcept;
// Filters out destroyed, disabled, or otherwise unusable controls.
[[nodiscard]] bool is_control_usable(const UiControl* control) noexcept;
// Collects focusable controls that are still live under the given element tree.
void collect_live_controls(const UiElement& element,std::vector<const UiControl*>& out_controls);
// Collects owned, non-destroyed scopes, including hidden/inactive subtrees.
void collect_live_scopes(const UiElement& element,std::vector<const UiFocusScope*>& out_scopes);
// Tests membership without assuming the stored control pointers are unique.
[[nodiscard]] bool contains_control(const std::vector<const UiControl*>& controls,const UiControl* control) noexcept;
// Tests membership without assuming the stored scope pointers are unique.
[[nodiscard]] bool contains_scope(const std::vector<const UiFocusScope*>& scopes,const UiFocusScope* scope) noexcept;
}
