#pragma once

#include "../../core/geometry/vector2.h"

namespace elysia::ui
{
enum class UiLayoutAnchor
{
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

enum class UiLayoutDirection
{
    Horizontal,
    Vertical
};

enum class UiLayoutAlign
{
    Start,
    Center,
    End
};

// Insets carved out from a host rect before child layout runs.
struct UiLayoutPadding
{
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

// Extra offset applied around one child when anchoring or stacking it.
struct UiLayoutMargin
{
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

// Optional per-child layout overrides consumed by layout hosts.
struct UiLayoutChildOptions
{
    UiLayoutAnchor _anchor = UiLayoutAnchor::TopLeft;
    UiLayoutMargin _margin;
    UiLayoutAlign _cross_align = UiLayoutAlign::Start;
    elysia::core::Vector2 _size_override{ 0.0f,0.0f };

    bool _use_custom_cross_align = false;
    bool _fill_cross_axis = false;
    bool _use_size_override = false;
};

// Lightweight transform metadata for layouts that offset or scale child content.
struct UiLayoutTransform
{
    elysia::core::Vector2 translation{ 0.0f,0.0f };
    elysia::core::Vector2 scale{ 1.0f,1.0f };
};
}
