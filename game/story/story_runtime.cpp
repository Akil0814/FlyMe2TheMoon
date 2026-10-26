#include "story_runtime.h"
extern "C" {
#include "game/vendor/lua/lua.h"
#include "game/vendor/lua/lauxlib.h"
#include "game/vendor/lua/lualib.h"
}
#include <algorithm>
#include <stdexcept>
namespace game::story {
Runtime::Runtime()=default;
Runtime::~Runtime(){if(_state)lua_close(_state);}
void Runtime::new_thread(){_thread=lua_newthread(_state);_reference=luaL_ref(_state,LUA_REGISTRYINDEX);lua_sethook(_thread,[](lua_State* s,lua_Debug*){luaL_error(s,"Story instruction budget exceeded");},LUA_MASKCOUNT,500000);}
void Runtime::start(const std::string& source,Host host) {
    if(_state)lua_close(_state);_state=luaL_newstate();if(!_state)throw std::runtime_error("Lua allocation failed");
    _thread=nullptr;_host=std::move(host);_dialogue.reset();_delay=0;_pending.clear();_skip_enabled=false;_skipped=false;
    luaL_requiref(_state,"_G",luaopen_base,1);lua_pop(_state,1);
    // Source data needs no file system, process, package or network libraries.
    for(const char* n:{"dofile","loadfile","load","collectgarbage"}){lua_pushnil(_state);lua_setglobal(_state,n);}
    for(const char* name:{"faceBujie","faceDeyi","faceJingya","faceManyi","facePutong","faceQidao","faceRenzhen","faceShengqi","faceShijin","faceWeixiao","faceWunai","faceZhoumei"}){lua_pushstring(_state,name);lua_setglobal(_state,name);}
    for(const char* name:{"global.getBool","global.setBool","global.isLevelPassed","game.isRestart","game.setUserInputEnabled","game.pauseLogic","game.resumeLogic","game.restartTiming","game.showTutorialDiagram","game.setReplayMode","game.setPlayMode","game.loadTutorialRep","game.success","role.setIdleActionEnabled","role.lookLeftRight","role.loadState","camera.setFollowRoleMode","camera.gotoPosition","camera.gotoRole","sound.play","replay.pause","replay.skipNonControlFrames","dialog.setSkipEnabled","dialog.show","dialog.showMinTime","dialog.showFixedTime","script.delay"}) {
        std::string op=name;auto dot=op.find('.');const auto module=op.substr(0,dot),function=op.substr(dot+1);
        lua_getglobal(_state,module.c_str());if(lua_isnil(_state,-1)){lua_pop(_state,1);lua_newtable(_state);lua_pushvalue(_state,-1);lua_setglobal(_state,module.c_str());}
        lua_pushlightuserdata(_state,this);lua_pushstring(_state,name);lua_pushcclosure(_state,&Runtime::dispatch,2);lua_setfield(_state,-2,function.c_str());lua_pop(_state,1);
    }
    new_thread();if(luaL_loadbuffer(_thread,source.data(),source.size(),"original-level")!=LUA_OK)throw std::runtime_error(lua_tostring(_thread,-1));resume();
}
int Runtime::continued(lua_State* l,int,std::intptr_t) {
    auto* self=static_cast<Runtime*>(lua_touserdata(l,lua_upvalueindex(1)));lua_pushinteger(l,self->_skipped?1:0);self->_skipped=false;return 1;
}
int Runtime::dispatch(lua_State* l) {
    auto* self=static_cast<Runtime*>(lua_touserdata(l,lua_upvalueindex(1)));
    bool yield=false;int count=0;
    // Destroy C++ objects before lua_yieldk performs its nonlocal transfer.
    {
        const std::string op=lua_tostring(l,lua_upvalueindex(2));content::Json args=content::Json::array();
        for(int i=1;i<=lua_gettop(l);++i){if(lua_isboolean(l,i))args.push_back(lua_toboolean(l,i)!=0);else if(lua_type(l,i)==LUA_TNUMBER)args.push_back(lua_tonumber(l,i));else if(lua_isstring(l,i))args.push_back(lua_tostring(l,i));else args.push_back(nullptr);}
        if(op=="script.delay"){self->_delay=std::max(0.,args.at(0).get<double>());yield=true;}
        else if(op=="dialog.setSkipEnabled")self->_skip_enabled=args.at(0).get<bool>();
        else if(op.starts_with("dialog.show")){
            self->_dialogue=Dialogue{args.at(0).get<std::string>(),args.at(1).get<std::string>(),args.size()>2?std::max(0.,args[2].get<double>()):0,op=="dialog.showFixedTime"};yield=true;
        }else{
            const auto result=self->_host(op,args);
            if((op=="camera.gotoPosition"||op=="camera.gotoRole")&&result.is_number()){
                self->_delay=std::max(0.,result.get<double>());yield=self->_delay>0;
            }
            else if(result.is_boolean()){lua_pushboolean(l,result.get<bool>());count=1;}
            else if(result.is_number()){lua_pushnumber(l,result.get<double>());count=1;}
        }
    }
    return yield?lua_yieldk(l,0,0,&Runtime::continued):count;
}
void Runtime::resume() {
    if(!_thread)return;
    int results=0;const int status=lua_resume(_thread,nullptr,0,&results);
    if(status!=LUA_OK&&status!=LUA_YIELD)throw std::runtime_error(std::string("Original story script: ")+lua_tostring(_thread,-1));
    if(status==LUA_OK){luaL_unref(_state,LUA_REGISTRYINDEX,_reference);_reference=LUA_NOREF;_thread=nullptr;
        if(!_pending.empty()){auto event_name=std::move(_pending);_pending.clear();if(!event(event_name.c_str()))_host("event.unhandled",content::Json::array({event_name}));}}
}
void Runtime::update(double delta) {
    if(_dialogue){_dialogue->remaining=std::max(0.,_dialogue->remaining-delta);if(_dialogue->fixed&&_dialogue->remaining<=0){_dialogue.reset();resume();}}
    else if(_thread&&_delay>0){_delay=std::max(0.,_delay-delta);if(_delay<=0)resume();}
    else if(_thread)resume();
}
void Runtime::advance(bool skip) {
    if(!_dialogue)return;
    if(skip){if(!_skip_enabled)return;}
    else if(_dialogue->remaining>0||_dialogue->fixed)return;
    _skipped=skip;_dialogue.reset();resume();
}
bool Runtime::event(const char* name) {
    // The original first-level script installs callbacks after its flying demo.
    // Keep events until the suspended top-level chunk has finished defining them.
    if(_thread){_pending=name;return true;}
    lua_getglobal(_state,name);const bool exists=lua_isfunction(_state,-1);lua_pop(_state,1);if(!exists)return false;
    new_thread();lua_getglobal(_thread,name);resume();return true;
}
}
