#include "game/audio/game_audio.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/resources/resource_service.h"
#include "game/content/content.h"
#include <fstream>
#include <iostream>
#include <limits>

using namespace elysia;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<1e-5;}
MIX_Track* track_for(const char* file){
    auto* audio=ELYSIA_RESOURCES->find_sound(game::audio::sound_key(file));
    for(auto* track:audio::detail::mixer_backend().tracks)if(MIX_GetTrackAudio(track)==audio&&(MIX_TrackPlaying(track)||MIX_TrackPaused(track)))return track;
    return nullptr;
}
void policies(){
    using namespace game::audio;
    require(distance_gain(0)==1&&near(distance_gain(100),std::pow(.5,1.4)),"Normal attenuation differs from original");
    require(distance_gain(400)>0&&distance_gain(400.01f)==0&&sleep_distance(DistanceModel::Normal)==480,"Normal sleep band lost");
    require(near(distance_gain(64,DistanceModel::Close),.5)&&distance_gain(240,DistanceModel::Close)>0&&distance_gain(241,DistanceModel::Close)==0,"Close attenuation differs from original");
    require(near(distance_gain(240,DistanceModel::Far),.25)&&distance_gain(641,DistanceModel::Far)==0,"Far attenuation differs from original");
    CrystalSequence sequence;require(sequence.pickup()==1,"First crystal must use 90-1");
    sequence.advance(.79);require(sequence.pickup()==2,"Fast crystal pickup must advance pitch");
    for(int i=0;i<30;++i)sequence.pickup();require(sequence.pickup()==10,"Crystal sequence must saturate at 90-10");
    sequence.advance(.8);require(sequence.pickup()==1,"Crystal chain must reset after 0.8 seconds");
    RoleSequence role;require(role.update(2,.1)==1,"Initial flying sound missing");
    require(role.update(3,.1)==0&&role.update(4,.1)==0,"Direction changes must not replay flying voices");
    require(role.update(5,.1)==2&&role.update(5,.1)==0,"Falling must sound only on state entry");
    require(role.update(2,.1)==0,"Short flight interruption must not replay a flying voice");
    role.update(0,1.1);require(role.update(2,0)==1,"Flying voice must rearm after one second outside flying states");
}
void mixer(){
    using game::audio::World;using audio::SoundGroup;
    auto* service=ELYSIA_AUDIO;
    std::ifstream input("assets/configs/manifests/audio_manifest.json");auto manifest=game::content::Json::parse(input);
    for(const auto& [key,entry]:manifest.at("sounds").items()){
        require(resources::ResourceManager::instance()->load_sound({key,std::filesystem::path("assets/audio")/entry.at("path").get<std::string>()}).has_value(),"Sound could not be decoded");
    }
    require(service->initialize({}),"Audio initialization failed");
    int decoded=0;
    for(const auto& [key,entry]:manifest.at("sounds").items()){
        const auto sound=service->request_sound(key);require(sound.handle.has_value(),"Registered sound failed to start");
        require(service->sound_active(*sound.handle),"Started sound not tracked");service->stop_sound(*sound.handle);++decoded;
    }
    require(ELYSIA_RESOURCES->find_sound("fly.voice_44_1")!=nullptr,"Original 44-1 alias missing");
    World world;world.reset();world.set_listener({0,0});world.loop(1,"23-2",{0,0},.5f);
    auto* wind=track_for("23-2");require(wind&&near(MIX_GetTrackGain(wind),.5),"Loop must start with its source gain, without a full-volume first frame");
    const auto deadline=SDL_GetTicks()+1000;
    while(MIX_GetTrackPlaybackPosition(wind)==0&&SDL_GetTicks()<deadline)SDL_Delay(5);
    require(MIX_GetTrackPlaybackPosition(wind)>0,"Loop playback is not advancing");
    world.set_paused(true);require(MIX_TrackPaused(wind),"World pause did not pause the mixer track");
    const auto position=MIX_GetTrackPlaybackPosition(wind);SDL_Delay(50);service->update(.05);
    require(MIX_GetTrackPlaybackPosition(wind)==position&&world.active_loops()==1,"Paused sound was advanced or pruned");
    auto shot=service->request_sound(game::audio::sound_key("70-1"));require(shot.handle&&MIX_TrackPaused(wind),"New sound reused the paused loop's channel");
    service->stop_sound(*shot.handle);world.set_paused(false);world.loop(1,"23-2",{0,0},.5f);
    require(MIX_TrackPlaying(wind)&&MIX_GetTrackPlaybackPosition(wind)>=position,"Loop resume restarted playback");
    service->set_master_volume(50);service->set_sound_volume(40);
    require(near(MIX_GetTrackGain(wind),.1),"Per-source gain lost when settings changed");
    world.set_listener({100,0});world.loop(1,"23-2",{0,0},.5f);
    require(near(MIX_GetTrackGain(wind),.1*game::audio::distance_gain(100)),"Moving listener did not update distance gain");
    world.set_listener({450,0});world.loop(1,"23-2",{0,0},.5f);
    require(MIX_TrackPaused(wind)&&world.active_loops()==1,"Inaudible sleep band must retain a paused source");
    world.set_listener({481,0});world.loop(1,"23-2",{0,0},.5f);
    require(world.active_loops()==0,"Distant source did not release its channel");
    world.set_listener({0,0});world.loop(1,"23-2",{0,0},.5f);
    require(world.active_loops()==1&&world.events().at("23-2")==2,"Returning to range did not wake the loop");
    world.play("70-1");world.set_paused(true);world.play("106-1",1,true);
    require(track_for("70-1")&&MIX_TrackPaused(track_for("70-1"))&&track_for("106-1")&&MIX_TrackPlaying(track_for("106-1")),"UI/result sounds must continue while gameplay sounds are paused");
    world.clear();for(auto* track:audio::detail::mixer_backend().tracks)require(!MIX_TrackPlaying(track)&&!MIX_TrackPaused(track),"World cleanup leaked a playing or paused sound");
    const auto first=service->request_sound(game::audio::sound_key("23-2"),{.loops=-1});require(first.handle.has_value(),"Stale-handle fixture failed");
    service->stop_sound(*first.handle);const auto second=service->request_sound(game::audio::sound_key("23-2"),{.loops=-1});
    require(second.handle&&*first.handle!=*second.handle&&!service->set_sound_gain(*first.handle,0)&&!service->set_sound_paused(*first.handle,true)&&service->sound_active(*second.handle),"Old handles affected a reused channel");
    require(!service->set_sound_gain(*second.handle,std::numeric_limits<double>::quiet_NaN()),"Invalid gain was accepted");
    service->stop_sound(*second.handle);service->shutdown();
    std::cout<<"PASS: "<<decoded<<" decoded sound entries; original attenuation/chain/state rules; live mixer gains, pause/resume, voice separation, sleep/wake, stale handles and cleanup\n";
}
int main(){int result=1;try{
    policies();SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy");require(SDL_Init(SDL_INIT_AUDIO),SDL_GetError());require(MIX_Init(),SDL_GetError());mixer();result=0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';}
    ELYSIA_AUDIO->shutdown();resources::ResourceManager::instance()->clear();audio::detail::mixer_backend().shutdown();MIX_Quit();SDL_Quit();return result;
}
