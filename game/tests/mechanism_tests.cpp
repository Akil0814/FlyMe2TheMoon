#include "game/gameplay/motion_profile.h"
#include "game/gameplay/tutorial_replay.h"
#include "game/gameplay/flight_input.h"
#include "game/gameplay/role_geometry.h"
#include "game/gameplay/wind_field.h"
#include "game/gameplay/shot_schedule.h"
#include "game/gameplay/seesaw_parameters.h"
#include "game/gameplay/laser_aim.h"
#include "game/gameplay/laser_geometry.h"
#include "game/gameplay/platform_parameters.h"
#include "game/gameplay/role_effect.h"
#include "game/gameplay/camera_move.h"
#include "engine/physics/physics_world.h"
#include "engine/core/game_object.h"
#include <fstream>
#include <iostream>
#include <format>
#include <set>
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main(){try{
    using namespace game::play;using namespace elysia::physics;using game::content::Json;
    CameraMove camera_move;
    check(camera_move.begin({0,0},{1000,0})==1&&camera_move.position()==elysia::core::Vector2{0,0},"Camera move must begin at the current focus");
    camera_move.advance(.5);check(std::abs(camera_move.position().x-707.10678f)<.001&&camera_move.active(),"Camera must use original CCEaseSineOut");
    const auto midway=camera_move.position();camera_move.begin(midway,{0,1000});check(camera_move.position()==midway,"Retargeting camera caused a jump");
    camera_move.advance(100);check(!camera_move.active()&&camera_move.position()==elysia::core::Vector2{0,1000},"Camera did not settle at the destination");
    check(camera_move.begin({0,1000},{0,1000})==0&&!camera_move.active(),"Zero-distance camera move must finish immediately");
    check(camera_move.begin({0,0},{0,1000},1)==2,"Opening hold plus distance duration incorrect");
    camera_move.advance(.75);check(camera_move.holding()&&camera_move.position()==elysia::core::Vector2{0,0},"Opening must hold on the moon for one second");
    camera_move.advance(.75);check(!camera_move.holding()&&std::abs(camera_move.position().y-707.10678f)<.001,"Opening pan lost time crossing the hold boundary");
    camera_move.advance(.5);check(!camera_move.active()&&camera_move.position()==elysia::core::Vector2{0,1000},"Opening pan did not finish at the role");
    RoleTransition transition;transition.begin_moon({0,0},{100,200});
    transition.advance(.6);check(transition.phase()==RoleTransitionPhase::MoonApproach&&std::abs(transition.position().x-100*std::sqrt(.5f))<.001,"Moon approach must use native CCEaseOut rate 2");
    transition.advance(.6);check(transition.phase()==RoleTransitionPhase::MoonWait&&transition.position()==elysia::core::Vector2{100,200},"Moon approach must finish at 1.2 seconds");
    transition.advance(1.199);check(transition.phase()==RoleTransitionPhase::MoonWait,"Moon callback ran before 2.4 seconds");
    transition.advance(.002);check(transition.phase()==RoleTransitionPhase::Complete,"Moon callback did not finish after 2.4 seconds");
    for(float distance:{0.f,100.f,1000.f,5000.f}) {
        transition.begin_portal({10,30},{0,0},{distance,0});
        check(transition.phase()==RoleTransitionPhase::PortalPull,"Portal did not start pulling");
        transition.advance(RoleTransition::kPull/2);check(std::abs(transition.scale()-.875f)<.001,"Portal pull scale/easing incorrect");
        transition.advance(RoleTransition::kPull/2+.0001);check(transition.phase()==RoleTransitionPhase::PortalTravel,"Portal camera started before pull completed");
        transition.advance(transition.travel+.137);check(transition.phase()==RoleTransitionPhase::PortalTravel,"Portal role emerged before callback margin elapsed");
        transition.advance(.002);check(transition.phase()==RoleTransitionPhase::PortalEmerge&&transition.camera_fraction()==1,"Portal exit / camera destination incorrect");
        transition.advance(.6);check(transition.phase()==RoleTransitionPhase::Complete&&transition.position()==elysia::core::Vector2{distance,0},"Portal did not restore its exit state");
    }
    check(LaserAim::in_range(Json{},1000)&&!LaserAim::in_range(Json{},1001)&&LaserAim::in_range(Json{{"ActiveRange","-1"}},50000),"Laser default range / unlimited sentinel incorrect");
    LaserAim tracking(Json{});
    const auto muzzle=laser_muzzle_offset(std::numbers::pi_v<float>/2,{41.5f,45});
    check(std::abs(muzzle.x)<.5f&&std::abs(muzzle.y+17.05f)<.1f,"Original vertical muzzle table / image coordinate conversion incorrect");
    check(std::abs(tracking.update(Json{},1,.1,0,{0,-100})+std::numbers::pi_v<float>/4)<.001,"Laser must track an overhead target with original 5*dt smoothing");
    LaserAim rear(Json{});
    check(rear.update(Json{},1,.1,0,{100,100})==0,"Laser tracking must clamp to its forward semicircle");
    const Json sweep{{"Cycle","4"},{"MinAngle","30"},{"MaxAngle","150"},{"Lag","2"}};
    LaserAim scanning(sweep);
    check(scanning.update(sweep,1,.1,0,{})==0&&std::abs(scanning.update(sweep,2.5,.1,0,{})+std::numbers::pi_v<float>/3)<.001,"Laser lag / default linear sweep incorrect");
    check(seesaw_width(108)==64&&seesaw_width(109)==128&&seesaw_width(110)==256&&seesaw_width(117)==512,"Original plank dimensions changed");
    check(seesaw_anchor(109,Json{{"Anchor","1"}})==60&&seesaw_anchor(108,Json{{"Anchor","0"}})==-28,"Plank pivot must stay four units inside its end cap");
    check(std::abs(seesaw_speed(Json{{"Speed","-90"}})-std::numbers::pi_v<float>/2)<.001,"Original Y-up motor speed was not converted to Y-down");
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
    check(force.x==-320&&std::abs(rotated.y+320)<.001&&std::abs(rotated.x)<.001,"Original wind must point left at zero degrees and upward at 90");
    check(wind_force({0,100},{0,0},{256,128},0,wind_info).length_squared()==0&&wind_force({140,0},{0,0},{256,128},0,wind_info).length_squared()==0,"Wind leaked beyond all six envelope sample points");
    check(std::abs(wind_force({128,0},{0,0},{256,128},0,wind_info).x+160)<.001,"Half of the six wind samples should produce half force");
    check(wind_force({0,0},{0,0},{256,128},0,Json{}).x==-640,"Original default wind Strength must be 20");
    check(wind_force({0,0},{0,0},{256,128},0,wind_info)==wind_force({0,0},{0,0},{256,128},0,Json{{"Strength","10"},{"Fade","00"}}),"Visual Fade flags must not change wind force");
    ShotSchedule volley(Json{{"Seq","0,0.05,0.1,0.15,2"},{"Lag","1"}});
    int shots=0;for(int tick=0;tick<60;++tick)shots+=volley.advance(1./60);
    check(shots<=1,"Firing occurred before configured lag");
    for(int tick=0;tick<12;++tick)shots+=volley.advance(1./60);
    check(shots==4,"Burst schedule missed closely spaced shots");
    for(int tick=0;tick<120;++tick)shots+=volley.advance(1./60);
    check(shots==8,"Burst repetition failed");
    MotionProfile gate(Json{{"Cycle","4"},{"Lag","99"},{"PauseHalf","2"},{"PauseEnd","3"},{"Rate","1"}});
    check(gate.fraction(98)==0&&gate.fraction(100)==.5&&gate.fraction(102)==1&&gate.fraction(104)==.5&&gate.fraction(106)==0&&gate.fraction(108)==0,"Gate travel, endpoint dwell, or cycle timing failed");
    check(gate.speed_ratio(98)==0&&gate.speed_ratio(100)==.5&&gate.speed_ratio(102)==0&&gate.speed_ratio(104)==-.5&&gate.speed_ratio(106)==0,"Platform velocity lag / direction / dwell incorrect");
    MotionProfile eased(Json{{"Cycle","4"},{"Rate","2"}});
    check(eased.speed_ratio(.5)==.5&&eased.speed_ratio(1)==1&&eased.speed_ratio(1.5)==.5&&eased.speed_ratio(2.5)==-.5,"Original platform quarter-cycle acceleration incorrect");
    for(int kind:{105,106,107}) {
        PhysicsWorld platform_world({.units_per_meter=32,.gravity={0,320}});
        elysia::core::GameObject pin(elysia::core::DepthLayer::Terrain),platform(elysia::core::DepthLayer::Terrain),obstacle(elysia::core::DepthLayer::Terrain);
        pin.set_world_rect({0,0,1,1});platform.set_world_rect({0,0,1,1});obstacle.set_world_rect({80+platform_width(kind)/2,0,1,1});
        Collider anchor_shape;anchor_shape.shape=CircleShape{{},1};anchor_shape.response=CollisionResponse::Ignore;
        const auto anchor_handle=platform_world.register_object(pin,BodyDefinition{.type=BodyType::Static},std::span(&anchor_shape,1));
        auto shape=platform_collider(kind);
        const auto body=platform_world.register_object(platform,BodyDefinition{.type=BodyType::Dynamic,.fixed_rotation=false},std::span(&shape,1));
        check(std::abs(platform_world.body_state(body)->mass-(kind-104))<.001f,"Platform density / original full dimensions incorrect");
        auto definition=platform_joint(kind,Json{{"X","200"}},0);definition.first=anchor_handle;definition.second=body;
        definition.settings.motor_speed=40;
        const auto handle=platform_world.create_prismatic_joint(definition);
        check(handle.is_valid(),"Platform prismatic joint rejected");
        Collider wall;wall.shape=BoxShape::from_rect({0,-100,16,200});wall.filter={.category=2,.mask=8};
        const auto wall_handle=platform_world.register_object(obstacle,BodyDefinition{.type=BodyType::Static},std::span(&wall,1));
        for(int tick=0;tick<240;++tick)platform_world.step(1./60);
        const auto stopped=platform_world.body_state(body);
        const auto joint=std::get<PrismaticJointState>(platform_world.joint_state(handle)->details);
        std::cout<<"Platform "<<kind<<" stalled at "<<stopped->position.x<<","<<stopped->position.y<<" force "<<joint.motor_force<<"\n";
        check(stopped->position.x>75&&stopped->position.x<81&&std::abs(stopped->position.y)<.1f,"Dynamic platform penetrated obstacle or escaped its rail");
        check(std::abs(joint.motor_force)<=definition.settings.max_motor_force+1&&std::abs(joint.motor_force)>100,"Platform stalled motor must exert bounded force");
        platform_world.unregister_object(wall_handle);
        for(int tick=0;tick<60;++tick)platform_world.step(1./60);
        const auto resumed=platform_world.body_state(body);
        check(resumed->position.x-stopped->position.x>38&&resumed->position.x-stopped->position.x<41,"Platform must resume at motor speed without catching up to a target pose");
        definition.settings.motor_speed=-40;check(platform_world.update_prismatic_joint(handle,definition.settings),"Platform motor reversal rejected");
        for(int tick=0;tick<240;++tick)platform_world.step(1./60);
        check(std::abs(platform_world.body_state(body)->position.x)<1,"Platform did not reverse and stop at lower rail limit");
    }
    std::cout<<"PASS: all 3 platform widths, density mass, rail constraints, bounded stall force, release and reversal\n";
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
        int overrides=0;
        for(int level:{25,30,42}) {
            std::ifstream file(std::format("assets/configs/fly/levels/{:03}.json",level));
            for(const auto& entity:Json::parse(file).at("entities")) {
                if(!entity.at("info").contains("Mass"))continue;
                ++overrides;
                PhysicsWorld mass_world({.units_per_meter=32,.gravity={0,0}});
                elysia::core::GameObject body(elysia::core::DepthLayer::Terrain);
                body.set_world_rect({0,0,256,8});
                Collider shape;shape.shape=BoxShape::from_rect({-128,-4,256,8});shape.density={.5f};
                const float mass=static_cast<float>(parameter(entity.at("info"),"Mass"));
                const auto handle=mass_world.register_object(body,BodyDefinition{.type=BodyType::Dynamic,
                    .mass_properties=seesaw_mass_properties(entity.at("kind"),entity.at("info")),.fixed_rotation=false},std::span(&shape,1));
                check(handle.is_valid(),"Original mass override rejected");
                auto state=mass_world.body_state(handle);
                check(state&&std::abs(state->mass-mass)<.0001f&&state->local_center.length_squared()==0,
                    "Seesaw mass or center differs from original; joint anchor must not offset center of mass");
                constexpr float inertia=5466.666667f;
                check(std::abs(state->rotational_inertia-inertia)<.1f,"Seesaw explicit mass must retain density-derived inertia");
                check(mass_world.apply_angular_impulse(handle,inertia)&&mass_world.apply_impulse(handle,{mass*10,0}),"Impulse rejected");
                state=mass_world.body_state(handle);
                check(std::abs(state->angular_velocity-1)<.001f&&std::abs(state->velocity.x-10)<.001f,
                    "Seesaw independent linear / angular impulse response incorrect");
            }
        }
        check(overrides==7,"Original explicit-mass seesaw coverage changed");
        check(std::holds_alternative<DensityMass>(seesaw_mass_properties(109,Json{})),"Default seesaw should retain density mass");
        std::cout<<"PASS: all 7 original mass overrides retain 5466.67 kg*EU^2 inertia and independent impulse responses\n";
    }
    {
        PhysicsWorld flight({.units_per_meter=32,.gravity={0,320}});
        elysia::core::GameObject role(elysia::core::DepthLayer::Character),floor(elysia::core::DepthLayer::Terrain);
        role.set_world_rect({0,-80,48,64});floor.set_world_rect({0,0,1,1});
        std::array<Collider,3> shapes;
        shapes[0].shape=CircleShape{{0,0},17.92f};shapes[1].shape=role_upper_shape();shapes[2].shape=role_sensor_shape();
        shapes[1].density={0};shapes[2].response=CollisionResponse::Overlap;
        const auto r=flight.register_object(role,BodyDefinition{.type=BodyType::Dynamic,.mass_properties=ExplicitMass{1},.linear_damping=.1f},shapes);
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
    {
        PhysicsWorld family({.units_per_meter=32,.gravity={0,0}});
        elysia::core::GameObject carrier(elysia::core::DepthLayer::Terrain),attachment(elysia::core::DepthLayer::Terrain),role(elysia::core::DepthLayer::Character);
        carrier.set_world_rect({0,0,64,8});attachment.set_world_rect({0,0,16,16});role.set_world_rect({24,0,1,1});
        Collider bar_shape;bar_shape.shape=BoxShape::from_rect({-32,-4,64,8});bar_shape.filter={.category=8,.mask=1|2|8,.group=-3};
        Collider carried;carried.shape=BoxShape::from_rect({-8,-8,16,16});carried.filter=bar_shape.filter;
        Collider player_shape;player_shape.shape=CircleShape{{},8};player_shape.filter={.category=1,.mask=8};
        const auto bar=family.register_object(carrier,BodyDefinition{.type=BodyType::Dynamic,.fixed_rotation=false},std::span(&bar_shape,1));
        const auto child=family.register_object(attachment,BodyDefinition{.type=BodyType::Kinematic},std::span(&carried,1));
        const auto player=family.register_object(role,BodyDefinition{.type=BodyType::Dynamic},std::span(&player_shape,1));
        check(bar.is_valid()&&child.is_valid()&&player.is_valid(),"Carrier family test registration failed");
        family.step(1./60);
        bool external_contact=false;
        std::vector<CollisionContact> contacts;
        family.collect_contacts(CollisionTarget::from_collider(family.collider_id(bar,0)),contacts);
        for(const auto& contact:contacts) {
            const auto a=contact.pair.first.collider,b=contact.pair.second.collider;
            check(!((a==family.collider_id(bar,0)&&b==family.collider_id(child,0))||(b==family.collider_id(bar,0)&&a==family.collider_id(child,0))),"Attached object collided with its own carrier");
            external_contact|=a==family.collider_id(player,0)||b==family.collider_id(player,0);
        }
        check(external_contact,"Carrier family exclusion incorrectly disabled player contact");
    }
    elysia::core::GameObject anchor(elysia::core::DepthLayer::Terrain),bar(elysia::core::DepthLayer::Terrain);anchor.set_world_rect({0,0,1,1});bar.set_world_rect({60,0,120,12});
    PhysicsWorld world({.units_per_meter=32,.gravity={0,0}});
    Collider pin;pin.shape=CircleShape{{0,0},1};pin.filter={.category=0,.mask=0};pin.response=CollisionResponse::Ignore;
    Collider plank;plank.shape=BoxShape::from_rect({-60,-6,120,12});
    const auto a=world.register_object(anchor,BodyDefinition{.type=BodyType::Static},std::span(&pin,1));
    const auto b=world.register_object(bar,BodyDefinition{.type=BodyType::Dynamic,.mass_properties=ExplicitMass{1},.fixed_rotation=false},std::span(&plank,1));
    check(a.is_valid()&&b.is_valid(),"Joint test bodies failed registration");
    const auto joint=world.create_revolute_joint({.first=a,.second=b,.local_anchor_second={-60,0},.settings={.enable_motor=true,.motor_speed=1.5f,.max_motor_torque=100000}});
    check(joint.is_valid(),"Public revolute joint rejected");
    for(int i=0;i<120;++i)world.step(1./60);
    const auto state=world.body_state(b);const auto constraint=world.joint_state(joint);check(state&&constraint,"Joint state disappeared");
    check(std::abs(state->angle)>1,"Motor failed to rotate bar");
    const auto offset=constraint->anchor_first-constraint->anchor_second;check(std::hypot(offset.x,offset.y)<1,"Revolute anchor drifted");
    std::cout<<"PASS: 388 motion profiles, 65 bar anchors, 102 area targets; engine motor and pivot constraints under 120 physics steps\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
