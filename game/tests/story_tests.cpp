#include "game/story/story_runtime.h"
#include <fstream>
#include <format>
#include <iostream>
#include <set>
#include <regex>
using game::content::Json;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){try {
    std::ifstream texts_file("assets/configs/fly/text/zh-Hans/strings.json");const auto texts=Json::parse(texts_file);
    std::set<std::string> lines,flags,operations,expected;int success=0,restores=0;
    for(const bool passed:{false,true})for(int level=1;level<=60;++level){
        std::ifstream file(std::format("assets/configs/fly/levels/{:03}.json",level));const auto data=Json::parse(file);
        const auto source=std::regex_replace(data.at("script").get<std::string>(),std::regex("--[^\\n]*"),"");
        const std::regex pattern("dialog\\.show\\w*\\(\\s*\"([^\"]+)\"");
        for(auto i=std::sregex_iterator(source.begin(),source.end(),pattern);i!=std::sregex_iterator();++i)expected.insert((*i)[1]);
        game::story::Runtime runtime;
        runtime.start(data.at("script"),[&](const std::string& op,const Json& args)->Json {
            operations.insert(op);
            if(op=="global.getBool")return flags.contains(args[0].get<std::string>());
            if(op=="global.isLevelPassed")return passed;
            if(op=="global.setBool")flags.insert(args[0].get<std::string>());
            if(op=="game.isRestart")return false;
            if(op=="game.success")++success;
            if(op=="role.loadState")++restores;
            return nullptr;
        });
        auto observe=[&]{if(runtime.dialogue()){const auto key=runtime.dialogue()->key;check(texts.contains(key)||key=="~~~~~~","Dialogue localization missing");lines.insert(key);}};
        auto drain=[&]{int steps=0;while(runtime.busy()&&steps++<10000){observe();runtime.update(.1);observe();runtime.advance();}check(!runtime.busy(),"Story did not finish");};
        drain();if(runtime.event("onRoleReachedMoon"))drain();if(level==1){runtime.event("onRoleReachedMoon");drain();}
        if(runtime.event("onRoleDied"))drain();
        if(runtime.event("onRoleDied"))drain();
    }
    for(const auto& key:expected)if(!lines.contains(key))std::cerr<<"Missing dialogue branch: "<<key<<'\n';
    check(lines==expected,"Original dialogue branch coverage incomplete");check(flags.size()>=25,"Story progress flags missing");check(success>=7,"Chapter / final success callbacks missing");check(restores>=4,"Tutorial restore callbacks missing");
    // Minimum display time and seen-story skip return values must survive coroutine suspension.
    game::story::Runtime timing;bool finished=false;
    timing.start("dialog.setSkipEnabled(true); local r=dialog.showMinTime('test',facePutong,2); assert(r==1); game.success()",[&](const std::string&,const Json&)->Json{finished=true;return nullptr;});
    timing.advance(true);check(!finished,"Minimum reading time ignored");timing.update(2);timing.advance(true);check(finished,"Skip return value lost");
    bool queued=false;game::story::Runtime pending;
    pending.start("script.delay(2); onRoleReachedMoon=function() game.success() end",[&](const std::string&,const Json&)->Json{queued=true;return nullptr;});
    check(pending.event("onRoleReachedMoon"),"Event during initial script was lost");check(!queued,"Event ran before callback declaration");pending.update(2);check(queued,"Deferred callback did not run");
    std::cout<<"PASS: 60 original scripts, "<<lines.size()<<" dialogue keys, "<<flags.size()<<" persistent flags, "<<operations.size()<<" host operations; minimum duration and skip semantics\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
