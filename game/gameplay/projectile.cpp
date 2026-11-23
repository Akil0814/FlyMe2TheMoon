#include "projectile.h"
#include "engine/animation/animation_service.h"
#include "engine/resources/resource_service.h"
#include "homing_parameters.h"
#include <algorithm>
#include <numbers>
#include <stdexcept>
namespace game::play {
using namespace elysia::physics;
using namespace elysia::core;
Projectile::Projectile(Vec p,Vec velocity,bool homing,float power,float maximum,float angle):GameObject(DepthLayer::EffectFront),_initial_velocity(velocity),_homing(homing),_power(power),_maximum(maximum),_initial_angle(angle) {
    set_world_rect({p.x,p.y,16,16});
    _colliders[0].shape=CircleShape{{0,0},7};_colliders[0].filter={.category=8,.mask=1|2};_colliders[0].response=CollisionResponse::Overlap;
    _animation=ELYSIA_ANIMATIONS->create_animation(homing?"fly.DynamicObjectsLite.DynamicObject_113.S01":"fly.DynamicObjectsLite.DynamicObject_112.S02");
    if(!_animation)throw std::runtime_error("Projectile animation unavailable");
    if(_homing){_colliders[0].shape=homing_shape();_colliders[0].density={1};const auto* frame=_animation->current_frame();_visual_size={frame->_width*.5f,frame->_height*.5f};}
    // Keep the flight frame fixed. Ordinary impact frames are drawn separately
    // by ProjectileBurst, after this object's collision lifetime has ended.
    _animation->pause();
}
BodyDefinition Projectile::body_definition()const {
    BodyDefinition b;b.type=BodyType::Dynamic;
    b.velocity=_initial_velocity;b.bullet=!_homing;
    // The sensor contributes no density-derived mass. Box2D skips gravity on
    // zero-mass bodies; the original projectile explicitly sets 0.1875 kg.
    if(!_homing)b.mass_properties=ExplicitMass{.1875f};
    else {b.angle=_initial_angle;b.fixed_rotation=false;b.linear_damping=.1f;b.angular_damping=4;b.mass_properties=homing_mass_data();}
    return b;
}
void Projectile::expire(Vec center) {
    expired=true;set_velocity({});
    if(!_homing)_burst_position=center;
}
bool Projectile::advance(double delta,Vec target,const Player& player) {
    if(expired)return false;
    _age+=delta;_animation->update(delta);
    if((_homing&&_age>=32)||(!_homing&&_age>12)){expire(position());return false;}
    Vec speed=velocity();
    if(_homing){
        const auto state=*physics_state();
        // Native cancels g=10 m/s^2 at COM, then steers by applying m*Power
        // toward the role at the local nose (0,height/2), producing real torque.
        const Vec toward=target-state.world_center;const float distance=toward.length();
        const Vec force=distance>1e-5f?toward/distance*(state.mass*_power*32):Vec{};
        const Vec nose=position()+rotate_vector({0,-12},state.angle);
        if(!physics_world()->apply_force(physics_handle(),{0,-state.mass*320})||
           !physics_world()->apply_force(physics_handle(),force,nose))throw std::runtime_error("Homing force rejected");
        if(_maximum>0&&speed.length()>_maximum*32){speed=speed/speed.length()*(_maximum*32);set_velocity(speed);}
    }
    // Include the ordinary projectile's gravitational drop in its swept path.
    // Box2D remains responsible for integrating its actual position/velocity.
    Vec displacement=speed*static_cast<float>(delta);
    if(!_homing)displacement+=physics_world()->config().gravity*static_cast<float>(.5*delta*delta);
    std::vector<CollisionQueryHit> hits;
    const float angle=_homing?physics_state()->angle:0;
    physics_world()->shape_cast_all({.shape=_colliders[0].shape,.pose={position(),angle},.displacement=displacement,.filter={.collision={.category=1|8,.mask=1|2|8},.ignored_object=physics_handle(),.include_sensors=false}},hits);
    for(const auto& hit:hits)if(hit.response==CollisionResponse::Block){expire(position()+displacement*hit.fraction);return player.owns_collider(hit.target.collider);}
    std::vector<CollisionOverlapQueryHit> overlaps;
    physics_world()->overlap_shape({.shape=_colliders[0].shape,.pose={position(),angle},.filter={.collision={.category=8,.mask=1},.ignored_object=physics_handle()}},overlaps);
    for(const auto& hit:overlaps)if(player.owns_collider(hit.target.collider)){expire(position());return true;}
    return false;
}
void Projectile::submit_render_commands(std::vector<RenderCommand>& out)const {
    if(expired)return;const auto p=render_rect().position();RenderCommand cmd;
    const auto pose=_homing&&physics_world()?physics_world()->render_pose(physics_handle()):std::optional<PhysicsPose>{};
    const float rotation=(pose?pose->angle:_initial_angle)*180/std::numbers::pi_v<float>;
    if(_animation->build_render_command({p.x-_visual_size.x/2,p.y-_visual_size.y/2,_visual_size.x,_visual_size.y},_homing?rotation:0,SpriteFlip::None,cmd))out.push_back(cmd);
}
ProjectileBurst::ProjectileBurst(Vec center):GameObject(DepthLayer::EffectFront) {
    set_world_rect({center.x,center.y,1,1});
    _atlas=ELYSIA_RESOURCES->find_atlas("fly.DynamicObjectsLite.DynamicObject_112.S02");
}
void ProjectileBurst::advance(double delta){_time=std::min(16./60.,_time+std::max(0.,delta));}
bool ProjectileBurst::finished()const noexcept{return !_atlas||_atlas->size()<16||_time+1e-9>=16./60.;}
void ProjectileBurst::submit_render_commands(std::vector<RenderCommand>& out)const {
    if(finished())return;
    // ThrowerThrown.updateExplodeAnimation: (0xc2080) uses floor(time*60),
    // frames 001..016. Cocos HD frame extents are logical pixels divided by 2.
    const auto* frame=_atlas->frame_at(static_cast<size_t>(_time*60+1e-9));
    const Vec size{frame->_width*.5f,frame->_height*.5f},center=position();
    RenderCommand command;command.texture=frame->_texture;
    command.command_rect={center.x-size.x/2,center.y-size.y/2,size.x,size.y};
    command.use_src_rect=frame->_source_rect.has_value();
    if(command.use_src_rect)command.src_rect=*frame->_source_rect;
    out.push_back(command);
}
}
