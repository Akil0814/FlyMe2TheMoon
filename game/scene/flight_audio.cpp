#include "flight_scene.h"
#include <numbers>

namespace game::scene {
using namespace play;
void FlightScene::update_audio(double delta){
    _audio.set_listener(camera().center());
    _audio.advance(_pause_menu_open||is_paused()?0:delta);
    if(_pause_menu_open)return;
    if(_entrance_sound_pending&&_player->effect_time()>=1./30.){
        _entrance_sound_pending=false;_audio.role_enter();
    }
    if(_finished)return;
    for(auto* entity:_entities){
        std::string file;bool enabled=true;auto model=game::audio::DistanceModel::Normal;
        if(entity->wind())file="23-2";
        else if(entity->kind==114){file="15-3";enabled=entity->laser_active();}
        else if(entity->kind==116){file="18-1";enabled=entity->bomb_state()==1;}
        else if(entity->moving()){file="13-2";model=game::audio::DistanceModel::Close;}
        else if(entity->kind==115){file="31-1";model=game::audio::DistanceModel::Close;}
        if(!file.empty())_audio.loop(entity->id,file,entity->sound_position(_audio.listener()),entity->sound_gain(),enabled,model);
    }
}
void FlightScene::update_role_audio(double delta){
    if(_finished||_death_pending||_event_pending||_transition.active())return;
    const bool grounded=physics_world().contact_state(_player->physics_handle()).grounded;
    const Vec velocity=_player->velocity();
    const bool flying=_player->thrust.y<-.1f;
    // RoleBodyComponent stores native Box2D velocity/acceleration in meters.
    // RoleView checks upward acceleration >450 m/s²,
    // rather than emitting a landing voice for every persistent floor contact.
    if(!flying&&_role_velocity_valid&&game::audio::landing_voice(_role_previous_velocity.y,velocity.y,delta))_audio.role_land();
    _role_previous_velocity=velocity;_role_velocity_valid=true;
    const int state=flying?(_player->thrust.x<-.2f?3:_player->thrust.x>.2f?4:2):!grounded&&(velocity.y>6.1f*32||_audio.role_state()==5)?5:0;
    _audio.role_state(state,delta);
    if(grounded&&_player->idle_enabled&&_player->thrust.length_squared()<.01f&&_player->look_time<=0){
        _idle_audio_time-=delta;
        if(_idle_audio_time<=0){
            _idle_audio_time=_audio.next_idle_delay();
            _player->idle_action=_audio.next_idle_action();_player->idle_action_time=_player->idle_action==3?3.84:3;
            if(_player->idle_action==2)_audio.role_look();
            if(_player->idle_action==3)_audio.play("49-1");
        }
    }else {_idle_audio_time=_audio.next_idle_delay();_player->idle_action_time=0;}
    for(auto* entity:_entities)if(entity->rotating()){
        auto& sound=_rotation_audio[entity->id];const float angle=entity->physics_state()->angle;
        if(sound.initialized){
            sound.accumulated+=std::remainder(angle-sound.angle,2*std::numbers::pi_v<float>)*180/std::numbers::pi_v<float>;
            if(std::abs(sound.accumulated)>20){_audio.at("14-3",entity->center());sound.accumulated+=sound.accumulated>0?-20:20;}
        }
        sound.initialized=true;sound.angle=angle;
    }
}
}
