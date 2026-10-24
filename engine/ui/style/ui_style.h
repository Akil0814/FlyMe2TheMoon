#pragma once

#include <optional>

namespace elysia::ui
{
template<class Style>
struct UiStyleOverrideTraits;

template<class T>
inline void apply_ui_style_override(T& target,const std::optional<T>& value) noexcept
{
    if (value)
        target = *value;
}

template<class Style>
class UiStyleState
{
public:
    using Overrides = typename UiStyleOverrideTraits<Style>::Overrides;

    // Resets both theme-derived and manual state. Callers typically use this from reset()
    // so controls fall back to pure theme ownership until an explicit override is applied.
    void reset(const Style& base_style) noexcept
    {
        _base_style = base_style;
        _overrides = Overrides{};
        rebuild_effective_style();
    }

    // Updates the theme-owned style without disturbing any active manual override.
    void set_base_style(const Style& style) noexcept
    {
        _base_style = style;
        rebuild_effective_style();
    }

    [[nodiscard]] const Style& base_style() const noexcept
    {
        return _base_style;
    }

    void set_style_overrides(const Overrides& overrides) noexcept
    {
        _overrides = overrides;
        rebuild_effective_style();
    }

    [[nodiscard]] bool has_style_overrides() const noexcept
    {
        return !UiStyleOverrideTraits<Style>::empty(_overrides);
    }

    void clear_style_overrides() noexcept
    {
        _overrides = Overrides{};
        rebuild_effective_style();
    }

    [[nodiscard]] const Overrides& style_overrides() const noexcept
    {
        return _overrides;
    }

    // Rendering and layout should always read through effective_style() so manual overrides
    // transparently shadow the current theme style.
    [[nodiscard]] const Style& effective_style() const noexcept
    {
        return _effective_style;
    }

private:
    void rebuild_effective_style() noexcept
    {
        _effective_style = _base_style;
        UiStyleOverrideTraits<Style>::apply(_effective_style,_overrides);
    }

    Style _base_style{};
    Overrides _overrides{};
    Style _effective_style{};
};
}
