#pragma once
#include "engine/core/render/render_command.h"

namespace game::frontend {
// SuccessLayer.showSingleStar: emits first; showSingleStar2: changes the frame.
inline constexpr double result_star_interval=.5;
inline constexpr double result_star_reveal_delay=.2;
void submit_result_star_particles(std::vector<elysia::core::UiRenderCommand>& out,
    const elysia::core::Rect& slot,double elapsed,int index);
}
