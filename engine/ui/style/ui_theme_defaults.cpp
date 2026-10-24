#include "ui_theme_defaults.h"

#include "ui_theme.h"

namespace elysia::ui
{
namespace
{
UiBuiltinTheme& default_builtin_theme() noexcept
{
    static UiBuiltinTheme theme = UiBuiltinTheme::BlueGlassMoon;
    return theme;
}

bool valid_builtin_theme(UiBuiltinTheme theme) noexcept
{
    switch (theme)
    {
    case UiBuiltinTheme::BlueGlassMoon:
    case UiBuiltinTheme::ElysiaLight:
    case UiBuiltinTheme::ElysiaDark:
    case UiBuiltinTheme::EvangelionUnit00:
    case UiBuiltinTheme::EvangelionUnit01:
    case UiBuiltinTheme::EvangelionUnit02:
    case UiBuiltinTheme::QuietSlate:
        return true;
    default:
        return false;
    }
}
}

bool UiThemeDefaults::set_builtin_theme(UiBuiltinTheme theme) noexcept
{
    if (!valid_builtin_theme(theme))
        return false;

    default_builtin_theme() = theme;
    return true;
}

UiBuiltinTheme UiThemeDefaults::current_builtin_theme() noexcept
{
    return default_builtin_theme();
}

const UiTheme& UiThemeDefaults::theme() noexcept
{
    return builtin_theme(default_builtin_theme());
}
}
