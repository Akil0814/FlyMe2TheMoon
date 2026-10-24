#pragma once

#include "ui_theme.h"
#include "ui_theme_defaults.h"
#include "ui_theme_style_resolver.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace elysia::ui
{
class UiElement;
class UiChildHost;
class UiThemeManager;

class UiThemeRegistration
{
public:
    UiThemeRegistration() = default;
    ~UiThemeRegistration();
    UiThemeRegistration(const UiThemeRegistration&) = delete;
    UiThemeRegistration& operator=(const UiThemeRegistration&) = delete;
    UiThemeRegistration(UiThemeRegistration&&) noexcept;
    UiThemeRegistration& operator=(UiThemeRegistration&&) noexcept;
    void reset() noexcept;
    [[nodiscard]] bool registered() const noexcept;
private:
    struct RegistrationRecord { UiThemeManager* manager = nullptr; UiChildHost* root = nullptr; std::size_t count = 0; };
    explicit UiThemeRegistration(std::weak_ptr<RegistrationRecord>) noexcept;
    std::weak_ptr<RegistrationRecord> _record;
    friend class UiThemeManager;
};

class UiThemeManager
{
public:
    UiThemeManager();
    ~UiThemeManager();
    UiThemeManager(const UiThemeManager&) = delete;
    UiThemeManager& operator=(const UiThemeManager&) = delete;

    [[nodiscard]] UiThemeRegistration register_root(UiChildHost& root);
    void unregister_root(UiChildHost& root) noexcept;
    void set_theme(UiBuiltinTheme theme);
    [[nodiscard]] UiBuiltinTheme current_builtin_theme() const noexcept;
    [[nodiscard]] const UiTheme& current_theme() const noexcept;
    void reapply_theme();

    void refresh_element(UiElement& element);
    void attach_and_apply_subtree(UiElement& element);
    void detach_subtree(UiElement& element) noexcept;
    void on_host_destroying(UiChildHost& host) noexcept;

private:
    void apply_style_subtree(UiElement& element);
    void attach_context_subtree(UiElement& element);
    void release(UiThemeRegistration::RegistrationRecord&) noexcept;
    void detach_all() noexcept;

    UiBuiltinTheme _builtin;
    UiTheme _theme;
    UiThemeStyleResolver _resolver;
    std::vector<std::shared_ptr<UiThemeRegistration::RegistrationRecord>> _records;
    friend class UiThemeRegistration;
};
}
