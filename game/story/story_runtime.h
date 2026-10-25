#pragma once
#include "game/content/content.h"
#include <functional>
#include <memory>
#include <optional>
struct lua_State;
namespace game::story {
struct Dialogue {std::string key,face;double remaining=0;bool fixed=false;};
class Runtime {
public:
    // Camera goto operations return their travel seconds; Lua waits before continuing.
    using Host=std::function<content::Json(const std::string&,const content::Json&)>;
    Runtime();~Runtime();
    Runtime(const Runtime&)=delete;Runtime& operator=(const Runtime&)=delete;
    void start(const std::string& source,Host host);
    void update(double delta);
    void advance(bool skip=false);
    bool event(const char* name);
    bool busy()const{return _thread!=nullptr;}
    const std::optional<Dialogue>& dialogue()const{return _dialogue;}
    bool skip_enabled()const{return _skip_enabled;}
private:
    static int dispatch(lua_State*);
    static int continued(lua_State*,int,std::intptr_t);
    void resume();
    void new_thread();
    lua_State* _state=nullptr;lua_State* _thread=nullptr;int _reference=-2;
    Host _host;
    std::optional<Dialogue> _dialogue;
    double _delay=0;bool _skip_enabled=false,_skipped=false;
    std::string _pending;
};
}
