#pragma once

#include "ui_control_focus_scope_host.h"

#include <vector>

namespace elysia::ui
{
// Bridges a composite host's flat focus graph with controls owned by nested UiFocusScope regions.
// It does not own UI elements; cached scope-member pairs are rebuilt with the host's focus registry.
class UiDelegatedFocusMixin
{
protected:
    // Describes directional transitions between delegated regions, not individual nested controls.
    struct DelegatedRegionEntry
    {
        UiElement* region = nullptr;
        UiElement* up = nullptr;
        UiElement* down = nullptr;
        UiElement* left = nullptr;
        UiElement* right = nullptr;
    };

protected:
    void reset_delegated_focus_state() noexcept;

    [[nodiscard]] static UiElement* delegated_focus_region(UiElement* element) noexcept;
    [[nodiscard]] static const UiElement* delegated_focus_region(const UiElement* element) noexcept;
    [[nodiscard]] static UiFocusScope* delegated_scope_for_region(UiElement* region) noexcept;
    [[nodiscard]] static const UiFocusScope* delegated_scope_for_region(const UiElement* region) noexcept;

    [[nodiscard]] std::vector<UiElement*> delegated_focus_regions(const UiChildHost& host) const;
    [[nodiscard]] std::vector<UiControl*> delegated_controls_in_region(const UiElement& element) const;
    [[nodiscard]] UiControl* first_focusable_control_in_delegated_region(const UiElement* element) const noexcept;

    // Flattens live controls into the owner host while remembering their actual nested scope.
    void build_delegated_focus_entries(
        const std::vector<DelegatedRegionEntry>& regions,
        std::vector<UiControlFocusScopeHost::FocusEntry>& out_entries);

    [[nodiscard]] UiFocusScope* delegated_owner_scope_of(const UiControl* control) const noexcept;
    [[nodiscard]] UiElement* delegated_region_of(const UiControl* control,const std::vector<UiElement*>& regions) const noexcept;
    [[nodiscard]] bool focus_delegated_region(UiElement* region,bool focus_first);
    [[nodiscard]] bool enter_delegated_region(UiControlFocusScopeHost& host,UiElement* region);
    // Pulls a nested scope's current target back into the outer host after delegated navigation.
    void sync_host_delegated_focus_target(UiControlFocusScopeHost& host) noexcept;
    void sync_delegated_owner_scope_target(UiControl* control) noexcept;
    void sync_delegated_scope_focus(UiControl* focused_target,bool scope_focused,const std::vector<UiElement*>& regions) noexcept;
    void sync_delegated_scope_focus(UiElement* active_region,bool scope_focused,const std::vector<UiElement*>& regions) noexcept;

private:
    struct ScopeMember
    {
        UiControl* control = nullptr;
        UiFocusScope* scope = nullptr;
    };

private:
    std::vector<ScopeMember> _delegated_scope_members;
};
}
