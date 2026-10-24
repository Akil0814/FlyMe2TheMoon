#pragma once
#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/physics/contracts/physics_step_participant.h"
#include "engine/animation/animation.h"
#include "game/content/content.h"
#include "motion_profile.h"
#include "shot_schedule.h"
#include "engine/camera/camera.h"
#include <array>
#include <map>

namespace game::play {
using Vec = elysia::core::Vector2;
using Rect = elysia::core::Rect;
class Parallax final:public elysia::core::GameObject {
public:
    Parallax(const content::Json& data,const elysia::camera::Camera* camera);
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out)const override;
private:
    struct LayerImage {elysia::core::RenderCommand command;Vec extent;};
    std::vector<LayerImage> _images;
    Vec _world_size;
    elysia::core::Color _sky_top{},_sky_bottom{};
    const elysia::camera::Camera* _camera;
};
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
    void die();
    Vec center()const noexcept{return position()+Vec{0,-13};}
    Rect render_rect()const noexcept override;
    bool owns_collider(elysia::physics::ColliderId id)const noexcept;
    bool idle_enabled=true,dead=false;
    double look_time=0;
private:
    std::array<elysia::physics::Collider,3> _colliders;
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
    bool wind()const{return kind==8||kind==24||kind==25;}
    Vec wind_force_at(Vec point)const;
    void attach_carrier(Entity* carrier);
    bool update_laser(Vec target,const Player& player);
    bool laser_active()const{return _laser_active;}
    float laser_length()const{return (_laser_end-_laser_start).length();}
    int shots_due(double delta,Vec target);
    Vec fire_direction(Vec target)const;
    bool update_bomb(double delta,Vec target);
    int bomb_state()const{return _bomb_state;}
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
    Entity* _carrier=nullptr;
    Vec _carrier_offset{};
    float _carrier_angle=0;
    bool _laser_active=false;
    Vec _laser_start{},_laser_end{};
    std::optional<ShotSchedule> _shots;
    int _bomb_state=0;
    double _bomb_time=0;
    std::unique_ptr<elysia::animation::Animation> _bomb_armed,_bomb_blast;
};
class Pivot final:public elysia::core::GameObject,public elysia::physics::PhysicsParticipant {
public:
    explicit Pivot(Vec position);
    std::span<const elysia::physics::Collider> collider_definitions()const override{return _colliders;}
private:
    std::array<elysia::physics::Collider,1> _colliders;
};
}
