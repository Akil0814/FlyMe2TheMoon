#pragma once
#include "game/content/content.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace game::progress {
// ScoreUtils.scoreByLevelID:nStars:time:nCrystalLarge:nCrystalSmall: (0x9a574).
// Crystal value = small + 5 * large; the original weights are 3 and 15.
inline int original_score(int level,int stars,int crystal_value,double seconds,const content::Json& svrs) {
    if(level<1||level>60||stars<0||stars>3||crystal_value<0||!std::isfinite(seconds)||seconds<0)throw std::invalid_argument("Invalid score inputs");
    // Preserve the reference program's explicit 1..30 lookup clamp, even for
    // later level files; do not silently infer a different 60-entry rule.
    float reference=svrs.value(std::to_string(std::clamp(level,1,30)),10.f);
    if(reference<=std::numeric_limits<float>::epsilon())reference=10;
    double ratio=seconds>0?(stars+1)*static_cast<double>(reference)/static_cast<float>(seconds):std::numeric_limits<double>::infinity();
    ratio=std::max(ratio,0.);if(ratio>1)ratio=2-1/ratio;
    return static_cast<int>((200*ratio+100*stars+3*crystal_value)*100);
}
}
