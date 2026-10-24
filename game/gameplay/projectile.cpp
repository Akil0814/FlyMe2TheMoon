#include "projectile.h"
#include "engine/animation/animation_service.h"
#include <stdexcept>
namespace game::play {
using namespace elysia::physics;
using namespace elysia::core;
Projectile::Projectile(Vec p,Vec velocity,bool homing,float power,float maximum):GameObject(DepthLayer::EffectFront),_initial_velocity(velocity),_homing(homing),_power(power),_maximum(maximum) {
    set_world_rect({p.x,p.y,16,16});
    _colliders[0].shape=CircleShape{{0,0},7};_colliders[0].filter={.category=8,.mask=1|2};_colliders[0].response=CollisionResponse::Overlap;
    _animation=ELYSIA_ANIMATIONS->create_animation(homing?"fly.DynamicObjectsLite.DynamicObject_113.S01":"fly.DynamicObjectsLite.DynamicObject_112.S02");
    if(!_animation)throw std::runtime_error("Projectile animation unavailable");
}
BodyDefinition Projectile::body_definition()const {BodyDefinition b;b.type=BodyType::Kinematic;b.velocity=_initial_velocity;b.gravity_scale=0;return b;}
bool Projectile::advance(double delta,Vec target,const Player& player) {
    if(expired)return false;
    _age+=delta;_animation->update(delta);
    if(_age>12){expired=true;set_velocity({});return false;}
    Vec speed=velocity();
    if(_homing){const Vec toward=target-position();const float distance=toward.length();if(distance>.01f)speed+=toward/distance*(_power*32*static_cast<float>(delta));
        if(speed.length()>_maximum*32)speed=speed/speed.length()*(_maximum*32);set_velocity(speed);}
    std::vector<CollisionQueryHit> hits;
    physics_world()->shape_cast_all({.shape=CircleShape{{0,0},7},.pose={position(),0},.displacement=speed*static_cast<float>(delta),.filter={.collision={.category=1|8,.mask=1|2|8},.ignored_object=physics_handle(),.include_sensors=false}},hits);
    for(const auto& hit:hits)if(hit.response==CollisionResponse::Block){expired=true;set_velocity({});return player.owns_collider(hit.target.collider);}
    std::vector<CollisionOverlapQueryHit> overlaps;
    physics_world()->overlap_shape({.shape=CircleShape{{0,0},7},.pose={position(),0},.filter={.collision={.category=8,.mask=1},.ignored_object=physics_handle()}},overlaps);
    for(const auto& hit:overlaps)if(player.owns_collider(hit.target.collider)){expired=true;set_velocity({});return true;}
    return false;
}
void Projectile::submit_render_commands(std::vector<RenderCommand>& out)const {
    if(expired)return;const auto p=render_rect().position();RenderCommand cmd;
    if(_animation->build_render_command({p.x-8,p.y-8,16,16},0,SpriteFlip::None,cmd))out.push_back(cmd);
}
}
