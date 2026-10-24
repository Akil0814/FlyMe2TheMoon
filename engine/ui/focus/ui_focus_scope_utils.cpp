#include "ui_focus_scope_utils.h"

#include "../core/ui_child_host.h"
#include "../core/ui_control.h"

#include <algorithm>

namespace elysia::ui
{
namespace
{
[[nodiscard]] std::size_t child_count_of(const UiElement& element) noexcept
{
    if (const auto* child_host = dynamic_cast<const UiChildHost*>(&element))
        return child_host->child_count();
    return 0;
}

[[nodiscard]] const UiElement* child_at_of(const UiElement& element,std::size_t index) noexcept
{
    if (const auto* child_host = dynamic_cast<const UiChildHost*>(&element))
        return child_host->child_at(index);
    return nullptr;
}
}

bool is_navigation_action(UiAction action) noexcept
{
    switch (action)
    {
    case UiAction::NavigateLeft:
    case UiAction::NavigateRight:
    case UiAction::NavigateUp:
    case UiAction::NavigateDown:
        return true;
    default:
        return false;
    }
}

bool is_control_usable(const UiControl* control) noexcept
{
    return control && !control->is_destroyed() && control->is_active() && control->is_visible() && control->is_enabled();
}

void collect_live_controls(const UiElement& element,std::vector<const UiControl*>& out_controls)
{
    if (element.is_destroyed() || !element.is_active() || !element.is_visible())
        return;
    if (const auto* control = dynamic_cast<const UiControl*>(&element))
        out_controls.push_back(control);
    const std::size_t count = child_count_of(element);
    for (std::size_t index = 0; index < count; ++index)
    {
        const UiElement* child = child_at_of(element,index);
        if (child)
            collect_live_controls(*child,out_controls);
    }
}

void collect_live_scopes(const UiElement& element,std::vector<const UiFocusScope*>& out_scopes)
{
    // Registration follows ownership, including temporarily hidden/inactive subtrees.
    // Input eligibility is checked separately by the window.
    if (element.is_destroyed())
        return;
    if (const auto* scope = dynamic_cast<const UiFocusScope*>(&element))
        out_scopes.push_back(scope);
    const std::size_t count = child_count_of(element);
    for (std::size_t index = 0; index < count; ++index)
    {
        const UiElement* child = child_at_of(element,index);
        if (child)
            collect_live_scopes(*child,out_scopes);
    }
}

bool contains_control(const std::vector<const UiControl*>& controls,const UiControl* control) noexcept
{
    return std::find(controls.begin(),controls.end(),control) != controls.end();
}

bool contains_scope(const std::vector<const UiFocusScope*>& scopes,const UiFocusScope* scope) noexcept
{
    return std::find(scopes.begin(),scopes.end(),scope) != scopes.end();
}
}
