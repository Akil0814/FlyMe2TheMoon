#pragma once
#include "motion_profile.h"
#include <sstream>
#include <vector>
#include <stdexcept>

namespace game::play {
class ShotSchedule {
public:
    explicit ShotSchedule(const content::Json& info):_lag(parameter(info,"Lag")) {
        if(info.contains("Seq")) {
            std::stringstream in(info.at("Seq").get<std::string>());std::string token;
            while(std::getline(in,token,','))_shots.push_back(std::stod(token));
            if(_shots.size()<2)throw std::runtime_error("Malformed firing sequence");
            _period=_shots.back();_shots.pop_back();
        }else{_period=parameter(info,"Interval",3);_shots={0};}
        if(_period<=0||!std::is_sorted(_shots.begin(),_shots.end()))throw std::runtime_error("Invalid firing period");
        for(double shot:_shots)if(shot<0||shot>_period)throw std::runtime_error("Invalid firing phase");
    }
    int advance(double delta) {
        const double before=_time;_time+=delta;int count=0;
        while(next_time()<_time){if(next_time()>=before)++count;if(++_index==_shots.size()){_index=0;++_cycle;}}
        return count;
    }
private:
    double next_time()const{return _lag+_cycle*_period+_shots[_index];}
    std::vector<double> _shots;
    double _period=3,_lag=0,_time=0;
    size_t _index=0,_cycle=0;
};
}
