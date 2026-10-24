#pragma once
#include "game/content/content.h"
#include <array>
#include <set>
#include <string>

namespace game::progress {
struct Progress {
    std::array<int,60> stars;
    std::array<int,60> scores{};
    std::array<int,60> deaths{};
    std::array<double,60> best_time{};
    std::int64_t crystals=0;
    std::set<int> achievements;
    std::set<std::string> story_flags;
    bool no_pickup_clear=false;
    Progress(){stars.fill(-1);}
    bool passed(int level)const;
    bool unlocked(int level,const content::Json& dependencies)const;
    void evaluate_achievements();
    content::Json encode()const;
    static Progress decode(const content::Json& json);
};
class Store {
public:
    static Store& instance();
    bool load();
    bool save();
    bool record_clear(int level,int stars,int crystal_value,int crystal_count,int pickups,double elapsed);
    bool record_death(int level,int crystals);
    Progress data;
    std::string error;
    bool recovered=false;
private:
    bool _loaded=false,_writable=true;
    std::string slot()const;
};
}
