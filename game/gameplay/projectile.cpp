#include "projectile.h"
#include "engine/animation/animation_service.h"
#include "homing_parameters.h"
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
    // The first frame is the flying sprite; subsequent images fade it out.
    // Keep that effect sequence from looping while the projectile is alive.
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
bool Projectile::advance(double delta,Vec target,const Player& player) {
    if(expired)return false;
    _age+=delta;_animation->update(delta);
    if((_homing&&_age>=32)||(!_homing&&_age>12)){expired=true;set_velocity({});return false;}
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
    for(const auto& hit:hits)if(hit.response==CollisionResponse::Block){expired=true;set_velocity({});return player.owns_collider(hit.target.collider);}
    std::vector<CollisionOverlapQueryHit> overlaps;
    physics_world()->overlap_shape({.shape=_colliders[0].shape,.pose={position(),angle},.filter={.collision={.category=8,.mask=1},.ignored_object=physics_handle()}},overlaps);
    for(const auto& hit:overlaps)if(player.owns_collider(hit.target.collider)){expired=true;set_velocity({});return true;}
    return false;
}
void Projectile::submit_render_commands(std::vector<RenderCommand>& out)const {
    if(expired)return;const auto p=render_rect().position();RenderCommand cmd;
    const auto pose=_homing&&physics_world()?physics_world()->render_pose(physics_handle()):std::optional<PhysicsPose>{};
    const float rotation=(pose?pose->angle:_initial_angle)*180/std::numbers::pi_v<float>;
    if(_animation->build_render_command({p.x-_visual_size.x/2,p.y-_visual_size.y/2,_visual_size.x,_visual_size.y},_homing?rotation:0,SpriteFlip::None,cmd))out.push_back(cmd);
}
}
