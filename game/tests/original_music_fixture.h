#pragma once

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace game::testing {
// Independent snapshot of the original map headers and -[Game nameMusic].
inline const std::map<std::string,std::vector<int>> original_level_music{
    {"fly.SymphonyofRush",{1,2,3,4,5,6,10,12,13,16,21,23,24,29,31,32,34,37,44,49,54,55,57}},
    {"fly.Hectic",{7,8,9,17,25,30,39,42,45,46,48,51,60}},
    {"fly.LifeforSpeed",{11,14,15,18,19,26,28,33,35,38,41,47,50,53,56,58}},
    {"fly.SpiralofAdventure",{20,22,27,36,40,43,52,59}}
};
inline const std::string& expected_level_music(int id){
    for(const auto& [music,levels]:original_level_music)
        if(std::find(levels.begin(),levels.end(),id)!=levels.end())return music;
    throw std::out_of_range("Level missing from original music snapshot");
}
}
