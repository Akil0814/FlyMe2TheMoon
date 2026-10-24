#pragma once
#include "flight_objects.h"

namespace game::play {
class Projectile final:public elysia::core::GameObject,public elysia::physics::PhysicsParticipant {
public:
    Projectile(Vec position,Vec velocity,bool homing,float power,float maximum);
    elysia::physics::BodyDefinition body_definition()const override;
    std::span<const elysia::physics::Collider> collider_definitions()const override{return _colliders;}
    bool advance(double delta,Vec target,const Player& player);
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out)const override;
    bool expired=false;
private:
    Vec _initial_velocity;
    bool _homing;
    float _power,_maximum;
    double _age=0;
    std::unique_ptr<elysia::animation::Animation> _animation;
    std::array<elysia::physics::Collider,1> _colliders;
};
}
