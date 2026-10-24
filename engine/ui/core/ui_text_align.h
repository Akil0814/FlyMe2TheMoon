#pragma once

namespace elysia::ui
{
// Alignment is applied inside a widget's padded content rect, not against its parent container.
enum class TextHorizontalAlign
{
    Left,
    Center,
    Right
};

enum class TextVerticalAlign
{
    Top,
    Center,
    Bottom
};
}
