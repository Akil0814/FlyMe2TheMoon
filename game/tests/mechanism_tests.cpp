#include "game/gameplay/motion_profile.h"
#include "game/gameplay/tutorial_replay.h"
#include "game/gameplay/flight_input.h"
#include "game/gameplay/role_geometry.h"
#include "game/gameplay/wind_field.h"
#include "game/gameplay/shot_schedule.h"
#include "engine/physics/physics_world.h"
#include "engine/core/game_object.h"
#include <fstream>
#include <iostream>
#include <format>
#include <set>
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main(){try{
    using namespace game::play;using namespace elysia::physics;using game::content::Json;
    std::ifstream catalog_file("assets/configs/fly/catalog.json");const auto config=Json::parse(catalog_file).at("role");
    const auto vertical=pointer_thrust({0,0},{0,100},config);
    check(std::abs(vertical.x)<.001&&vertical.y<-.99,"Vertical input force incorrect");
    const auto tilted=pointer_thrust({464,1182},{508,1370},config);
    check(tilted.x<-.09&&tilted.x>-.12,"Original power curve must preserve small steering angles");
    check(pointer_thrust({0,0},{0,51.2f},config).length_squared()==0,"Touch inside inner radius must not thrust");
    check(std::abs(pointer_thrust({0,0},{0,57.6f},config).y+.5f)<.001,"Distance attenuation incorrect");
    const auto capped=thrust_force({0,-1},{0,-320},config);
    check(std::abs(capped.y+163.2f)<.01&&thrust_force({0,-1},{0,320},config).y==-896,"Power cap must limit positive work only");
    check(std::abs(damp_velocity({100,700},false,.1f,config).x-81)<.01&&damp_velocity({100,700},false,.1f,config).y==700,"Horizontal damping or vertical freedom changed");
    const auto left=pointer_thrust({0,0},{80,70},config),right=pointer_thrust({0,0},{-80,70},config),above=pointer_thrust({0,0},{0,-100},config);
    check(left.x<0&&right.x>0&&std::abs(left.x+right.x)<.001&&above.length_squared()==0,"Touch steering symmetry / upper rejection failed");
    std::ifstream recording("assets/configs/fly/tutorial.json");const auto tutorial=Json::parse(recording);
    for(const auto& [key,record]:tutorial.items()) {
        TutorialReplay replay;replay.load(record);replay.skip_idle();
        const auto expected=record.at("frames")[replay.cursor()];const auto input=replay.next(1440);
        check(input&&std::abs(input->x-expected[0].get<float>())<.001f&&std::abs(input->y-(1440-expected[1].get<float>()))<.001f,"Original replay coordinate conversion failed");
        while(!replay.ended())replay.next(1440);
        check(replay.cursor()==record.at("frames").size()&&!replay.next(1440),"Replay overran original input sequence");
    }
    check(tutorial.at("Rep_rd_1").at("frames").size()==718&&tutorial.at("Rep_rd_2").at("frames").size()==1422&&tutorial.at("Rep_rd_3").at("frames").size()==1103,"Tutorial record inventory changed");
    const Json wind_info{{"Strength","10"},{"Fade","11"}};
    check(std::abs(angle_difference(1.5f*std::numbers::pi_v<float>,-.5f*std::numbers::pi_v<float>))<.0001f,"Carrier pose must treat 270 and -90 degrees as identical");
    const auto force=wind_force({0,0},{0,0},{256,128},0,wind_info);
    const auto rotated=wind_force({0,0},{0,0},{256,128},std::numbers::pi_v<float>/2,wind_info);
    check(force.y==-320&&std::abs(rotated.x-320)<.001&&std::abs(rotated.y)<.001,"Wind direction or original strength scale failed");
    check(wind_force({0,64},{0,0},{256,128},0,wind_info).length_squared()==0&&wind_force({129,0},{0,0},{256,128},0,wind_info).length_squared()==0,"Wind leaked outside field / faded endpoint");
    ShotSchedule volley(Json{{"Seq","0,0.05,0.1,0.15,2"},{"Lag","1"}});
    int shots=0;for(int tick=0;tick<60;++tick)shots+=volley.advance(1./60);
    check(shots<=1,"Firing occurred before configured lag");
    for(int tick=0;tick<12;++tick)shots+=volley.advance(1./60);
    check(shots==4,"Burst schedule missed closely spaced shots");
    for(int tick=0;tick<120;++tick)shots+=volley.advance(1./60);
    check(shots==8,"Burst repetition failed");
    MotionProfile gate(Json{{"Cycle","4"},{"Lag","99"},{"PauseHalf","2"},{"PauseEnd","3"},{"Rate","1"}});
    check(gate.fraction(98)==0&&gate.fraction(100)==.5&&gate.fraction(102)==1&&gate.fraction(104)==.5&&gate.fraction(106)==0&&gate.fraction(108)==0,"Gate travel, endpoint dwell, or cycle timing failed");
    int moving=0,rotating=0,actions=0,carriers=0;
    for(int i=1;i<=60;++i){std::ifstream input(std::format("assets/configs/fly/levels/{:03}.json",i));auto data=Json::parse(input);std::set<int> targets;
        for(const auto& e:data.at("entities"))if(!e.at("item").get<bool>()){
            targets.insert(e.at("id").get<int>());const int k=e.at("kind");
            if(k>=105&&k<=107){++moving;MotionProfile m(e.at("info"));
                for(double time:{0.,m.lag,m.lag+m.cycle/4,m.lag+m.cycle/2,m.lag+m.cycle/2+m.pause_half,m.lag+m.cycle+m.pause_half,m.lag+m.cycle+m.pause_half+m.pause_end}){const double f=m.fraction(time);check(std::isfinite(f)&&f>=0&&f<=1,"Nonfinite / out-of-path platform pose");}}
            if(k==108||k==109||k==110||k==117){++rotating;const double anchor=parameter(e.at("info"),"Anchor",.5);check(anchor>=0&&anchor<=1,"Invalid rotational anchor");}
        }
        for(const auto& area:data.at("areas"))for(const auto& a:area.at("actions")){++actions;const auto name=a.at("Target").get<std::string>();check(name.starts_with("4:")&&targets.contains(std::stoi(name.substr(2))),"Area references missing dynamic object");check(a.at("Action")=="Start"&&parameter(a,"Delay")>=0,"Unsupported trigger action");}
        for(const auto& e:data.at("entities"))if(e.at("info").contains("CarrierID")){++carriers;check(targets.contains(static_cast<int>(parameter(e.at("info"),"CarrierID"))),"Carrier target absent");}
        for(const auto& e:data.at("entities"))if(e.at("kind")==112||e.at("kind")==113){ShotSchedule sequence(e.at("info"));int total=0;for(int tick=0;tick<3600;++tick)total+=sequence.advance(1./60);check(total>0&&total<2000,"Original firing sequence did not produce bounded fire over 60 seconds");}
    }
    check(moving==388&&rotating==65&&actions==102,"Original mechanism inventory changed");
    check(carriers==71,"Carrier inventory changed");
    {
        PhysicsWorld flight({.units_per_meter=32,.gravity={0,320}});
        elysia::core::GameObject role(elysia::core::DepthLayer::Character),floor(elysia::core::DepthLayer::Terrain);
        role.set_world_rect({0,-80,48,64});floor.set_world_rect({0,0,1,1});
        std::array<Collider,3> shapes;
        shapes[0].shape=CircleShape{{0,0},17.92f};shapes[1].shape=role_upper_shape();shapes[2].shape=role_sensor_shape();
        shapes[1].density={0};shapes[2].response=CollisionResponse::Overlap;
        const auto r=flight.register_object(role,BodyDefinition{.type=BodyType::Dynamic,.mass_policy=MassPolicy::ExplicitMass,.mass=1,.linear_damping=.1f},shapes);
        Collider ground;ground.shape=SegmentShape{{-1000,0},{1000,0}};
        const auto f=flight.register_object(floor,BodyDefinition{.type=BodyType::Static},std::span(&ground,1));
        check(r.is_valid()&&f.is_valid()&&flight.registered_collider_count()==4,"Original composite role or segment terrain rejected");
        for(int i=0;i<180;++i)flight.step(1./60);
        check(flight.contact_state(r).grounded&&std::abs(flight.body_state(r)->position.y+17.92f)<1,"Role foot did not settle on segment terrain");
        for(int i=0;i<900;++i){const auto v=flight.body_state(r)->velocity;flight.apply_force(r,thrust_force({0,-1},v,config));flight.step(1./60);}
        const auto speed=flight.body_state(r)->velocity.y;
        check(speed< -150&&speed> -165,"Power-limited flight did not reach original-model terminal speed");
        std::cout<<"PASS: original composite collider / segment contact; terminal upward velocity "<<speed<<" EU/s\n";
    }
    {
        PhysicsWorld beam_world({.units_per_meter=32,.gravity={0,0}});
        elysia::core::GameObject wall(elysia::core::DepthLayer::Terrain),role(elysia::core::DepthLayer::Character);
        wall.set_world_rect({100,-100,10,200});role.set_world_rect({200,0,1,1});
        Collider obstacle;obstacle.shape=BoxShape::from_rect({0,0,10,200});obstacle.filter={.category=2,.mask=1};
        Collider player;player.shape=CircleShape{{0,0},14};player.filter={.category=1,.mask=2|4|8};
        const auto w=beam_world.register_object(wall,BodyDefinition{},std::span(&obstacle,1));
        const auto p=beam_world.register_object(role,BodyDefinition{},std::span(&player,1));
        const RayCastQuery ray{.origin={0,0},.direction={1,0},.max_distance=1000,.filter={.collision={.category=1|8,.mask=1|2|8}}};
        auto hit=beam_world.raycast(ray);check(hit&&hit->target.collider==beam_world.collider_id(w,0)&&std::abs(hit->distance-100)<.01,"Terrain must occlude lethal laser");
        beam_world.unregister_object(w);hit=beam_world.raycast(ray);check(hit&&hit->target.collider==beam_world.collider_id(p,0),"Laser filter failed to see unoccluded player");
    }
    elysia::core::GameObject anchor(elysia::core::DepthLayer::Terrain),bar(elysia::core::DepthLayer::Terrain);anchor.set_world_rect({0,0,1,1});bar.set_world_rect({60,0,120,12});
    PhysicsWorld world({.units_per_meter=32,.gravity={0,0}});
    Collider pin;pin.shape=CircleShape{{0,0},1};pin.filter={.category=0,.mask=0};pin.response=CollisionResponse::Ignore;
    Collider plank;plank.shape=BoxShape::from_rect({-60,-6,120,12});
    const auto a=world.register_object(anchor,BodyDefinition{.type=BodyType::Static},std::span(&pin,1));
    const auto b=world.register_object(bar,BodyDefinition{.type=BodyType::Dynamic,.mass_policy=MassPolicy::ExplicitMass,.mass=1,.fixed_rotation=false},std::span(&plank,1));
    check(a.is_valid()&&b.is_valid(),"Joint test bodies failed registration");
    const auto joint=world.create_revolute_joint({.first=a,.second=b,.local_anchor_second={-60,0},.settings={.enable_motor=true,.motor_speed=1.5f,.max_motor_torque=100000}});
    check(joint.is_valid(),"Public revolute joint rejected");
    for(int i=0;i<120;++i)world.step(1./60);
    const auto state=world.body_state(b);const auto constraint=world.joint_state(joint);check(state&&constraint,"Joint state disappeared");
    check(std::abs(state->angle)>1,"Motor failed to rotate bar");
    const auto offset=constraint->anchor_first-constraint->anchor_second;check(std::hypot(offset.x,offset.y)<1,"Revolute anchor drifted");
    std::cout<<"PASS: 388 motion profiles, 65 bar anchors, 102 area targets; engine motor and pivot constraints under 120 physics steps\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
