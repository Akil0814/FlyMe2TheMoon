#include "flight_objects.h"
#include "engine/resources/resource_service.h"
#include "engine/animation/animation_service.h"
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace game::play {
using namespace elysia::core;
using namespace elysia::physics;
Decor::Decor(const content::Json& data,bool foreground):GameObject(foreground?DepthLayer::Foreground:DepthLayer::Terrain) {
    if(!foreground) {
        RenderCommand bg; bg.type=RenderCommandType::FillRect;
        bg.command_rect={-2000,-2000,data.at("width").get<float>()+4000,data.at("height").get<float>()+4000};
        bg.color={14,22,49,255}; _commands.push_back(bg);
    }
    for(const auto& d:data.at("decor")) {
        if((d.at("layer")=="Foreground")!=foreground || d.at("layer")=="Farground")continue;
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
    _colliders[0].shape=AabbShape{{0,-1.5f,std::max(length,0.1f),3}};
    _colliders[0].filter={.category=2,.mask=1};
    _colliders[0].material={.friction=0.05f,.restitution=0};
    _colliders[0].tag="terrain";
}
BodyDefinition Edge::body_definition()const {BodyDefinition b;b.type=BodyType::Static;b.angle=_angle;return b;}
Player::Player(Vec born):GameObject(DepthLayer::Character) {
    set_world_rect({born.x-24,born.y-48,48,64});
    _colliders[0].shape=CircleShape{{24,34},14};
    _colliders[0].filter={.category=1,.mask=2|4|8};
    _colliders[0].material={.friction=0.05f,.restitution=0};
    _colliders[0].tag="player";
    for(const std::string name:{"charIdle","charUp","charFree","charLeft","charRight","charDown","charDeath","charStand","charLookLeftRight"}) {
        auto anim=ELYSIA_ANIMATIONS->create_animation("fly.CharActions."+name);
        if(!anim)throw std::runtime_error("Missing player animation "+name);
        _animations.emplace(name,std::move(anim));
    }
}
BodyDefinition Player::body_definition()const {BodyDefinition b;b.type=BodyType::Dynamic;b.mass_policy=MassPolicy::ExplicitMass;b.mass=1;b.linear_damping=0.1f;b.bullet=true;return b;}
void Player::fixed_update(double delta) {
    auto* world=physics_world();if(!world)return;
    Vec v=velocity();v.x=std::clamp(v.x,-160.0f,160.0f);v.y=std::clamp(v.y,-420.0f,420.0f);
    v.x*=static_cast<float>(std::exp(-delta));set_velocity(v);
    if(!world->apply_force(physics_handle(),{thrust.x*896,thrust.y*896}))throw std::runtime_error("Player force rejected");
}
void Player::update(double delta) {
    const Vec v=velocity();
    look_time=std::max(0.,look_time-delta);
    _action=dead?"charDeath":look_time>0?"charLookLeftRight":thrust.y<-.1f?(thrust.x<-.2f?"charLeft":thrust.x>.2f?"charRight":"charUp"):
        std::abs(v.y)>35?"charFree":idle_enabled?"charIdle":"charStand";
    _animations.at(_action)->update(delta);
}
void Player::submit_render_commands(std::vector<RenderCommand>& out) const {
    RenderCommand cmd;if(_animations.at(_action)->build_render_command(render_rect(),0,SpriteFlip::None,cmd))out.push_back(cmd);
}
Entity::Entity(const content::Json& d):GameObject(d.at("item").get<bool>()?DepthLayer::Item:DepthLayer::EffectBack),
    kind(d.at("kind")),id(d.at("id")),item(d.at("item")),info(d.at("info")),origin{d.at("x"),d.at("y")},angle(d.at("rotation")),_motion(d.at("info")) {
    const float w=d.at("w"),h=d.at("h");set_world_rect({origin.x,origin.y,w,h});
    std::string key=d.at("key");
    _animation=ELYSIA_ANIMATIONS->create_animation(key);
    const auto& sizes=content::catalog().at("sizes");
    _visual_size={w,h};
    if(sizes.contains(key)&&_animation) {_visual_size={sizes.at(key)[0],sizes.at(key)[1]};}
    _texture=ELYSIA_RESOURCES->find_texture(key);
    Collider c;c.filter={.category=item||kind==101?4u:8u,.mask=1};
    c.response=item||kind==101||kind==111||kind==115||kind==116||kind==8||kind==24||kind==25?CollisionResponse::Overlap:CollisionResponse::Block;
    if(item||kind==101)c.shape=CircleShape{{0,0},kind==101?26.f:kind==3?18.f:10.f};
    else c.shape=AabbShape{{-w/2+2,-h/2+2,std::max(1.f,w-4),std::max(1.f,h-4)}};
    c.tag=item?"pickup":kind==101?"moon":"hazard";_colliders.push_back(c);
    if(kind>=105&&kind<=107){
        const auto spikes=info.value("Spikes",std::string("0000"));
        for(int side=0;side<4&&side<spikes.size();++side)if(spikes[side]=='1'){
            Collider spike;spike.filter={.category=8,.mask=1};spike.response=CollisionResponse::Overlap;spike.tag="spike";
            if(side==0)spike.shape=AabbShape{{-w/2,-h/2-8,w,8}};
            if(side==1)spike.shape=AabbShape{{w/2,-h/2,8,h}};
            if(side==2)spike.shape=AabbShape{{-w/2,h/2,w,8}};
            if(side==3)spike.shape=AabbShape{{-w/2-8,-h/2,8,h}};
            _colliders.push_back(spike);
            const float length=(side%2==0)?w:h;
            for(float at=8;at<length;at+=16)_spike_positions.push_back(side==0?Vec{-w/2+at,-h/2-4}:side==1?Vec{w/2+4,-h/2+at}:side==2?Vec{-w/2+at,h/2+4}:Vec{-w/2-4,-h/2+at});
        }
        _spike_texture=ELYSIA_RESOURCES->find_texture("fly.DynamicObjectsLite.DynamicObject_111.first");
    }
}
BodyDefinition Entity::body_definition()const {
    BodyDefinition b;b.type=kind>=105&&kind<=107?BodyType::Kinematic:rotating()?BodyType::Dynamic:BodyType::Static;
    b.angle=angle*std::numbers::pi_v<float>/180;
    if(rotating()){b.fixed_rotation=false;b.mass_policy=MassPolicy::ExplicitMass;b.mass=static_cast<float>(parameter(info,"Mass",1));b.angular_damping=static_cast<float>(parameter(info,"AngularDamping",3));}
    return b;
}
Rect Entity::render_rect()const noexcept {const Vec p=position();return {p.x-size().x/2,p.y-size().y/2,size().x,size().y};}
void Entity::fixed_update(double delta) {
    _time+=delta;
    if(kind>=105&&kind<=107){
        const double part=_motion.fraction(_time);const Vec target=origin+Vec{static_cast<float>(parameter(info,"X")*part),static_cast<float>(-parameter(info,"Y")*part)};
        set_velocity((target-position())/static_cast<float>(delta));
    }
}
bool Entity::lethal(ColliderId hit)const {
    if(kind==111)return hit==physics_collider(0);
    for(size_t i=1;i<_colliders.size();++i)if(hit==physics_collider(i))return true;
    return false;
}
void Entity::update(double delta) {if(_animation)_animation->update(delta);}
void Entity::submit_render_commands(std::vector<RenderCommand>& out) const {
    if(collected)return;
    Rect r=render_rect();const auto c=r.center();r={c.x-_visual_size.x/2,c.y-_visual_size.y/2,_visual_size.x,_visual_size.y};
    const float rotation=physics_state()?physics_state()->angle*180/std::numbers::pi_v<float>:angle;
    RenderCommand cmd;
    if(_animation) {if(_animation->build_render_command(r,rotation,SpriteFlip::None,cmd))out.push_back(cmd);}
    else if(_texture){cmd.texture=_texture;cmd.command_rect=r;cmd.rotation_degrees=rotation;out.push_back(cmd);}
    const float rad=rotation*std::numbers::pi_v<float>/180;
    for(Vec point:_spike_positions){const Vec p=position()+Vec{point.x*std::cos(rad)-point.y*std::sin(rad),point.x*std::sin(rad)+point.y*std::cos(rad)};
        RenderCommand s;s.texture=_spike_texture;s.command_rect={p.x-8,p.y-8,16,16};s.rotation_degrees=rotation;if(s.texture)out.push_back(s);}
}
Pivot::Pivot(Vec p):GameObject(DepthLayer::Terrain){set_world_rect({p.x,p.y,1,1});_colliders[0].shape=CircleShape{{0,0},1};_colliders[0].filter={.category=0,.mask=0};_colliders[0].response=CollisionResponse::Ignore;}
}
