#pragma once
#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/physics/contracts/physics_step_participant.h"
#include "engine/animation/animation.h"
#include "game/content/content.h"
#include "motion_profile.h"
#include "shot_schedule.h"
#include "laser_aim.h"
#include "wind_visual.h"
#include "role_effect.h"
#include "thrust_particles.h"
#include "laser_visual.h"
#include "engine/physics/joint_definition.h"
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
    void play_effect(RoleEffect effect);
    RoleEffectPose effect_pose()const{return role_effect_pose(_effect,_effect_time);}
    bool body_visible()const{return !_presentation_hidden&&effect_pose().body_visible;}
    bool entry_effect_finished()const{return _effect==RoleEffect::Enter&&_effect_time>=16./30.;}
    bool death_effect_finished()const{return _effect==RoleEffect::Death&&_effect_time>=37./30.;}
    double death_effect_time()const{return _effect==RoleEffect::Death?_effect_time:0;}
    void present_at(std::optional<Vec> center,float scale=1,bool hidden=false){_presentation=center;_presentation_scale=scale;_presentation_hidden=hidden;}
    Vec center()const noexcept{return position()+Vec{0,-13};}
    Rect render_rect()const noexcept override;
    bool owns_collider(elysia::physics::ColliderId id)const noexcept;
    bool idle_enabled=true,dead=false;
    double look_time=0;
private:
    std::array<elysia::physics::Collider,3> _colliders;
    bool show_thrust()const{
        const auto pose=effect_pose();
        return !dead&&!_presentation_hidden&&!_presentation&&pose.body_visible&&pose.image==BallImage::None&&thrust.y<0;
    }
    ThrustParticles _thrust_particles;
    // Follow the displayed sprite's feet rather than moving the pivot with analog power.
    Vec thrust_attachment()const{return _action=="charLeft"?Vec{.6875f,.875f}:_action=="charRight"?Vec{.3125f,.875f}:Vec{.5f,.9375f};}
    std::map<std::string,std::unique_ptr<elysia::animation::Animation>> _animations;
    std::string _action="charIdle";
    RoleEffect _effect=RoleEffect::None;
    double _effect_time=0;
    std::optional<Vec> _presentation;
    float _presentation_scale=1;
    bool _presentation_hidden=false;
    std::array<const elysia::resources::FrameInfo*,11> _effect_frames{};
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
    void trigger_start(){_time=std::max(_time,_motion.lag);}
    bool moving()const{return kind>=105&&kind<=107;}
    void attach_platform_joint(elysia::physics::PhysicsObjectHandle anchor);
    bool rotating()const{return kind==108||kind==109||kind==110||kind==117;}
    bool wind()const{return kind==8||kind==24||kind==25;}
    Vec wind_force_at(Vec point)const;
    void attach_carrier(Entity* carrier);
    bool update_laser(double delta,Vec target,const Player& player);
    bool laser_active()const{return _laser_active;}
    float laser_length()const{return (_laser_end-_laser_start).length();}
    int shots_due(double delta,Vec target);
    Vec fire_direction(Vec target)const;
    Vec fire_position(Vec direction)const;
    bool update_bomb(double delta,Vec target);
    int bomb_state()const{return _bomb_state;}
    bool lethal(elysia::physics::ColliderId hit)const;
    int kind=0,id=0;
    bool item=false,collected=false;
    void begin_pickup(){if(collected)return;collected=true;_pickup_time=0;set_visible(kind!=3);}
    void restore_pickup(){collected=false;_pickup_time=0;set_visible(true);}
    void light_moon(){if(kind==101)_moon_highlight_time=0;}
    std::uint8_t moon_highlight_alpha()const;
    content::Json info;
    Vec origin;
    float angle=0;
private:
    std::vector<elysia::physics::Collider> _colliders;
    std::unique_ptr<elysia::animation::Animation> _animation;
    SDL_Texture* _texture=nullptr;
    Vec _visual_size;
    std::optional<WindVisual> _wind_visual;
    std::optional<LaserVisual> _laser_visual;
    elysia::core::SpriteFlip _base_flip=elysia::core::SpriteFlip::None;
    const elysia::resources::FrameInfo* _pivot_frame=nullptr;
    std::array<const elysia::resources::FrameInfo*,2> _leaf_frames{};
    const elysia::resources::FrameInfo* _thrower_head_frame=nullptr;
    float _thrower_orient=1.5707963267948966f;
    std::array<const elysia::resources::FrameInfo*,2> _portal_frames{};
    double _portal_visual_time=0;
    double _pickup_time=0;
    double _moon_highlight_time=220./60.;
    const elysia::resources::FrameInfo* _moon_highlight_frame=nullptr;
    double _shot_visual_time=1;
    MotionProfile _motion;
    elysia::physics::JointHandle _platform_joint;
    elysia::physics::PrismaticJointSettings _platform_settings;
    LaserAim _laser_aim;
    double _time=0;
    std::vector<Vec> _spike_positions;
    SDL_Texture* _spike_texture=nullptr;
    Entity* _carrier=nullptr;
    Vec _carrier_offset{};
    float _carrier_angle=0;
    bool _laser_active=false;
    bool _laser_hit_surface=false;
    float _laser_local_angle=1.5707963267948966f;
    Vec _laser_start{},_laser_end{};
    std::optional<ShotSchedule> _shots;
    int _bomb_state=0;
    double _bomb_time=0;
    double _bomb_idle_time=0;
    const elysia::resources::FrameInfo* _bomb_leaf_frame=nullptr;
    const elysia::resources::FrameInfo* _bomb_center_frame=nullptr;
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
