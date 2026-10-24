#pragma once

#include "ui_theme_id.h"

namespace elysia::ui
{
struct UiTheme;

// Process-wide construction defaults. This does not manage or refresh UI trees.
class UiThemeDefaults final
{
public:
    UiThemeDefaults() = delete;

    [[nodiscard]] static bool set_builtin_theme(
        UiBuiltinTheme theme) noexcept;
    [[nodiscard]] static UiBuiltinTheme current_builtin_theme() noexcept;
    [[nodiscard]] static const UiTheme& theme() noexcept;
};
}
