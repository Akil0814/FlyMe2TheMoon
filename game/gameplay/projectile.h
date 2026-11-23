#pragma once
#include "flight_objects.h"

namespace game::play {
class Projectile final:public elysia::core::GameObject,public elysia::physics::PhysicsParticipant {
public:
    Projectile(Vec position,Vec velocity,bool homing,float power,float maximum,float angle=0);
    elysia::physics::BodyDefinition body_definition()const override;
    std::span<const elysia::physics::Collider> collider_definitions()const override{return _colliders;}
    bool advance(double delta,Vec target,const Player& player);
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out)const override;
    bool expired=false;
    bool homing()const noexcept{return _homing;}
    std::optional<Vec> burst_position()const noexcept{return _burst_position;}
private:
    void expire(Vec center);
    Vec _initial_velocity;
    bool _homing;
    float _power,_maximum;
    float _initial_angle=0;
    Vec _visual_size{16,16};
    double _age=0;
    std::optional<Vec> _burst_position;
    std::unique_ptr<elysia::animation::Animation> _animation;
    std::array<elysia::physics::Collider,1> _colliders;
};
// Presentation only: the projectile has already reported its hit and expired.
// No physics body or damage state survives into the debris animation.
class ProjectileBurst final:public elysia::core::GameObject {
public:
    explicit ProjectileBurst(Vec center);
    void advance(double delta);
    bool finished()const noexcept;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out)const override;
private:
    const elysia::resources::Atlas* _atlas=nullptr;
    double _time=0;
};
}
