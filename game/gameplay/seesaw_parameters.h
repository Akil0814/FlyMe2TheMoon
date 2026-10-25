#pragma once
#include "motion_profile.h"
#include <numbers>

namespace game::play {
// Seesaw.initExtraInfo... and createInPhysicalWorld: in the original binary.
// Texture widths include end caps; the physical plank is exactly 64/128/256/512.
inline float seesaw_width(int kind) {
    switch(kind){case 108:return 64;case 109:return 128;case 110:return 256;case 117:return 512;default:throw std::invalid_argument("Not a seesaw");}
}
inline float seesaw_anchor(int kind,const content::Json& info) {
    const float width=seesaw_width(kind);
    return std::clamp(static_cast<float>(parameter(info,"Anchor",.5)-.5)*width,-width/2+4,width/2-4);
}
inline float seesaw_speed(const content::Json& info) {
    // Original Box2D is Y-up, whereas engine angles are clockwise in Y-down.
    return -static_cast<float>(parameter(info,"Speed"))*std::numbers::pi_v<float>/180;
}
}
