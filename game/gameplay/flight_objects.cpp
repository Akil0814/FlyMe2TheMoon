#include "flight_objects.h"
#include "engine/resources/resource_service.h"
#include "engine/animation/animation_service.h"
#include <cmath>
#include <numbers>
#include <stdexcept>
#include "wind_field.h"
#include "flight_input.h"
#include "role_geometry.h"
#include "seesaw_parameters.h"

namespace game::play {
using namespace elysia::core;
using namespace elysia::physics;
Parallax::Parallax(const content::Json& data,const elysia::camera::Camera* camera):GameObject(DepthLayer::Background),_world_size{data.at("width"),data.at("height")},_camera(camera) {
    // BackgroundLayer.load: in the supplied original selects these RGBA colors.
    switch(data.at("suite").get<int>()) {
        case 1:_sky_top={139,64,113,255};_sky_bottom={225,116,177,255};break;
        case 2:_sky_top={224,151,48,255};_sky_bottom={225,196,115,255};break;
        case 3:_sky_top={6,126,142,255};_sky_bottom={75,153,213,255};break;
        default:_sky_top={32,99,175,255};_sky_bottom={58,130,191,255};break;
    }
    for(const auto& d:data.at("decor"))if(d.at("layer")=="Farground"||d.at("layer")=="Background"){
        LayerImage image;image.extent={d.at("layerWidth"),d.at("layerHeight")};
        image.command.texture=ELYSIA_RESOURCES->find_texture(d.at("key").get<std::string>());
        if(!image.command.texture)throw std::runtime_error("Missing parallax texture");
        image.command.command_rect={d.at("x"),d.at("y").get<float>()-_world_size.y+image.extent.y,d.at("w"),d.at("h")};
        image.command.rotation_degrees=d.at("rotation");image.command.rotation_origin={0,1};
        image.command.flip=d.at("flip").get<bool>()?SpriteFlip::Horizontal:SpriteFlip::None;
        _images.push_back(image);
    }
}
void Parallax::submit_render_commands(std::vector<RenderCommand>& out)const {
    const auto view=_camera->view_rect();
    for(float y=view.y();y<view.y()+view.height();y+=4){
        const float fraction=std::clamp((y+2)/_world_size.y,0.f,1.f);
        const auto blend=[&](Uint8 a,Uint8 b){return static_cast<Uint8>(std::lround(a+(b-a)*fraction));};
        RenderCommand bg;bg.type=RenderCommandType::FillRect;bg.command_rect={view.x(),y,view.width(),std::min(4.f,view.y()+view.height()-y)};
        bg.color={blend(_sky_top.r,_sky_bottom.r),blend(_sky_top.g,_sky_bottom.g),blend(_sky_top.b,_sky_bottom.b),255};out.push_back(bg);
    }
    for(const auto& image:_images){auto cmd=image.command;
        const auto offset=[](float camera,float viewport,float layer,float world){
            const float travel=std::max(0.f,layer-viewport),range=std::max(0.f,world-viewport);
            const float scroll=range>0?std::clamp(camera/range,0.f,1.f)*travel:0;
            return camera+std::max(0.f,(viewport-layer)/2)-scroll;
        };
        cmd.command_rect.set_position(cmd.command_rect.position()+Vec{
            offset(view.x(),view.width(),image.extent.x,_world_size.x),
            offset(view.y(),view.height(),image.extent.y,_world_size.y)});out.push_back(cmd);
    }
}
Decor::Decor(const content::Json& data,bool foreground):GameObject(foreground?DepthLayer::Foreground:DepthLayer::Terrain) {
    for(const auto& d:data.at("decor")) {
        if((d.at("layer")=="Foreground")!=foreground || d.at("layer")=="Farground"||d.at("layer")=="Background")continue;
        RenderCommand cmd;cmd.texture=ELYSIA_RESOURCES->find_texture(d.at("key").get<std::string>());
        if(!cmd.texture)throw std::runtime_error("Missing level texture");
        cmd.command_rect={d.at("x"),d.at("y"),d.at("w"),d.at("h")};
        cmd.rotation_degrees=d.at("rotation");
        cmd.rotation_origin={0,1};
        cmd.flip=d.at("flip").get<bool>()?SpriteFlip::Horizontal:SpriteFlip::None;
        _commands.push_back(cmd);
    }
}
void Decor::submit_render_commands(std::vector<RenderCommand>& out) const {out.insert(out.end(),_commands.begin(),_commands.end());}
Edge::Edge(Vec a,Vec b):GameObject(DepthLayer::Terrain) {
    const float dx=b.x-a.x,dy=b.y-a.y,length=std::hypot(dx,dy);
    set_world_rect({a.x,a.y,1,1});_angle=std::atan2(dy,dx);
    _colliders[0].shape=SegmentShape{{0,0},{length,0}};
    _colliders[0].filter={.category=2,.mask=1|8};
    _colliders[0].material={.friction=0.05f,.restitution=0};
    _colliders[0].tag="terrain";
}
BodyDefinition Edge::body_definition()const {BodyDefinition b;b.type=BodyType::Static;b.angle=_angle;return b;}
Player::Player(Vec born):GameObject(DepthLayer::Character) {
    const auto anchor=role_spawn(born);set_world_rect({anchor.x,anchor.y,48,64});
    _colliders[0].shape=CircleShape{{0,0},17.92f};
    _colliders[1].shape=role_upper_shape();
    _colliders[2].shape=role_sensor_shape();_colliders[2].response=CollisionResponse::Overlap;
    for(auto& collider:_colliders){collider.filter={.category=1,.mask=2|4|8};collider.material={.friction=.2f,.restitution=0};collider.tag="player";}
    _colliders[0].material.friction=1;
    _colliders[0].density={1.27324f};_colliders[1].density={0};_colliders[2].density={0};
    for(const std::string name:{"charIdle","charUp","charFree","charLeft","charRight","charDown","charDeath","charStand","charLookLeftRight"}) {
        auto anim=ELYSIA_ANIMATIONS->create_animation("fly.CharActions."+name);
        if(!anim)throw std::runtime_error("Missing player animation "+name);
        _animations.emplace(name,std::move(anim));
    }
}
BodyDefinition Player::body_definition()const {BodyDefinition b;b.type=BodyType::Dynamic;b.mass_policy=MassPolicy::ExplicitMass;b.mass=1;b.linear_damping=0.1f;b.bullet=true;return b;}
void Player::fixed_update(double delta) {
    auto* world=physics_world();if(!world)return;
    const auto& config=content::catalog().at("role");
    const auto force=thrust_force(thrust,velocity(),config);
    set_velocity(damp_velocity(velocity(),force.length_squared()>0,static_cast<float>(delta),config));
    if(!world->apply_force(physics_handle(),force))throw std::runtime_error("Player force rejected");
}
Rect Player::render_rect()const noexcept {const auto p=GameObject::render_rect().position();return {p.x-24,p.y-45,48,64};}
bool Player::owns_collider(ColliderId id)const noexcept {for(size_t i=0;i<_colliders.size();++i)if(physics_collider(i)==id)return true;return false;}
void Player::update(double delta) {
    const Vec v=velocity();
    look_time=std::max(0.,look_time-delta);
    _action=dead?"charDeath":look_time>0?"charLookLeftRight":thrust.y<-.1f?(thrust.x<-.2f?"charLeft":thrust.x>.2f?"charRight":"charUp"):
        std::abs(v.y)>35?"charFree":idle_enabled?"charIdle":"charStand";
    _animations.at(_action)->update(delta);
}
void Player::die(){dead=true;thrust={};_action="charDeath";_animations.at(_action)->reset();_animations.at(_action)->set_loop(false);set_update_when_paused(true);}
void Player::submit_render_commands(std::vector<RenderCommand>& out) const {
    RenderCommand cmd;if(_animations.at(_action)->build_render_command(render_rect(),0,SpriteFlip::None,cmd))out.push_back(cmd);
}
Entity::Entity(const content::Json& d):GameObject(d.at("item").get<bool>()?DepthLayer::Item:DepthLayer::EffectBack),
    kind(d.at("kind")),id(d.at("id")),item(d.at("item")),info(d.at("info")),origin{d.at("x"),d.at("y")},angle(d.at("rotation")),_motion(d.at("info")),_laser_aim(d.at("info")) {
    float w=d.at("w"),h=d.at("h");
    if(wind()) {
        const Vec anchor=origin+Vec{-w/2,h/2};
        const float blocks=static_cast<float>(parameter(info,"NumBlocks",1));
        w=64*blocks;
        // Original extra blocks extend to the left of the first 64-unit cell.
        origin=anchor+rotate_vector({64-w/2,-h/2},angle*std::numbers::pi_v<float>/180);
    }
    set_world_rect({origin.x,origin.y,w,h});
    std::string key=d.at("key");
    _animation=ELYSIA_ANIMATIONS->create_animation(key);
    const auto& sizes=content::catalog().at("sizes");
    _visual_size={w,h};
    if(sizes.contains(key)&&_animation) {_visual_size={sizes.at(key)[0],sizes.at(key)[1]};}
    _texture=ELYSIA_RESOURCES->find_texture(key);
    if(kind==112||kind==113)_shots.emplace(info);
    if(kind==116){_bomb_armed=ELYSIA_ANIMATIONS->create_animation(key+".S03");_bomb_blast=ELYSIA_ANIMATIONS->create_animation(key+".S04");}
    Collider c;c.filter={.category=item||kind==101?4u:8u,.mask=1};
    c.response=item||kind==101||kind==111||kind==115||kind==116||kind==8||kind==24||kind==25?CollisionResponse::Overlap:CollisionResponse::Block;
    if(item||kind==101)c.shape=CircleShape{{0,0},kind==101?26.f:kind==3?18.f:10.f};
    else c.shape=BoxShape::from_rect({-w/2+2,-h/2+2,std::max(1.f,w-4),std::max(1.f,h-4)});
    if(c.response==CollisionResponse::Block)c.filter.mask=1|2|8;
    if(rotating()){
        const float width=seesaw_width(kind);c.shape=BoxShape::from_rect({-width/2,-4,width,8});
        c.density={.5f};c.material={.friction=.8f,.restitution=0};
    }
    if(wind())c.shape=BoxShape::from_rect({-w/2,-h/2,w,h});
    c.tag=item?"pickup":kind==101?"moon":"hazard";_colliders.push_back(c);
    if(kind>=105&&kind<=107){
        const auto spikes=info.value("Spikes",std::string("0000"));
        // DynamicBox's original string order is East, North, West, South.
        constexpr int sides[]{1,0,3,2};
        for(int index=0;index<4&&index<spikes.size();++index)if(spikes[index]=='1'){
            const int side=sides[index];
            Collider spike;spike.filter={.category=8,.mask=1};spike.response=CollisionResponse::Overlap;spike.tag="spike";
            if(side==0)spike.shape=BoxShape::from_rect({-w/2,-h/2-8,w,8});
            if(side==1)spike.shape=BoxShape::from_rect({w/2,-h/2,8,h});
            if(side==2)spike.shape=BoxShape::from_rect({-w/2,h/2,w,8});
            if(side==3)spike.shape=BoxShape::from_rect({-w/2-8,-h/2,8,h});
            _colliders.push_back(spike);
            const float length=(side%2==0)?w:h;
            for(float at=8;at<length;at+=16)_spike_positions.push_back(side==0?Vec{-w/2+at,-h/2-4}:side==1?Vec{w/2+4,-h/2+at}:side==2?Vec{-w/2+at,h/2+4}:Vec{-w/2-4,-h/2+at});
        }
        _spike_texture=ELYSIA_RESOURCES->find_texture("fly.DynamicObjectsLite.DynamicObject_111.first");
    }
}
BodyDefinition Entity::body_definition()const {
    BodyDefinition b;b.type=(kind>=105&&kind<=107)||info.contains("CarrierID")?BodyType::Kinematic:rotating()?BodyType::Dynamic:BodyType::Static;
    if(info.contains("CarrierID"))b.fixed_rotation=false;
    b.angle=angle*std::numbers::pi_v<float>/180;
    if(rotating()){
        b.fixed_rotation=false;b.angular_damping=static_cast<float>(parameter(info,"AngularDamping",1.6));
        const float mass=static_cast<float>(parameter(info,"Mass",-1));
        if(mass>0){b.mass_policy=MassPolicy::ExplicitMass;b.mass=mass;}
    }
    return b;
}
Rect Entity::render_rect()const noexcept {const Vec p=position();return {p.x-size().x/2,p.y-size().y/2,size().x,size().y};}
void Entity::fixed_update(double delta) {
    _time+=delta;
    if(_carrier) {
        const float carrier_angle=_carrier->physics_state()->angle;
        const Vec target=_carrier->position()+rotate_vector(_carrier_offset,carrier_angle);
        if(!set_target_transform({target,carrier_angle+_carrier_angle},delta))throw std::runtime_error("Carrier target rejected");
        return;
    }
    if(kind>=105&&kind<=107){
        const double part=_motion.fraction(_time);const Vec target=origin+Vec{static_cast<float>(parameter(info,"X")*part),static_cast<float>(-parameter(info,"Y")*part)};
        if(!set_target_transform({target,physics_state()->angle},delta))throw std::runtime_error("Platform target rejected");
    }
}
void Entity::attach_carrier(Entity* carrier) {
    _carrier=carrier;const float rad=carrier->physics_state()->angle;
    _carrier_offset=rotate_vector(origin-carrier->origin,-rad);
    _carrier_angle=angle*std::numbers::pi_v<float>/180-rad;
}
bool Entity::update_laser(double delta,Vec target,const Player& player) {
    _laser_active=false;if(kind!=114)return false;
    const float rad=_laser_aim.update(info,_time,delta,physics_state()->angle,player.position()-position());
    if(!LaserAim::in_range(info,(target-position()).length()))return false;
    const Vec direction{std::cos(rad),std::sin(rad)};
    _laser_start=position()+direction*24;_laser_end=_laser_start+direction*32000;
    std::vector<CollisionQueryHit> hits;
    physics_world()->raycast_all({.origin=_laser_start,.direction=direction,.max_distance=32000,.filter={.collision={.category=1|8,.mask=1|2|8}}},hits);
    _laser_active=true;
    for(const auto& hit:hits)if(hit.target.collider!=physics_collider(0)&&hit.response==CollisionResponse::Block){
        _laser_end=hit.point;return player.owns_collider(hit.target.collider);
    }
    return false;
}
int Entity::shots_due(double delta,Vec target) {
    if(!_shots)return 0;const int count=_shots->advance(delta);
    return (target-position()).length()<=parameter(info,"ActiveRange",800)?count:0;
}
Vec Entity::fire_direction(Vec target)const {
    if(info.contains("FixedAngle")){
        const float rad=physics_state()->angle-static_cast<float>(parameter(info,"FixedAngle"))*std::numbers::pi_v<float>/180;
        return {std::cos(rad),std::sin(rad)};
    }
    const Vec dir=target-position();return dir.length()>.01f?dir/dir.length():Vec{0,-1};
}
bool Entity::update_bomb(double delta,Vec target) {
    if(kind!=116||_bomb_state==3)return false;
    const auto overlaps_role=[&](float radius){std::vector<CollisionOverlapQueryHit> hits;physics_world()->overlap_shape({.shape=CircleShape{{0,0},radius},.pose={position(),0},.filter={.collision={.category=8,.mask=1}}},hits);return !hits.empty();};
    if(_bomb_state==0){if(overlaps_role(64)){_bomb_state=1;_bomb_time=0;}return false;}
    _bomb_time+=delta;
    // Original Bomb.explode tests the 1.25 m damage sensor once on detonation.
    if(_bomb_state==1&&_bomb_time>parameter(info,"Alarm",1)){_bomb_state=2;_bomb_time=0;return overlaps_role(40);}
    if(_bomb_state==2){
        if(_bomb_time>=27./60){_bomb_state=3;set_visible(false);physics_world()->set_body_enabled(physics_handle(),false);}
    }
    return false;
}
bool Entity::lethal(ColliderId hit)const {
    if(kind==111)return hit==physics_collider(0);
    for(size_t i=1;i<_colliders.size();++i)if(hit==physics_collider(i))return true;
    return false;
}
Vec Entity::wind_force_at(Vec point)const {return wind()?wind_force(point,position(),size(),angle*std::numbers::pi_v<float>/180,info):Vec{};}
void Entity::update(double delta) {if(_animation)_animation->update(delta);if(_bomb_armed)_bomb_armed->update(delta);}
void Entity::submit_render_commands(std::vector<RenderCommand>& out) const {
    if(collected)return;
    if(wind()) {
        const float rad=angle*std::numbers::pi_v<float>/180;
        const int count=std::max(4,static_cast<int>(size().x*size().y/2000));
        for(int i=0;i<count;++i){
            const float x=size().x/2-static_cast<float>(std::fmod(_time*90+i*37,size().x));
            const float y=-size().y/2+static_cast<float>(std::fmod(i*53+11,size().y));
            float fade=1;const std::string ends=info.value("Fade",std::string("00"));
            if(ends.size()>0&&ends[0]=='1')fade*=std::clamp((size().x/2-x)/24.f,0.f,1.f);
            if(ends.size()>1&&ends[1]=='1')fade*=std::clamp((size().x/2+x)/24.f,0.f,1.f);
            RenderCommand streak;streak.type=RenderCommandType::DrawLine;streak.color={150,213,255,static_cast<Uint8>(110*fade)};streak.stroke_width=1.5f;
            streak.line_start=position()+rotate_vector({x,y},rad);
            streak.line_end=position()+rotate_vector({std::max(-size().x/2,x-18),y},rad);out.push_back(streak);
        }
        return;
    }
    Rect r=render_rect();const auto c=r.center();r={c.x-_visual_size.x/2,c.y-_visual_size.y/2,_visual_size.x,_visual_size.y};
    const float rotation=physics_state()?physics_state()->angle*180/std::numbers::pi_v<float>:angle;
    RenderCommand cmd;
    if(kind==116&&_bomb_state==3)return;
    if(kind==116&&_bomb_state==2&&_bomb_blast){
        // Original 27-frame scale/opacity table at ARM address 0x1a04b8 (60 Hz).
        static constexpr float scale[]={.34f,.70f,.93f,1,.95f,.80f,.90f,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
        static constexpr Uint8 alpha[]={255,255,255,255,191,0,114,229,127,178,153,127,0,76,70,64,58,52,46,40,35,29,23,17,11,5,0};
        const int frame=std::clamp(static_cast<int>(_bomb_time*60),0,26);const float half=40*scale[frame];
        if(_bomb_blast->build_render_command({c.x-half,c.y-half,half*2,half*2},rotation,SpriteFlip::None,cmd)){cmd.alpha=alpha[frame];out.push_back(cmd);}
    }
    else if(kind==116&&_bomb_state==1&&_bomb_armed){if(_bomb_armed->build_render_command(r,rotation,SpriteFlip::None,cmd))out.push_back(cmd);}
    else if(_animation) {if(_animation->build_render_command(r,rotation,SpriteFlip::None,cmd))out.push_back(cmd);}
    else if(_texture){cmd.texture=_texture;cmd.command_rect=r;cmd.rotation_degrees=rotation;out.push_back(cmd);}
    if(_laser_active) {
        RenderCommand beam;beam.type=RenderCommandType::DrawLine;beam.line_start=_laser_start;beam.line_end=_laser_end;
        beam.color={221,88,255,130};beam.stroke_width=8;out.push_back(beam);
        beam.color={255,221,255,255};beam.stroke_width=2;out.push_back(beam);
    }
    const float rad=rotation*std::numbers::pi_v<float>/180;
    for(Vec point:_spike_positions){const Vec p=position()+Vec{point.x*std::cos(rad)-point.y*std::sin(rad),point.x*std::sin(rad)+point.y*std::cos(rad)};
        RenderCommand s;s.texture=_spike_texture;s.command_rect={p.x-8,p.y-8,16,16};s.rotation_degrees=rotation;if(s.texture)out.push_back(s);}
}
Pivot::Pivot(Vec p):GameObject(DepthLayer::Terrain){set_world_rect({p.x,p.y,1,1});_colliders[0].shape=CircleShape{{0,0},1};_colliders[0].filter={.category=0,.mask=0};_colliders[0].response=CollisionResponse::Ignore;}
}
