#pragma once
#include "motion_profile.h"
#include "engine/core/geometry/vector2.h"
#include <numbers>

namespace game::play {
// Original LaserBeam uses a local Y-up angle, even in the Y-down port.
class LaserAim {
public:
    explicit LaserAim(const content::Json& info):_motion(info) {
        _motion.rate=std::max(.1,parameter(info,"Rate",1));
        _angle=static_cast<float>(parameter(info,"FixedAngle"))*radians;
    }
    float update(const content::Json& info,double time,double delta,float body_angle,elysia::core::Vector2 to_role) {
        if(info.contains("FixedAngle"))return body_angle-_angle;
        if(info.contains("Cycle")) {
            // Lag delays rotation, not emission. The initial angle is zero.
            if(time>=_motion.lag)_angle=static_cast<float>(parameter(info,"MinAngle")+(parameter(info,"MaxAngle",180)-parameter(info,"MinAngle"))*_motion.fraction(time))*radians;
        } else {
            constexpr float pi=std::numbers::pi_v<float>;
            float target=std::atan2(-to_role.y,to_role.x)+body_angle+pi/2;
            target-=std::floor(target/(2*pi))*(2*pi);
            target=std::clamp(target-pi/2,0.f,pi);
            _angle+=(target-_angle)*static_cast<float>(std::clamp(delta*5,0.,1.));
        }
        return body_angle-_angle;
    }
    static bool in_range(const content::Json& info,float distance) {
        const double range=parameter(info,"ActiveRange",1000);
        return range==-1||distance<=range;
    }
private:
    static constexpr float radians=std::numbers::pi_v<float>/180;
    MotionProfile _motion;
    float _angle=0;
};
}
