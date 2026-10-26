#include "flight_objects.h"
#include "laser_geometry.h"
#include "engine/resources/resource_service.h"
#include "engine/animation/animation_service.h"
#include <cmath>
#include <numbers>
#include <stdexcept>
#include "wind_field.h"
#include "flight_input.h"
#include "role_geometry.h"
#include "seesaw_parameters.h"
#include "platform_parameters.h"
#include "launcher_visual.h"
#include "portal_visual.h"
#include "bomb_visual.h"
#include "decor_transform.h"
#include "moon_visual.h"
#include "thrower_visual.h"
#include "homing_parameters.h"

namespace game::play {
using namespace elysia::core;
using namespace elysia::physics;
namespace {
const elysia::resources::FrameInfo* mechanism_frame(const std::string& key,std::size_t index) {
    const auto* atlas=ELYSIA_RESOURCES->find_atlas(key);
    const auto* frame=atlas?atlas->frame_at(index):nullptr;
    if(!frame||!frame->_texture)throw std::runtime_error("Missing mechanism sprite "+key+" frame "+std::to_string(index));
    return frame;
}
void append_component(std::vector<RenderCommand>& out,const elysia::resources::FrameInfo& frame,
                      Vec anchor,Vec origin,float rotation) {
    const Vec size{frame._width*.5f,frame._height*.5f};
    RenderCommand cmd;cmd.texture=frame._texture;
    cmd.command_rect={anchor.x-origin.x*size.x,anchor.y-origin.y*size.y,size.x,size.y};
    cmd.rotation_origin=origin;cmd.rotation_degrees=rotation;
    cmd.use_src_rect=frame._source_rect.has_value();if(cmd.use_src_rect)cmd.src_rect=*frame._source_rect;
    out.push_back(cmd);
}
}
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
        image.command=decor_transform(d,{0,-_world_size.y+image.extent.y});
        image.command.texture=ELYSIA_RESOURCES->find_texture(d.at("key").get<std::string>());
        if(!image.command.texture)throw std::runtime_error("Missing parallax texture");
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
        RenderCommand cmd=decor_transform(d);cmd.texture=ELYSIA_RESOURCES->find_texture(d.at("key").get<std::string>());
        if(!cmd.texture)throw std::runtime_error("Missing level texture");
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
    _effect_frames[0]=mechanism_frame("fly.CharActions.charEffects.ball",0);
    for(int i=0;i<4;++i)_effect_frames[1+i]=mechanism_frame("fly.CharActions.charEffects.ballDisapear",i);
    for(int i=0;i<6;++i)_effect_frames[5+i]=mechanism_frame("fly.CharActions.charEffects.ballExplode",i);
    // Scene pause freezes gameplay, while the original role presentation keeps advancing.
    set_update_when_paused(true);
}
BodyDefinition Player::body_definition()const {BodyDefinition b;b.type=BodyType::Dynamic;b.mass_properties=ExplicitMass{1};b.linear_damping=0.1f;b.bullet=true;return b;}
void Player::fixed_update(double delta) {
    auto* world=physics_world();if(!world||_presentation||dead)return;
    const auto& config=content::catalog().at("role");
    const auto force=thrust_force(thrust,velocity(),config);
    set_velocity(damp_velocity(velocity(),force.length_squared()>0,static_cast<float>(delta),config));
    if(!world->apply_force(physics_handle(),force))throw std::runtime_error("Player force rejected");
}
Rect Player::render_rect()const noexcept {const auto p=_presentation.value_or(GameObject::render_rect().position()+Vec{0,-13});const float s=_presentation_scale;return {p.x-24*s,p.y-32*s,48*s,64*s};}
bool Player::owns_collider(ColliderId id)const noexcept {for(size_t i=0;i<_colliders.size();++i)if(physics_collider(i)==id)return true;return false;}
void Player::update(double delta) {
    _effect_time+=std::max(0.,delta);
    const Vec v=velocity();
    look_time=std::max(0.,look_time-delta);
    _action=dead?"charDeath":look_time>0?"charLookLeftRight":thrust.y<-.1f?(thrust.x<-.2f?"charLeft":thrust.x>.2f?"charRight":"charUp"):
        std::abs(v.y)>35?"charFree":idle_enabled?"charIdle":"charStand";
    _animations.at(_action)->update(delta);
    _thrust_particles.update(delta,render_rect(),thrust,thrust_attachment(),show_thrust());
}
void Player::play_effect(RoleEffect effect){_effect=effect;_effect_time=0;}
void Player::die(){if(dead)return;play_effect(RoleEffect::Death);present_at(std::nullopt);dead=true;thrust={};_action="charDeath";_animations.at(_action)->reset();_animations.at(_action)->set_loop(false);set_update_when_paused(true);}
void Player::submit_render_commands(std::vector<RenderCommand>& out) const {
    if(_presentation_hidden)return;
    const auto pose=effect_pose();
    if(show_thrust())_thrust_particles.submit(out);
    RenderCommand cmd;if(pose.body_visible&&_animations.at(_action)->build_render_command(render_rect(),0,SpriteFlip::None,cmd))out.push_back(cmd);
    if(pose.image!=BallImage::None) {
        const auto* frame=_effect_frames[pose.image==BallImage::Ball?0:(pose.image==BallImage::Disappear?1:5)+pose.frame];
        const auto rect=render_rect();const Vec center{rect.x()+rect.width()/2,rect.y()+rect.height()/2};
        const float size=90*pose.scale*_presentation_scale;
        cmd={};cmd.texture=frame->_texture;cmd.command_rect={center.x-size/2,center.y-size/2,size,size};
        cmd.use_src_rect=frame->_source_rect.has_value();if(cmd.use_src_rect)cmd.src_rect=*frame->_source_rect;
        out.push_back(cmd);
    }
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
    // Mechanism atlases contain state variants and component layers, not an
    // animation timeline. Select the base and draw attached parts separately.
    const bool platform=kind>=105&&kind<=107;
    const bool state_images=item||kind==101||platform||rotating()||kind==112||kind==113||kind==114||kind==115||kind==116||wind();
    if(!state_images)_animation=ELYSIA_ANIMATIONS->create_animation(key);
    const auto& sizes=content::catalog().at("sizes");
    _visual_size={w,h};
    if(sizes.contains(key)&&(_animation||state_images)) {_visual_size={sizes.at(key)[0],sizes.at(key)[1]};}
    _texture=ELYSIA_RESOURCES->find_texture(state_images?key+".first":key);
    if(state_images&&!_texture)throw std::runtime_error("Missing normal-state texture "+key);
    if(wind())_wind_visual.emplace(kind,info,*mechanism_frame(key,1),*mechanism_frame(key,2),id);
    if(kind==101){_moon_highlight_frame=mechanism_frame(key,1);set_update_when_paused(true);}
    if(platform&&parameter(info,"Lock")!=0)_texture=mechanism_frame(key,3)->_texture;
    if(rotating()) {
        const float anchor=static_cast<float>(parameter(info,"Anchor",.5)-.5);
        const float margin=16/seesaw_width(kind);
        if(anchor<-.5f+margin||anchor>.5f-margin) {
            _texture=mechanism_frame(key,1)->_texture;
            if(anchor>.5f-margin)_base_flip=SpriteFlip::Horizontal;
        }
        _pivot_frame=mechanism_frame(key+".S01",0);
    }
    if(kind==113){_leaf_frames[0]=mechanism_frame(key,2);_leaf_frames[1]=mechanism_frame(key,1);}
    if(kind==112){_thrower_head_frame=mechanism_frame(key+".S01",0);_thrower_orient=static_cast<float>(parameter(info,"FixedAngle",90))*std::numbers::pi_v<float>/180;}
    if(kind==114){_laser_visual.emplace(key,id);_laser_local_angle=static_cast<float>(parameter(info,"FixedAngle",90))*std::numbers::pi_v<float>/180;}
    if(kind==115) {
        const auto parts=key+(info.contains("TargetPortalID")?".S01":".S02");
        for(std::size_t i=0;i<_portal_frames.size();++i)_portal_frames[i]=mechanism_frame(parts,i);
    }
    if(kind==112)_shots.emplace(info);
    if(kind==113){auto shot_info=info;shot_info["Seq"]=info.value("Seq",std::string("1,2"));shot_info.erase("Lag");_shots.emplace(shot_info);}
    if(kind==116){_bomb_leaf_frame=mechanism_frame(key+".S02",0);_bomb_center_frame=mechanism_frame(key+".S01",0);_bomb_armed=ELYSIA_ANIMATIONS->create_animation(key+".S03");_bomb_blast=ELYSIA_ANIMATIONS->create_animation(key+".S04");}
    Collider c;c.filter={.category=item||kind==101?4u:8u,.mask=1};
    c.response=item||kind==101||kind==111||kind==115||kind==116||kind==8||kind==24||kind==25?CollisionResponse::Overlap:CollisionResponse::Block;
    if(item||kind==101)c.shape=CircleShape{{0,0},kind==101?26.f:kind==3?18.f:10.f};
    else c.shape=BoxShape::from_rect({-w/2+2,-h/2+2,std::max(1.f,w-4),std::max(1.f,h-4)});
    if(c.response==CollisionResponse::Block)c.filter.mask=1|2|8;
    if(platform)c=platform_collider(kind);
    if(rotating()){
        const float width=seesaw_width(kind);c.shape=BoxShape::from_rect({-width/2,-4,width,8});
        c.density={.5f};c.material={.friction=.8f,.restitution=0};
    }
    if(kind==114)c.shape=laser_base_shape({w,h});
    if(wind())c.shape=BoxShape::from_rect({-w/2,-h/2,w,h});
    c.tag=item?"pickup":kind==101?"moon":"hazard";_colliders.push_back(c);
    if(kind>=105&&kind<=107){
        w=platform_width(kind);h=32;
        const auto spikes=info.value("Spikes",std::string("0000"));
        // DynamicBox's original string order is East, North, West, South.
        constexpr int sides[]{1,0,3,2};
        for(int index=0;index<4&&index<spikes.size();++index)if(spikes[index]=='1'){
            const int side=sides[index];
            Collider spike;spike.filter={.category=8,.mask=1};spike.response=CollisionResponse::Overlap;spike.tag="spike";
            spike.density={0};
            if(side==0)spike.shape=BoxShape::from_rect({-w/2,-22,w,8});
            if(side==1)spike.shape=BoxShape::from_rect({w/2-2,-16,8,32});
            if(side==2)spike.shape=BoxShape::from_rect({-w/2,14,w,8});
            if(side==3)spike.shape=BoxShape::from_rect({-w/2-6,-16,8,32});
            _colliders.push_back(spike);
            const float length=(side%2==0)?w:h;
            for(float at=8;at<length;at+=16)_spike_positions.push_back(side==0?Vec{-w/2+at,-h/2-4}:side==1?Vec{w/2+4,-h/2+at}:side==2?Vec{-w/2+at,h/2+4}:Vec{-w/2-4,-h/2+at});
        }
        _spike_texture=ELYSIA_RESOURCES->find_texture("fly.DynamicObjectsLite.DynamicObject_111.first");
    }
}
BodyDefinition Entity::body_definition()const {
    BodyDefinition b;b.type=info.contains("CarrierID")?BodyType::Kinematic:moving()||rotating()?BodyType::Dynamic:BodyType::Static;
    if(moving())b.fixed_rotation=false;
    if(info.contains("CarrierID"))b.fixed_rotation=false;
    b.angle=angle*std::numbers::pi_v<float>/180;
    if(rotating()){
        b.fixed_rotation=false;b.angular_damping=static_cast<float>(parameter(info,"AngularDamping",1.6));
        // A continuously powered motor must remain awake across logic pauses.
        b.enable_sleep=seesaw_speed(info)==0;
        b.mass_properties=seesaw_mass_properties(kind,info);
    }
    return b;
}
Rect Entity::render_rect()const noexcept {const Vec p=GameObject::render_rect().position();return {p.x-size().x/2,p.y-size().y/2,size().x,size().y};}
void Entity::fixed_update(double delta) {
    _time+=delta;
    if(_carrier) {
        const float carrier_angle=_carrier->physics_state()->angle;
        const Vec target=_carrier->position()+rotate_vector(_carrier_offset,carrier_angle);
        if(!set_target_transform({target,carrier_angle+_carrier_angle},delta))throw std::runtime_error("Carrier target rejected");
        return;
    }
    if(_platform_joint.is_valid()){
        _platform_settings.motor_speed=static_cast<float>(_motion.speed_ratio(_time))*_platform_settings.upper_translation;
        if(!physics_world()->update_prismatic_joint(_platform_joint,_platform_settings))throw std::runtime_error("Platform motor update rejected");
    }
}
void Entity::attach_platform_joint(PhysicsObjectHandle anchor) {
    auto definition=platform_joint(kind,info,angle);
    definition.first=anchor;definition.second=physics_handle();
    _platform_settings=definition.settings;
    _platform_joint=physics_world()->create_prismatic_joint(definition);
    if(!_platform_joint.is_valid())throw std::runtime_error("Platform joint creation failed");
}
void Entity::attach_carrier(Entity* carrier) {
    _carrier=carrier;const float rad=carrier->physics_state()->angle;
    // Original carried fixtures belong to the carrier's body and cannot collide
    // with it or their siblings. Keep that exclusion for these attached poses.
    const int group=-(carrier->id+1);
    for(auto* member:{carrier,this})for(std::size_t i=0;i<member->_colliders.size();++i){
        member->_colliders[i].filter.group=group;
        member->update_physics_collider(i,member->_colliders[i]);
    }
    _carrier_offset=rotate_vector(origin-carrier->origin,-rad);
    _carrier_angle=angle*std::numbers::pi_v<float>/180-rad;
}
bool Entity::update_laser(double delta,Vec target,const Player& player) {
    _laser_active=false;_laser_hit_surface=false;if(kind!=114)return false;
    const float rad=_laser_aim.update(info,_time,delta,physics_state()->angle,player.position()-position());
    _laser_local_angle=physics_state()->angle-rad;
    if(!LaserAim::in_range(info,(target-position()).length()))return false;
    const Vec direction{std::cos(rad),std::sin(rad)};
    const float body_angle=physics_state()->angle;
    _laser_start=position()+rotate_vector(laser_muzzle_offset(body_angle-rad,size()),body_angle);
    _laser_end=_laser_start+direction*32000;
    std::vector<CollisionQueryHit> hits;
    physics_world()->raycast_all({.origin=_laser_start,.direction=direction,.max_distance=32000,.filter={.collision={.category=1|8,.mask=1|2|8}}},hits);
    _laser_active=true;
    for(const auto& hit:hits)if(hit.target.collider!=physics_collider(0)&&hit.response==CollisionResponse::Block){
        _laser_hit_surface=true;_laser_end=hit.point;return player.owns_collider(hit.target.collider);
    }
    return false;
}
int Entity::shots_due(double delta,Vec target) {
    if(!_shots)return 0;const int count=_shots->advance(delta);
    if(kind==112&&!info.contains("FixedAngle")) {
        // Thrower.updateOrientByVec:dt: aims within its local forward semicircle.
        constexpr float pi=std::numbers::pi_v<float>;
        const float body_angle=physics_state()?physics_state()->angle:angle*pi/180;
        const Vec to_role=target-position();
        float local=std::atan2(-to_role.y,to_role.x)+body_angle+pi/2;
        local-=std::floor(local/(2*pi))*(2*pi);
        local=std::clamp(local-pi/2,0.f,pi);
        _thrower_orient+=(local-_thrower_orient)*static_cast<float>(std::clamp(delta*5,0.,1.));
    }
    const float body_angle=physics_state()?physics_state()->angle:angle*std::numbers::pi_v<float>/180;
    const Vec source=position()+(kind==113?rotate_vector(homing_launcher_anchor(_visual_size),body_angle):Vec{});
    const int fired=(target-source).length()<=(kind==113?homing_active_range(info):parameter(info,"ActiveRange",800))?count:0;
    if(fired>0)_shot_visual_time=0;
    return fired;
}
Vec Entity::fire_direction(Vec target)const {
    if(info.contains("FixedAngle")){
        const float rad=physics_state()->angle-static_cast<float>(parameter(info,"FixedAngle"))*std::numbers::pi_v<float>/180;
        return {std::cos(rad),std::sin(rad)};
    }
    const Vec dir=target-position();return dir.length()>.01f?dir/dir.length():Vec{0,-1};
}
Vec Entity::fire_position(Vec direction)const {
    if(kind!=113)return center()+direction*30;
    const float radians=physics_state()?physics_state()->angle:angle*std::numbers::pi_v<float>/180;
    return center()+rotate_vector(homing_muzzle_offset(_visual_size),radians);
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
std::uint8_t Entity::moon_highlight_alpha()const {return kind==101?play::moon_highlight_alpha(_moon_highlight_time):0;}
void Entity::update(double delta) {
    if(_laser_visual)_laser_visual->update(delta,_laser_active,_laser_hit_surface,_laser_end);
    if(kind==101)_moon_highlight_time=std::min(220./60.,_moon_highlight_time+std::max(0.,delta));
    _shot_visual_time=std::min(1.,_shot_visual_time+delta);
    if(_wind_visual)_wind_visual->update(delta);
    if(kind==115)_portal_visual_time=std::fmod(_portal_visual_time+delta,40./60.);
    if(kind==116&&_bomb_state==0)_bomb_idle_time=std::fmod(_bomb_idle_time+delta,1.5);
    if(_animation)_animation->update(delta);
    if(_bomb_armed)_bomb_armed->update(delta);
}
void Entity::submit_render_commands(std::vector<RenderCommand>& out) const {
    if(collected)return;
    Rect r=render_rect();const auto c=r.center();r={c.x-_visual_size.x/2,c.y-_visual_size.y/2,_visual_size.x,_visual_size.y};
    const auto pose=physics_world()?physics_world()->render_pose(physics_handle()):std::optional<PhysicsPose>{};
    const float rotation=pose?pose->angle*180/std::numbers::pi_v<float>:angle;
    if(_wind_visual){_wind_visual->submit(out,c,rotation);return;}
    if(_laser_visual){_laser_visual->submit(out,c,_visual_size,rotation,_laser_local_angle,_laser_active,laser_length(),_laser_hit_surface);return;}
    if(kind==115) {
        // The root image is transparent. Two groups of twelve sprites form
        // the original portal, rather than two alternating animation frames.
        const auto& parts=portal_pose(_portal_visual_time,!info.contains("TargetPortalID"));
        const float radians=rotation*std::numbers::pi_v<float>/180;
        for(std::size_t i=0;i<parts.size();++i) {
            const auto& part=parts[i];const auto& frame=*_portal_frames[i/12];
            const Vec center=c+rotate_vector(part.offset,radians);
            const Vec extent{frame._width*.5f*part.scale.x,frame._height*.5f*part.scale.y};
            RenderCommand sprite;sprite.texture=frame._texture;
            sprite.command_rect={center.x-extent.x/2,center.y-extent.y/2,extent.x,extent.y};
            sprite.rotation_degrees=rotation+part.rotation;sprite.alpha=part.alpha;
            sprite.use_src_rect=frame._source_rect.has_value();if(sprite.use_src_rect)sprite.src_rect=*frame._source_rect;
            out.push_back(sprite);
        }
        return;
    }
    if(kind==116&&_bomb_state==0) {
        // The transparent root is replaced by six independent S02 petals;
        // the S01 center sits above them, as in Bomb.initSprites.
        const float radians=rotation*std::numbers::pi_v<float>/180;
        for(const auto& leaf:bomb_sleeping_pose(_bomb_idle_time))
            append_component(out,*_bomb_leaf_frame,c+rotate_vector(leaf.offset,radians),{.5f,.5f},rotation+leaf.rotation);
        append_component(out,*_bomb_center_frame,c,{.5f,.5f},rotation);
        return;
    }
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
    else if(_texture){cmd.texture=_texture;cmd.command_rect=r;cmd.rotation_degrees=rotation;cmd.flip=_base_flip;out.push_back(cmd);}
    if(_thrower_head_frame) {
        // Original base center (21.75,37.25), in local Y-up image coordinates.
        const Vec mount=c+rotate_vector({21.75f-_visual_size.x/2,_visual_size.y/2-37.25f},rotation*std::numbers::pi_v<float>/180);
        const Vec scale=thrower_barrel_scale(_shot_visual_time);
        const Vec extent{_thrower_head_frame->_width*.5f*scale.x,_thrower_head_frame->_height*.5f*scale.y};
        const Vec origin{.5f,1-.344f};
        cmd={};cmd.texture=_thrower_head_frame->_texture;
        cmd.command_rect={mount.x-origin.x*extent.x,mount.y-origin.y*extent.y,extent.x,extent.y};
        cmd.rotation_origin=origin;cmd.rotation_degrees=rotation+90-_thrower_orient*180/std::numbers::pi_v<float>;
        cmd.use_src_rect=_thrower_head_frame->_source_rect.has_value();if(cmd.use_src_rect)cmd.src_rect=*_thrower_head_frame->_source_rect;
        out.push_back(cmd);
    }
    if(_moon_highlight_frame&&moon_highlight_alpha()>0) {
        cmd={};cmd.texture=_moon_highlight_frame->_texture;cmd.command_rect=r;cmd.rotation_degrees=rotation;cmd.flip=_base_flip;
        cmd.alpha=moon_highlight_alpha();cmd.use_src_rect=_moon_highlight_frame->_source_rect.has_value();
        if(cmd.use_src_rect)cmd.src_rect=*_moon_highlight_frame->_source_rect;
        out.push_back(cmd);
    }
    const float rad=rotation*std::numbers::pi_v<float>/180;
    if(_pivot_frame)append_component(out,*_pivot_frame,c+rotate_vector({seesaw_anchor(kind,info),0},rad),{.5f,.5f},rotation);
    if(kind==113)for(std::size_t part=0;part<_leaf_frames.size();++part) {
        // The original children use bottom-left anchors in the stem's local,
        // Y-up image coordinates. Keep their native size and original z order.
        const auto leaf=launcher_leaf_pose(_shot_visual_time,part==0);
        const Vec local{leaf.position.x-_visual_size.x/2,_visual_size.y/2-leaf.position.y};
        append_component(out,*_leaf_frames[part],c+rotate_vector(local,rad),{0,1},rotation+leaf.rotation);
    }
    for(Vec point:_spike_positions){const Vec p=c+Vec{point.x*std::cos(rad)-point.y*std::sin(rad),point.x*std::sin(rad)+point.y*std::cos(rad)};
        RenderCommand s;s.texture=_spike_texture;s.command_rect={p.x-8,p.y-8,16,16};s.rotation_degrees=rotation;if(s.texture)out.push_back(s);}
}
Pivot::Pivot(Vec p):GameObject(DepthLayer::Terrain){set_world_rect({p.x,p.y,1,1});_colliders[0].shape=CircleShape{{0,0},1};_colliders[0].filter={.category=0,.mask=0};_colliders[0].response=CollisionResponse::Ignore;}
}
