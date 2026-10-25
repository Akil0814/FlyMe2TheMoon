#pragma once
#include "engine/core/geometry/vector2.h"
#include <algorithm>
#include <cmath>

namespace game::play {
// GameController.cameraGotoPosition:todo: and moveGameCameraAndContinue:time:callBack:.
class CameraMove {
public:
    using Vec=elysia::core::Vector2;
    double begin(Vec from,Vec to,double hold=0) {
        _from=from;_to=to;_elapsed=0;_hold=std::max(0.,hold);
        _travel=std::pow((to-from).length()/1000.,.6);
        return duration();
    }
    void advance(double delta){_elapsed=std::min(duration(),_elapsed+std::max(0.,delta));}
    bool active()const{return _elapsed<duration();}
    bool holding()const{return _elapsed<_hold;}
    double duration()const{return _hold+_travel;}
    Vec position()const {
        const double t=_travel>0?std::clamp((_elapsed-_hold)/_travel,0.,1.):(_elapsed>=_hold?1:0);
        return _from+(_to-_from)*static_cast<float>(std::sin(t*1.5707963267948966));
    }
private:
    Vec _from{},_to{};
    double _elapsed=0,_hold=0,_travel=0;
};
}
