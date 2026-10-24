#pragma once

#include "../../core/render/render_command.h"

namespace elysia::gameplay::ui
{
enum class WorldBarFillDirection { LeftToRight, RightToLeft, TopToBottom, BottomToTop };

struct WorldBarStyle
{
    elysia::core::Color background{40, 40, 40, 255};
    elysia::core::Color fill{76, 175, 80, 255};
    elysia::core::Color border{255, 255, 255, 255};
    float border_width = 0.0f;
};

class WorldBar
{
public:
    void set_range(float minimum, float maximum) noexcept;
    void set_value(float value) noexcept;
    void set_ratio(float ratio) noexcept;
    [[nodiscard]] float value() const noexcept { return _value; }
    [[nodiscard]] float ratio() const noexcept;
    void set_style(WorldBarStyle style) noexcept { _style = style; }
    void set_fill_direction(WorldBarFillDirection direction) noexcept { _direction = direction; }
    void set_visible(bool visible) noexcept { _visible = visible; }
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands,
        const elysia::core::Rect& world_rect) const;

private:
    float _minimum = 0.0f;
    float _maximum = 1.0f;
    float _value = 0.0f;
    WorldBarStyle _style;
    WorldBarFillDirection _direction = WorldBarFillDirection::LeftToRight;
    bool _visible = true;
};
}
