#include "game_audio.h"
#include "engine/tools/logger.h"

namespace game::audio {
using elysia::audio::SoundGroup;
void World::clear(){
    if(ELYSIA_AUDIO->is_initialized()){
        for(const auto& shot:_shots)ELYSIA_AUDIO->stop_sound(shot.handle);
        for(const auto& [id,source]:_loops)if(source.handle)ELYSIA_AUDIO->stop_sound(source.handle);
    }
    _shots.clear();_loops.clear();_paused=false;
}
void World::reset(){
    clear();_crystals={};_role={};_requests=0;_events.clear();
    if(ELYSIA_AUDIO->is_initialized()){
        auto policy=ELYSIA_AUDIO->sound_group_config(SoundGroup::Gameplay);
        policy.overflow_policy=elysia::audio::SoundOverflowPolicy::ReplaceOldest;
        ELYSIA_AUDIO->set_sound_group_config(SoundGroup::Gameplay,policy);
        policy=ELYSIA_AUDIO->sound_group_config(SoundGroup::Ui);
        policy.overflow_policy=elysia::audio::SoundOverflowPolicy::ReplaceOldest;
        ELYSIA_AUDIO->set_sound_group_config(SoundGroup::Ui,policy);
    }
}
void World::advance(double delta){
    if(!_paused)_crystals.advance(delta);
    std::erase_if(_shots,[](const Shot& shot){return !ELYSIA_AUDIO->sound_active(shot.handle);});
}
void World::set_paused(bool value){
    if(value==_paused)return;_paused=value;
    for(const auto& shot:_shots)if(!shot.ui)ELYSIA_AUDIO->set_sound_paused(shot.handle,value);
    // Each loop restores its own enabled/distance state on the next refresh.
    if(value)for(const auto& [id,source]:_loops)if(source.handle)ELYSIA_AUDIO->set_sound_paused(source.handle,true);
}
void World::play(const std::string& file,float gain,bool ui){
    if((!ui&&_paused)||!ELYSIA_AUDIO->is_initialized()||gain<=0)return;
    const auto result=ELYSIA_AUDIO->request_sound(sound_key(file),{.group=ui?SoundGroup::Ui:SoundGroup::Gameplay,.gain=gain});
    if(result.handle){_shots.push_back({*result.handle,ui});++_requests;++_events[file];}
    else ELYSIA_LOG_WARN("fly.audio","Sound request rejected: "<<file);
}
void World::at(const std::string& file,Vec position,float gain){play(file,gain*distance_gain((position-_listener).length()));}
void World::loop(int owner,const std::string& file,Vec position,float gain,bool enabled,DistanceModel model){
    auto& source=_loops[owner];source.file=file;
    if(source.handle&&!ELYSIA_AUDIO->sound_active(source.handle))source.handle=0;
    const float distance=(position-_listener).length();
    if(distance>sleep_distance(model)){
        if(source.handle)ELYSIA_AUDIO->stop_sound(source.handle);source.handle=0;return;
    }
    const float effective=std::max(0.f,gain)*distance_gain(distance,model);
    if(!source.handle&&enabled&&!_paused&&effective>0&&ELYSIA_AUDIO->is_initialized()){
        const auto result=ELYSIA_AUDIO->request_sound(sound_key(file),{.loops=-1,.group=SoundGroup::Ambient,.gain=effective});
        if(result.handle){source.handle=*result.handle;++_requests;++_events[file];}
    }
    if(source.handle){
        ELYSIA_AUDIO->set_sound_gain(source.handle,effective);
        ELYSIA_AUDIO->set_sound_paused(source.handle,_paused||!enabled||effective<=0);
    }
}
void World::choose(std::initializer_list<const char*> files){
    const auto index=std::uniform_int_distribution<std::size_t>(0,files.size()-1)(_random);
    play(*(files.begin()+index));
}
void World::role_state(int state,double delta){
    const int event=_role.update(state,delta);
    if(event==1)choose({"42-1","42-2","59-1"});
    else if(event==2)choose({"45-1","45-2"});
}
int World::active_loops()const{
    return static_cast<int>(std::ranges::count_if(_loops,[](const auto& entry){return entry.second.handle&&ELYSIA_AUDIO->sound_active(entry.second.handle);}));
}
}
