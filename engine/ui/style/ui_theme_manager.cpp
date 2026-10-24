#include "ui_theme_manager.h"

#include "../core/ui_child_host.h"

#include <algorithm>

namespace elysia::ui
{
UiThemeRegistration::UiThemeRegistration(std::weak_ptr<RegistrationRecord> r) noexcept : _record(std::move(r)) {}
UiThemeRegistration::~UiThemeRegistration() { reset(); }
UiThemeRegistration::UiThemeRegistration(UiThemeRegistration&& other) noexcept : _record(std::move(other._record)) {}
UiThemeRegistration& UiThemeRegistration::operator=(UiThemeRegistration&& other) noexcept
{ if (this != &other) { reset(); _record = std::move(other._record); } return *this; }
void UiThemeRegistration::reset() noexcept
{ if (auto r = _record.lock(); r && r->manager) r->manager->release(*r); _record.reset(); }
bool UiThemeRegistration::registered() const noexcept
{ auto r = _record.lock(); return r && r->manager && r->root && r->count > 0; }

UiThemeManager::UiThemeManager()
    : _builtin(UiThemeDefaults::current_builtin_theme())
    , _theme(make_builtin_theme(_builtin))
{
    register_builtin_ui_theme_adapters(_resolver);
}
UiThemeManager::~UiThemeManager() { detach_all(); }

UiThemeRegistration UiThemeManager::register_root(UiChildHost& root)
{
    auto found = std::find_if(_records.begin(),_records.end(),[&](const auto& r) { return r && r->root == &root; });
    if (found != _records.end()) { ++(*found)->count; return UiThemeRegistration(*found); }
    if (root._theme_manager && root._theme_manager != this)
        root._theme_manager->unregister_root(root);
    auto record = std::make_shared<UiThemeRegistration::RegistrationRecord>();
    record->manager = this; record->root = &root; record->count = 1;
    _records.push_back(record);
    attach_and_apply_subtree(root);
    return UiThemeRegistration(record);
}

void UiThemeManager::unregister_root(UiChildHost& root) noexcept
{
    auto found = std::find_if(_records.begin(),_records.end(),[&](const auto& r) { return r && r->root == &root; });
    if (found != _records.end()) { (*found)->count = 1; release(*(*found)); }
}

void UiThemeManager::set_theme(UiBuiltinTheme theme) { _builtin = theme; _theme = make_builtin_theme(theme); reapply_theme(); }
UiBuiltinTheme UiThemeManager::current_builtin_theme() const noexcept { return _builtin; }
const UiTheme& UiThemeManager::current_theme() const noexcept { return _theme; }
void UiThemeManager::reapply_theme()
{ for (const auto& r : _records) if (r && r->root && !r->root->is_destroyed()) apply_style_subtree(*r->root); }

void UiThemeManager::refresh_element(UiElement& element)
{
    if (!element.is_destroyed()) (void)_resolver.apply(element,_theme);
}

void UiThemeManager::attach_and_apply_subtree(UiElement& element)
{ attach_context_subtree(element); apply_style_subtree(element); }

void UiThemeManager::attach_context_subtree(UiElement& element)
{
    auto* host = dynamic_cast<UiChildHost*>(&element);
    if (!host) return;
    host->attach_theme_manager(*this);
    for (std::size_t i = 0; i < host->child_count(); ++i)
        if (UiElement* child = host->child_at(i)) attach_context_subtree(*child);
    for (UiElement* child : host->_external_style_children)
        if (child) attach_context_subtree(*child);
}

void UiThemeManager::detach_subtree(UiElement& element) noexcept
{
    auto* host = dynamic_cast<UiChildHost*>(&element);
    if (!host) return;
    for (std::size_t i = 0; i < host->child_count(); ++i)
        if (UiElement* child = host->child_at(i)) detach_subtree(*child);
    for (UiElement* child : host->_external_style_children)
        if (child) detach_subtree(*child);
    host->detach_theme_manager(*this);
}

void UiThemeManager::on_host_destroying(UiChildHost& host) noexcept
{
    auto found = std::find_if(_records.begin(),_records.end(),[&](const auto& r) { return r && r->root == &host; });
    if (found == _records.end())
    {
        host.detach_theme_manager(*this);
        return;
    }
    detach_subtree(host);
    (*found)->manager = nullptr;
    (*found)->root = nullptr;
    (*found)->count = 0;
    _records.erase(found);
}

void UiThemeManager::apply_style_subtree(UiElement& element)
{
    if (element.is_destroyed()) return;
    const UiThemeStyleResolution result = _resolver.apply(element,_theme);
    if (result.traversal == UiThemeStyleTraversal::Stop) return;
    if (result.traversal == UiThemeStyleTraversal::AdapterDefinedChildren)
    { for (UiElement* child : result.adapter_children) if (child) apply_style_subtree(*child); return; }
    auto* host = dynamic_cast<UiChildHost*>(&element);
    if (!host) return;
    for (const UiChildHost::ChildEntry& entry : host->children())
        if (entry.element && entry.style_relation == UiChildStyleRelation::Independent) apply_style_subtree(*entry.element);
    for (UiElement* child : host->_external_style_children)
        if (child) apply_style_subtree(*child);
}

void UiThemeManager::release(UiThemeRegistration::RegistrationRecord& record) noexcept
{
    if (record.manager != this || record.count == 0 || --record.count > 0) return;
    if (record.root) detach_subtree(*record.root);
    record.manager = nullptr; record.root = nullptr;
    std::erase_if(_records,[&](const auto& r) { return r.get() == &record; });
}

void UiThemeManager::detach_all() noexcept
{
    auto records = _records; _records.clear();
    for (const auto& r : records) if (r) { if (r->root) detach_subtree(*r->root); r->manager = nullptr; r->root = nullptr; r->count = 0; }
}
}
