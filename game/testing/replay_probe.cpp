#include "replay_probe.h"
#include "engine/io/path/path_manager.h"
#include "engine/tools/logger.h"
#include <fstream>
#include <cstdlib>

namespace game::testing {
int ReplayProbe::requested_level() {
    const char* value=std::getenv("FLY_REPLAY_SCENARIO");if(!value)return 0;
    std::ifstream in(value);if(!in)throw std::runtime_error("QA scenario does not exist");
    return content::Json::parse(in).at("level").get<int>();
}
void ReplayProbe::begin() {
    if(_enabled||!_scenario.is_null())return;
    const char* value=std::getenv("FLY_REPLAY_SCENARIO");if(!value)return;
    std::ifstream in(value);_scenario=content::Json::parse(in);
    _output=elysia::io::PathManager::instance()->root()/"docs/development/test-evidence"/_scenario.at("name").get<std::string>();
    _start=SDL_GetTicks();_enabled=true;
    ELYSIA_LOG_INFO("fly.test","Input replay started; no simulation overrides");
}
bool ReplayProbe::update(SDL_Renderer* renderer,const content::Json& state) {
    if(!_enabled)return false;
    const double t=(SDL_GetTicks()-_start)/1000.0;
    const auto window_id=SDL_GetWindowID(SDL_GetRenderWindow(renderer));
    while(_next<_scenario.at("events").size()&&_scenario["events"][_next]["time"].get<double>()<=t) {
        const auto& pending=_scenario["events"][_next];
        if(pending.contains("when")){
            bool ready=true;for(auto it=pending["when"].begin();it!=pending["when"].end();++it)ready&=state.contains(it.key())&&state[it.key()]==it.value();
            if(!ready)break;
        }
        const auto& entry=_scenario["events"][_next++];const std::string key=entry.at("key");
        const auto scancode=SDL_GetScancodeFromName(key.c_str());
        if(scancode==SDL_SCANCODE_UNKNOWN)throw std::runtime_error("Unknown QA input key "+key);
        const bool down=entry.at("down");
        SDL_Event e{};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;e.key.windowID=window_id;e.key.scancode=scancode;e.key.key=SDL_GetKeyFromScancode(scancode,SDL_KMOD_NONE,false);e.key.down=down;
        if(!SDL_PushEvent(&e))throw std::runtime_error("Could not enqueue QA input");
        ELYSIA_LOG_INFO("fly.test.input",std::to_string(t)+" "+key+(down?" down":" up"));
        if(down)_pressed.insert(scancode);else _pressed.erase(scancode);
    }
    if(t-_last_sample>=.1) {auto sample=state;sample["wall_time"]=t;_samples.push_back(sample);_last_sample=t;}
    if(t<_scenario.at("duration").get<double>())return false;
    bool passed=true;content::Json checks=content::Json::object();
    for(auto it=_scenario.at("expect").begin();it!=_scenario.at("expect").end();++it) {
        const bool ok=state.contains(it.key())&&state[it.key()]==it.value();checks[it.key()]={{"expected",it.value()},{"actual",state.value(it.key(),content::Json())},{"passed",ok}};passed&=ok;
    }
    if(_scenario.contains("observe"))for(const auto& observation:_scenario.at("observe")) {
        const auto field=observation.at("field").get<std::string>();double low=INFINITY,high=-INFINITY;
        for(const auto& sample:_samples)if(sample.contains(field)&&sample[field].is_number()){
            const double value=sample[field];low=std::min(low,value);high=std::max(high,value);
        }
        bool ok=std::isfinite(low)&&std::isfinite(high);
        if(observation.contains("max_at_least"))ok&=high>=observation.at("max_at_least").get<double>();
        if(observation.contains("min_at_most"))ok&=low<=observation.at("min_at_most").get<double>();
        if(observation.contains("span_at_least"))ok&=high-low>=observation.at("span_at_least").get<double>();
        checks["observe."+field]={{"requested",observation},{"minimum",low},{"maximum",high},{"passed",ok}};passed&=ok;
    }
    std::ofstream out(_output.string()+".json");out<<content::Json{{"passed",passed},{"checks",checks},{"scenario",_scenario},{"samples",_samples}}.dump(2);
    if(auto* surface=SDL_RenderReadPixels(renderer,nullptr)){SDL_SaveBMP(surface,(_output.string()+".bmp").c_str());SDL_DestroySurface(surface);}
    const std::string outcome=passed?"Replay assertions PASS":"Replay assertions FAIL";
    ELYSIA_LOG_INFO("fly.test",outcome);_enabled=false;return true;
}
}
