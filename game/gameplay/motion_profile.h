#pragma once
#include "game/content/content.h"
#include <algorithm>
#include <cmath>
namespace game::play {
inline double parameter(const content::Json& info,const char* key,double fallback=0) {
    if(!info.contains(key))return fallback;
    return info[key].is_string()?std::stod(info[key].get<std::string>()):info[key].get<double>();
}
// Original DynamicBox quarter-cycle power curve and its velocity derivative.
struct MotionProfile {
    double cycle=4,lag=0,pause_half=0,pause_end=0,rate=2;
    explicit MotionProfile(const content::Json& j):cycle(std::max(.01,parameter(j,"Cycle",4))),lag(parameter(j,"Lag")),pause_half(std::max(0.,parameter(j,"PauseHalf"))),pause_end(std::max(0.,parameter(j,"PauseEnd"))),rate(std::max(.1,parameter(j,"Rate",2))){}
    double speed_ratio(double time)const {
        if(time<lag)return 0;
        const double t=std::fmod(time-lag,cycle+pause_half+pause_end),q=cycle/4;
        double phase=0,sign=1;
        if(t<q)phase=t/q;
        else if(t<2*q)phase=(2*q-t)/q;
        else if(t<2*q+pause_half)return 0;
        else if(t<3*q+pause_half){phase=(t-2*q-pause_half)/q;sign=-1;}
        else if(t<cycle+pause_half){phase=(cycle+pause_half-t)/q;sign=-1;}
        else return 0;
        return sign*2*rate*std::pow(phase,rate-1)/cycle;
    }
    double fraction(double time)const {
        if(time<lag)return 0;
        const double half=cycle/2,period=cycle+pause_half+pause_end;
        const double t=std::fmod(time-lag,period);double value=0;
        if(t<half)value=t/half;
        else if(t<half+pause_half)return 1;
        else if(t<cycle+pause_half)value=1-(t-half-pause_half)/half;
        else return 0;
        if(rate==1)return value;
        return value<.5?.5*std::pow(value*2,rate):1-.5*std::pow((1-value)*2,rate);
    }
};
}
