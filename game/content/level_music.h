#pragma once

#include <string_view>

namespace game::content {
// Original -[Game nameMusic]: the first three levels override their suite.
constexpr std::string_view level_music(int level_id,int suite_id) {
    if(level_id<4)return "fly.SymphonyofRush";
    switch(suite_id){
    case 1:return "fly.LifeforSpeed";
    case 2:return "fly.Hectic";
    case 3:return "fly.SpiralofAdventure";
    default:return "fly.SymphonyofRush";
    }
}
}
