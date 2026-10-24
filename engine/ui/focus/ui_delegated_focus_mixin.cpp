#include "ui_delegated_focus_mixin.h"

namespace elysia::ui
{
void UiDelegatedFocusMixin::reset_delegated_focus_state() noexcept
{
    _delegated_scope_members.clear();
}

UiElement* UiDelegatedFocusMixin::delegated_focus_region(UiElement* element) noexcept
{
    if (!element)
        return nullptr;
    if (dynamic_cast<UiControl*>(element))
        return element;
    if (dynamic_cast<UiFocusScope*>(element))
        return element;
    return nullptr;
}

const UiElement* UiDelegatedFocusMixin::delegated_focus_region(const UiElement* element) noexcept
{
    return delegated_focus_region(const_cast<UiElement*>(element));
}

UiFocusScope* UiDelegatedFocusMixin::delegated_scope_for_region(UiElement* region) noexcept
{
    return dynamic_cast<UiFocusScope*>(region);
}

const UiFocusScope* UiDelegatedFocusMixin::delegated_scope_for_region(const UiElement* region) noexcept
{
    return dynamic_cast<const UiFocusScope*>(region);
}

std::vector<UiElement*> UiDelegatedFocusMixin::delegated_focus_regions(const UiChildHost& host) const
{
    std::vector<UiElement*> regions;
    regions.reserve(host.child_count());
    for (std::size_t index = 0; index < host.child_count(); ++index)
    {
        UiElement* region = delegated_focus_region(const_cast<UiElement*>(host.child_at(index)));
        if (region)
            regions.push_back(region);
    }
    return regions;
}

std::vector<UiControl*> UiDelegatedFocusMixin::delegated_controls_in_region(const UiElement& element) const
{
    std::vector<UiControl*> controls;
    if (const auto* control = dynamic_cast<const UiControl*>(&element))
    {
        controls.push_back(const_cast<UiControl*>(control));
        return controls;
    }

    std::vector<const UiControl*> live_controls;
    collect_live_controls(element,live_controls);
    controls.reserve(live_controls.size());
    for (const UiControl* control : live_controls)
        controls.push_back(const_cast<UiControl*>(control));
    return controls;
}

UiControl* UiDelegatedFocusMixin::first_focusable_control_in_delegated_region(const UiElement* element) const noexcept
{
    if (!element)
        return nullptr;

    if (const UiFocusScope* scope = delegated_scope_for_region(element))
    {
        if (UiControl* target = scope->focused_target(); is_control_usable(target))
            return target;
    }

    const std::vector<UiControl*> controls = delegated_controls_in_region(*element);
    auto found = std::find_if(controls.begin(),controls.end(),[](const UiControl* control)
    {
        return is_control_usable(control);
    });
    return found != controls.end() ? *found : nullptr;
}

void UiDelegatedFocusMixin::build_delegated_focus_entries(
    const std::vector<DelegatedRegionEntry>& regions,
    std::vector<UiControlFocusScopeHost::FocusEntry>& out_entries)
{
    _delegated_scope_members.clear();
    out_entries.clear();

    for (const DelegatedRegionEntry& region_entry : regions)
    {
        if (!region_entry.region)
            continue;

        const std::vector<UiControl*> controls = delegated_controls_in_region(*region_entry.region);
        if (controls.empty())
            continue;

        UiControlFocusScopeHost::FocusEntry entry{};
        entry.neighbors.up = first_focusable_control_in_delegated_region(region_entry.up);
        entry.neighbors.down = first_focusable_control_in_delegated_region(region_entry.down);
        entry.neighbors.left = first_focusable_control_in_delegated_region(region_entry.left);
        entry.neighbors.right = first_focusable_control_in_delegated_region(region_entry.right);

        UiFocusScope* scope = delegated_scope_for_region(region_entry.region);
        for (UiControl* control : controls)
        {
            entry.control = control;
            out_entries.push_back(entry);
            if (scope)
                _delegated_scope_members.push_back(ScopeMember{ control,scope });
        }
    }
}

UiFocusScope* UiDelegatedFocusMixin::delegated_owner_scope_of(const UiControl* control) const noexcept
{
    if (!control)
        return nullptr;

    auto found = std::find_if(_delegated_scope_members.begin(),_delegated_scope_members.end(),[control](const ScopeMember& member)
    {
        return member.control == control;
    });
    return found != _delegated_scope_members.end() ? found->scope : nullptr;
}

UiElement* UiDelegatedFocusMixin::delegated_region_of(const UiControl* control,const std::vector<UiElement*>& regions) const noexcept
{
    if (!control)
        return nullptr;

    if (UiFocusScope* scope = delegated_owner_scope_of(control))
        return &scope->focus_scope_element();

    auto found = std::find(regions.begin(),regions.end(),control);
    return found != regions.end() ? *found : nullptr;
}

bool UiDelegatedFocusMixin::focus_delegated_region(UiElement* region,bool focus_first)
{
    if (!region)
        return false;

    if (UiFocusScope* scope = delegated_scope_for_region(region))
    {
        UiControl* target = scope->focused_target();
        if (focus_first || !is_control_usable(target))
        {
            if (!scope->focus_first_available())
                return false;
            target = scope->focused_target();
        }
        return is_control_usable(target);
    }

    UiControl* control = dynamic_cast<UiControl*>(region);
    return is_control_usable(control);
}

bool UiDelegatedFocusMixin::enter_delegated_region(UiControlFocusScopeHost& host,UiElement* region)
{
    if (!focus_delegated_region(region,false))
        return false;

    UiControl* target = first_focusable_control_in_delegated_region(region);
    if (!host.set_focused_target_internal(target))
        return false;

    sync_delegated_owner_scope_target(target);
    return true;
}

void UiDelegatedFocusMixin::sync_host_delegated_focus_target(UiControlFocusScopeHost& host) noexcept
{
    UiControl* target = host._focused_target;
    if (UiFocusScope* scope = delegated_owner_scope_of(target))
    {
        UiControl* scope_target = scope->focused_target();
        if (is_control_usable(scope_target) && host.is_registered_focus_target(scope_target))
        {
            (void)host.set_focused_target_internal(scope_target);
            target = scope_target;
        }
    }

    sync_delegated_owner_scope_target(target);
}

void UiDelegatedFocusMixin::sync_delegated_owner_scope_target(UiControl* control) noexcept
{
    if (UiFocusScope* scope = delegated_owner_scope_of(control))
    {
        if (auto* host = dynamic_cast<UiControlFocusScopeHost*>(scope))
            host->set_focused_target(control);
    }
}

void UiDelegatedFocusMixin::sync_delegated_scope_focus(
    UiControl* focused_target,
    bool scope_focused,
    const std::vector<UiElement*>& regions
) noexcept
{
    sync_delegated_scope_focus(delegated_region_of(focused_target,regions),scope_focused,regions);
    sync_delegated_owner_scope_target(focused_target);
}

void UiDelegatedFocusMixin::sync_delegated_scope_focus(
    UiElement* active_region,
    bool scope_focused,
    const std::vector<UiElement*>& regions
) noexcept
{
    for (UiElement* region : regions)
    {
        if (UiFocusScope* scope = delegated_scope_for_region(region))
            scope->set_scope_focused(scope_focused && region == active_region);
    }
}
}
