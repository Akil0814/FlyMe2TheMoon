#pragma once

namespace elysia::ui
{
// Theme roles are semantic lookup keys. They do not own colors and remain stable across built-in themes.
enum class UiPanelVisualRole
{
    Default,
    Screen,
    Dialog,
    List
};

enum class UiLabelVisualRole
{
    Default,
    Title,
    Subtitle,
    Muted
};

enum class UiTextBlockVisualRole
{
    Default,
    Muted
};

enum class UiButtonVisualRole
{
    Default,
    Primary,
    Danger
};

enum class UiBarVisualRole
{
    Default,
    Progress
};

enum class UiDialogVisualRole
{
    Default
};

enum class UiDropdownVisualRole
{
    Default
};

}
