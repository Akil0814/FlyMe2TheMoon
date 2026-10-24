#include "game/gameplay/motion_profile.h"
#include "engine/physics/physics_world.h"
#include "engine/core/game_object.h"
#include <fstream>
#include <iostream>
#include <format>
#include <set>
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main(){try{
    using namespace game::play;using namespace elysia::physics;using game::content::Json;
    MotionProfile gate(Json{{"Cycle","4"},{"Lag","99"},{"PauseHalf","2"},{"PauseEnd","3"},{"Rate","1"}});
    check(gate.fraction(98)==0&&gate.fraction(100)==.5&&gate.fraction(102)==1&&gate.fraction(104)==.5&&gate.fraction(106)==0&&gate.fraction(108)==0,"Gate travel, endpoint dwell, or cycle timing failed");
    int moving=0,rotating=0,actions=0;
    for(int i=1;i<=60;++i){std::ifstream input(std::format("assets/configs/fly/levels/{:03}.json",i));auto data=Json::parse(input);std::set<int> targets;
        for(const auto& e:data.at("entities"))if(!e.at("item").get<bool>()){
            targets.insert(e.at("id").get<int>());const int k=e.at("kind");
            if(k>=105&&k<=107){++moving;MotionProfile m(e.at("info"));
                for(double time:{0.,m.lag,m.lag+m.cycle/4,m.lag+m.cycle/2,m.lag+m.cycle/2+m.pause_half,m.lag+m.cycle+m.pause_half,m.lag+m.cycle+m.pause_half+m.pause_end}){const double f=m.fraction(time);check(std::isfinite(f)&&f>=0&&f<=1,"Nonfinite / out-of-path platform pose");}}
            if(k==108||k==109||k==110||k==117){++rotating;const double anchor=parameter(e.at("info"),"Anchor",.5);check(anchor>=0&&anchor<=1,"Invalid rotational anchor");}
        }
        for(const auto& area:data.at("areas"))for(const auto& a:area.at("actions")){++actions;const auto name=a.at("Target").get<std::string>();check(name.starts_with("4:")&&targets.contains(std::stoi(name.substr(2))),"Area references missing dynamic object");check(a.at("Action")=="Start"&&parameter(a,"Delay")>=0,"Unsupported trigger action");}
    }
    check(moving==388&&rotating==65&&actions==102,"Original mechanism inventory changed");
    elysia::core::GameObject anchor(elysia::core::DepthLayer::Terrain),bar(elysia::core::DepthLayer::Terrain);anchor.set_world_rect({0,0,1,1});bar.set_world_rect({60,0,120,12});
    PhysicsWorld world({.units_per_meter=32,.gravity={0,0}});
    Collider pin;pin.shape=CircleShape{{0,0},1};pin.filter={.category=0,.mask=0};pin.response=CollisionResponse::Ignore;
    Collider plank;plank.shape=AabbShape{{-60,-6,120,12}};
    const auto a=world.register_object(anchor,BodyDefinition{.type=BodyType::Static},std::span(&pin,1));
    const auto b=world.register_object(bar,BodyDefinition{.type=BodyType::Dynamic,.mass_policy=MassPolicy::ExplicitMass,.mass=1,.fixed_rotation=false},std::span(&plank,1));
    check(a.is_valid()&&b.is_valid(),"Joint test bodies failed registration");
    const auto joint=world.create_revolute_joint({.first=a,.second=b,.local_anchor_second={-60,0},.enable_motor=true,.motor_speed=1.5f,.max_motor_torque=100000});
    check(joint.is_valid(),"Public revolute joint rejected");
    for(int i=0;i<120;++i)world.step(1./60);
    const auto state=world.body_state(b);const auto constraint=world.joint_state(joint);check(state&&constraint,"Joint state disappeared");
    check(std::abs(state->angle)>1,"Motor failed to rotate bar");
    const auto offset=constraint->anchor_first-constraint->anchor_second;check(std::hypot(offset.x,offset.y)<1,"Revolute anchor drifted");
    std::cout<<"PASS: 388 motion profiles, 65 bar anchors, 102 area targets; engine motor and pivot constraints under 120 physics steps\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
