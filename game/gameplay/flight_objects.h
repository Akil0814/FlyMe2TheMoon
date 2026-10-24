#pragma once
#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/physics/contracts/physics_step_participant.h"
#include "engine/animation/animation.h"
#include "game/content/content.h"
#include "motion_profile.h"
#include <array>
#include <map>

namespace game::play {
using Vec = elysia::core::Vector2;
using Rect = elysia::core::Rect;
class Decor final : public elysia::core::GameObject {
public:
    Decor(const content::Json& data, bool foreground);
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out) const override;
private:
    std::vector<elysia::core::RenderCommand> _commands;
};
class Edge final : public elysia::core::GameObject, public elysia::physics::PhysicsParticipant {
public:
    Edge(Vec start,Vec end);
    elysia::physics::BodyDefinition body_definition() const override;
    std::span<const elysia::physics::Collider> collider_definitions() const override {return _colliders;}
private:
    float _angle=0;
    std::array<elysia::physics::Collider,1> _colliders;
};
class Player final : public elysia::core::GameObject, public elysia::core::Updatable,
                     public elysia::physics::PhysicsParticipant, public elysia::physics::PhysicsStepParticipant {
public:
    explicit Player(Vec born);
    elysia::physics::BodyDefinition body_definition() const override;
    std::span<const elysia::physics::Collider> collider_definitions() const override {return _colliders;}
    void fixed_update(double delta) override;
    void update(double delta) override;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out) const override;
    Vec thrust{};
    bool idle_enabled=true,dead=false;
    double look_time=0;
private:
    std::array<elysia::physics::Collider,1> _colliders;
    std::map<std::string,std::unique_ptr<elysia::animation::Animation>> _animations;
    std::string _action="charIdle";
};
class Entity final : public elysia::core::GameObject, public elysia::core::Updatable,
                     public elysia::physics::PhysicsParticipant,public elysia::physics::PhysicsStepParticipant {
public:
    explicit Entity(const content::Json& data);
    elysia::physics::BodyDefinition body_definition() const override;
    std::span<const elysia::physics::Collider> collider_definitions() const override {return _colliders;}
    void update(double delta) override;
    void fixed_update(double delta) override;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out) const override;
    Rect render_rect()const noexcept override;
    Vec center()const noexcept{return position();}
    void trigger_start(){_time=_motion.lag;}
    bool rotating()const{return kind==108||kind==109||kind==110||kind==117;}
    bool lethal(elysia::physics::ColliderId hit)const;
    int kind=0,id=0;
    bool item=false,collected=false;
    content::Json info;
    Vec origin;
    float angle=0;
private:
    std::vector<elysia::physics::Collider> _colliders;
    std::unique_ptr<elysia::animation::Animation> _animation;
    SDL_Texture* _texture=nullptr;
    Vec _visual_size;
    MotionProfile _motion;
    double _time=0;
    std::vector<Vec> _spike_positions;
    SDL_Texture* _spike_texture=nullptr;
};
class Pivot final:public elysia::core::GameObject,public elysia::physics::PhysicsParticipant {
public:
    explicit Pivot(Vec position);
    std::span<const elysia::physics::Collider> collider_definitions()const override{return _colliders;}
private:
    std::array<elysia::physics::Collider,1> _colliders;
};
}
