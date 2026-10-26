#include "replay_probe.h"
#include "engine/io/path/path_manager.h"
#include "engine/tools/logger.h"
#include <fstream>
#include <cstdlib>
#include "engine/localization/localization_service.h"

namespace game::testing {
ReplayProbe::~ReplayProbe() {
    if(!_enabled||_output.empty())return;
    // Preserve observations if the application closes before the scenario ends.
    // Do not suppress quit requests or manufacture a passing result.
    try {std::ofstream out(_output.string()+".json");out<<content::Json{{"passed",false},{"interrupted",true},{"reason","Application exited before scenario completion"},{"scenario",_scenario},{"samples",_samples}}.dump(2);}catch(...){}
}
int ReplayProbe::requested_level() {
    const char* value=std::getenv("FLY_REPLAY_SCENARIO");if(!value)return 0;
    std::ifstream in(value);if(!in)throw std::runtime_error("QA scenario does not exist");
    return content::Json::parse(in).at("level").get<int>();
}
void ReplayProbe::begin() {
    if(_enabled||!_scenario.is_null())return;
    const char* value=std::getenv("FLY_REPLAY_SCENARIO");if(!value)return;
    std::ifstream in(value);_scenario=content::Json::parse(in);
    if(_scenario.contains("language"))if(!ELYSIA_LOCALIZATION->set_language(_scenario.at("language")))throw std::runtime_error("QA language selection failed");
    _output=elysia::io::PathManager::instance()->root()/"docs/development/test-evidence"/_scenario.at("name").get<std::string>();
    _start=SDL_GetTicks();_enabled=true;
    ELYSIA_LOG_INFO("fly.test","Input replay started; no simulation overrides");
}
bool ReplayProbe::update(SDL_Renderer* renderer,const content::Json& state) {
    if(!_enabled)return false;
    const double t=(SDL_GetTicks()-_start)/1000.0;
    const auto window_id=SDL_GetWindowID(SDL_GetRenderWindow(renderer));
    if(_scenario.contains("pilot"))_pilot.update(window_id,t,_scenario.at("pilot"),state);
    while(_next<_scenario.at("events").size()&&_scenario["events"][_next]["time"].get<double>()<=t) {
        const auto& pending=_scenario["events"][_next];
        if(pending.contains("when")){
            bool ready=true;for(auto it=pending["when"].begin();it!=pending["when"].end();++it)ready&=state.contains(it.key())&&state[it.key()]==it.value();
            if(!ready)break;
        }
        const auto& entry=_scenario["events"][_next++];
        if(entry.contains("mouse")){
            float x=0,y=0;
            if(!SDL_RenderCoordinatesToWindow(renderer,entry.at("mouse")[0],entry.at("mouse")[1],&x,&y))throw std::runtime_error("QA mouse coordinate conversion failed");
            SDL_Event move{};move.type=SDL_EVENT_MOUSE_MOTION;move.motion.windowID=window_id;move.motion.x=x;move.motion.y=y;
            if(!SDL_PushEvent(&move))throw std::runtime_error("Could not enqueue QA mouse motion");
            SDL_Event click{};click.type=entry.at("down").get<bool>()?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
            click.button.windowID=window_id;click.button.button=SDL_BUTTON_LEFT;click.button.down=entry.at("down");click.button.x=x;click.button.y=y;
            if(!SDL_PushEvent(&click))throw std::runtime_error("Could not enqueue QA mouse button");
            ELYSIA_LOG_INFO("fly.test.input",std::to_string(t)+" pointer "+std::to_string(x)+","+std::to_string(y));continue;
        }
        const std::string key=entry.at("key");
        const auto scancode=SDL_GetScancodeFromName(key.c_str());
        if(scancode==SDL_SCANCODE_UNKNOWN)throw std::runtime_error("Unknown QA input key "+key);
        const bool down=entry.at("down");
        SDL_Event e{};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;e.key.windowID=window_id;e.key.scancode=scancode;e.key.key=SDL_GetKeyFromScancode(scancode,SDL_KMOD_NONE,false);e.key.down=down;
        if(!SDL_PushEvent(&e))throw std::runtime_error("Could not enqueue QA input");
        ELYSIA_LOG_INFO("fly.test.input",std::to_string(t)+" "+key+(down?" down":" up"));
        if(down)_pressed.insert(scancode);else _pressed.erase(scancode);
    }
    if(t-_last_sample>=.1) {auto sample=state;sample["wall_time"]=t;if(_scenario.contains("pilot"))sample["waypoint"]=_pilot.waypoint;_samples.push_back(sample);_last_sample=t;}
    if(state.value("success",false)&&state.value("finished",false)&&_success_at<0)_success_at=t;
    if(!state.value("success",false)&&state.value("finished",false)&&_failure_at<0)_failure_at=t;
    const bool settled_success=_scenario.contains("stop_after_success")&&_success_at>=0&&t-_success_at>=_scenario.at("stop_after_success").get<double>();
    const bool settled_failure=_scenario.contains("stop_after_failure")&&_failure_at>=0&&t-_failure_at>=_scenario.at("stop_after_failure").get<double>();
    if(t<_scenario.at("duration").get<double>()&&!settled_success&&!settled_failure)return false;
    bool passed=true;content::Json checks=content::Json::object();
    for(auto it=_scenario.at("expect").begin();it!=_scenario.at("expect").end();++it) {
        const bool ok=state.contains(it.key())&&state[it.key()]==it.value();checks[it.key()]={{"expected",it.value()},{"actual",state.value(it.key(),content::Json())},{"passed",ok}};passed&=ok;
    }
    if(_scenario.contains("observe"))for(const auto& observation:_scenario.at("observe")) {
        const auto field=observation.at("field").get<std::string>();double low=INFINITY,high=-INFINITY;
        int matched_samples=0;
        for(const auto& sample:_samples)if(sample.contains(field)&&sample[field].is_number()){
            bool matches=true;if(observation.contains("when"))for(const auto& [key,value]:observation.at("when").items())matches&=sample.contains(key)&&sample.at(key)==value;
            if(!matches)continue;++matched_samples;
            const double value=sample[field];low=std::min(low,value);high=std::max(high,value);
        }
        bool ok=std::isfinite(low)&&std::isfinite(high);
        if(observation.contains("max_at_least"))ok&=high>=observation.at("max_at_least").get<double>();
        if(observation.contains("min_at_most"))ok&=low<=observation.at("min_at_most").get<double>();
        if(observation.contains("span_at_least"))ok&=high-low>=observation.at("span_at_least").get<double>();
        if(observation.contains("span_at_most"))ok&=high-low<=observation.at("span_at_most").get<double>();
        if(observation.contains("samples_at_least"))ok&=matched_samples>=observation.at("samples_at_least").get<int>();
        const auto check_key="observe."+field+(checks.contains("observe."+field)?"."+std::to_string(checks.size()):"");
        checks[check_key]={{"requested",observation},{"minimum",low},{"maximum",high},{"samples",matched_samples},{"passed",ok}};passed&=ok;
    }
    std::ofstream out(_output.string()+".json");out<<content::Json{{"passed",passed},{"checks",checks},{"scenario",_scenario},{"samples",_samples}}.dump(2);
    if(auto* surface=SDL_RenderReadPixels(renderer,nullptr)){SDL_SaveBMP(surface,(_output.string()+".bmp").c_str());SDL_DestroySurface(surface);}
    const std::string outcome=passed?"Replay assertions PASS":"Replay assertions FAIL";
    ELYSIA_LOG_INFO("fly.test",outcome);_enabled=false;return true;
}
}
