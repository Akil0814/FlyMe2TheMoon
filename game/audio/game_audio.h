#pragma once
#include "engine/audio/audio_service.h"
#include "engine/core/geometry/vector2.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace game::audio {
using Vec=elysia::core::Vector2;
using Handle=elysia::audio::SoundHandle;
enum class DistanceModel { Normal, Close, Far };
// Original SfxDistanceModel* uses Euclidean distance in unscaled game points.
inline float distance_gain(float distance,DistanceModel model=DistanceModel::Normal){
    const float cutoff=model==DistanceModel::Close?240.f:model==DistanceModel::Far?640.f:400.f;
    if(distance>cutoff)return 0;
    const float reference=model==DistanceModel::Close?32.f:model==DistanceModel::Far?120.f:50.f;
    const float ratio=distance<=reference?1.f:reference/distance;
    return model==DistanceModel::Close?ratio:model==DistanceModel::Far?ratio*ratio:std::pow(ratio,1.4f);
}
inline float sleep_distance(DistanceModel model){return model==DistanceModel::Close?280.f:model==DistanceModel::Far?800.f:480.f;}
inline std::string sound_key(std::string name){std::replace(name.begin(),name.end(),'-','_');return "fly.voice_"+name;}
inline void ui_click(bool accepted=true){
    if(ELYSIA_AUDIO->is_initialized())(void)ELYSIA_AUDIO->request_sound(sound_key(accepted?"6-3":"7-4"),{.group=elysia::audio::SoundGroup::Ui});
}
struct CrystalSequence {
    double remaining=0;int count=0;
    void advance(double delta){remaining=std::max(0.,remaining-std::max(0.,delta));}
    int pickup(){count=remaining>0?count+1:1;remaining=.8;return std::min(count,10);}
};
struct RoleSequence {
    int state=0;double prepare=2;
    // Return 1 for start flying, 2 for start falling. The original updates
    // transitions before resetting/incrementing the one-second preparation timer.
    int update(int next,double delta){
        const bool flying=next>=2&&next<=4,was_flying=state>=2&&state<=4;
        const int event=next!=state&&flying&&!was_flying&&prepare>1?1:next==5&&state!=5?2:0;
        state=next;prepare=flying?0:prepare+std::max(0.,delta);return event;
    }
};
class World {
public:
    World()=default;
    ~World(){clear();}
    World(const World&)=delete;
    World& operator=(const World&)=delete;
    void clear();
    void reset();
    void set_listener(Vec value){_listener=value;}
    Vec listener()const{return _listener;}
    void advance(double delta);
    void set_paused(bool value);
    void play(const std::string& file,float gain=1,bool ui=false);
    void at(const std::string& file,Vec position,float gain=1);
    void loop(int owner,const std::string& file,Vec position,float gain,bool enabled=true,DistanceModel model=DistanceModel::Normal);
    void role_state(int state,double delta);
    void role_enter(){_role={};play("70-1");}
    void role_die(){choose({"47-1","48-1"});}
    void role_land(){choose({"40-1","40-2"});}
    void role_look(){choose({"50-1","50-2"});}
    double next_idle_delay(){return std::uniform_real_distribution<double>(6,10)(_random);}
    int next_idle_action(){return std::uniform_int_distribution<int>(0,3)(_random);}
    void crystal(){play("90-"+std::to_string(_crystals.pickup()));}
    int requests()const{return _requests;}
    int active_loops()const;
    const std::map<std::string,int>& events()const{return _events;}
private:
    struct Shot {Handle handle;bool ui;};
    struct Loop {std::string file;Handle handle=0;};
    void choose(std::initializer_list<const char*> files);
    Vec _listener{};
    bool _paused=false;
    int _requests=0;
    std::map<std::string,int> _events;
    std::vector<Shot> _shots;
    std::map<int,Loop> _loops;
    CrystalSequence _crystals;
    RoleSequence _role;
    std::mt19937 _random{std::random_device{}()};
};
}
