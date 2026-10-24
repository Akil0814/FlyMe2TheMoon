#include "progress.h"
#include "scoring.h"
#include "engine/save/save_service.h"
#include "engine/tools/logger.h"
#include <algorithm>
#include <numeric>
#include <cstdlib>
#include <cmath>
#include <stdexcept>

namespace game::progress {
bool Progress::passed(int level)const {return level>=1&&level<=60&&stars[level-1]>=0;}
bool Progress::unlocked(int level,const content::Json& deps)const {
    if(level<1||level>60)return false;
    for(const auto& d:deps.at(std::to_string(level)))if(!passed(d.get<int>()))return false;
    return true;
}
void Progress::evaluate_achievements() {
    if(passed(1)&&passed(2)&&passed(3))achievements.insert(101);
    for(int stage=0;stage<4;++stage) {
        bool complete=true,all_stars=true;
        for(int i=stage*15;i<(stage+1)*15;++i){complete&=stars[i]>=0;all_stars&=stars[i]==3;}
        if(complete)achievements.insert(102+stage*2);
        if(all_stars)achievements.insert(103+stage*2);
    }
    const auto total=std::accumulate(scores.begin(),scores.end(),std::int64_t{0});
    const auto dead=std::accumulate(deaths.begin(),deaths.end(),std::int64_t{0});
    if(total>3000000)achievements.insert(121);
    if(total>4250000)achievements.insert(122);
    if(dead>=100)achievements.insert(131);
    if(dead>=1000)achievements.insert(132);
    if(*std::max_element(deaths.begin(),deaths.end())>100)achievements.insert(139);
    if(crystals>1000)achievements.insert(141);
    if(crystals>10000)achievements.insert(142);
    if(no_pickup_clear)achievements.insert(151);
}
content::Json Progress::encode()const {return {{"version",1},{"stars",stars},{"scores",scores},{"score_verified",score_verified},{"legacy_scores",legacy_scores},{"deaths",deaths},{"best_time",best_time},{"crystals",crystals},{"achievements",achievements},{"story_flags",story_flags},{"no_pickup_clear",no_pickup_clear}};}
Progress Progress::decode(const content::Json& j) {
    if(j.at("version")!=1)throw std::runtime_error("Unsupported game save version; original file retained");
    for(const std::string field:{"stars","scores","deaths","best_time"})if(!j.at(field).is_array()||j.at(field).size()!=60)throw std::runtime_error("Invalid progress array "+field);
    Progress p;p.stars=j.at("stars").get<decltype(p.stars)>();p.scores=j.at("scores").get<decltype(p.scores)>();p.deaths=j.at("deaths").get<decltype(p.deaths)>();p.best_time=j.at("best_time").get<decltype(p.best_time)>();
    if(j.contains("score_verified")){p.score_verified=j.at("score_verified").get<decltype(p.score_verified)>();p.legacy_scores=j.at("legacy_scores").get<decltype(p.legacy_scores)>();}
    else {p.legacy_scores=p.scores;p.scores.fill(0);} // Preserve approximate development records separately.
    p.crystals=j.at("crystals");p.achievements=j.at("achievements").get<std::set<int>>();p.story_flags=j.at("story_flags").get<std::set<std::string>>();p.no_pickup_clear=j.at("no_pickup_clear");
    if(p.crystals<0)throw std::runtime_error("Invalid crystal count");
    for(int i=0;i<60;++i)if(p.stars[i]<-1||p.stars[i]>3||p.scores[i]<0||p.legacy_scores[i]<0||p.deaths[i]<0||!std::isfinite(p.best_time[i])||p.best_time[i]<0)throw std::runtime_error("Invalid saved level record");
    return p;
}
Store& Store::instance(){static Store store;return store;}
std::string Store::slot()const{return std::getenv("FLY_REPLAY_SCENARIO")?"fly_test_progress":"fly_progress";}
bool Store::load() {
    if(_loaded)return _writable;_loaded=true;
    const auto fail=[&](const std::string& message){error=message;_writable=false;ELYSIA_LOG_ERROR("fly.save",message);return false;};
    auto* saves=ELYSIA_SAVE;const auto name=slot();
    const auto exists=saves->exists(name);if(!exists)return fail(exists.error().diagnostic.message);
    if(!*exists) {const auto made=saves->create(name);if(!made)return fail(made.error().diagnostic.message);return save();}
    const auto opened=saves->open(name);if(!opened)return fail(opened.error().diagnostic.message);
    if(opened->warning){recovered=true;ELYSIA_LOG_WARN("fly.save",opened->warning->diagnostic.message);}
    auto payload=saves->get<std::string>(name,"game.progress");
    if(!payload)return fail(payload.error().diagnostic.message);
    try {data=Progress::decode(content::Json::parse(*payload));}catch(const std::exception& e){return fail(e.what());}
    return true;
}
bool Store::save() {
    if(!_loaded&&!load())return false;
    if(!_writable)return false;
    auto* saves=ELYSIA_SAVE;const auto name=slot();
    const auto changed=saves->set(name,"game.progress",data.encode().dump());
    if(!changed){error=changed.error().diagnostic.message;ELYSIA_LOG_ERROR("fly.save",error);return false;}
    const auto committed=saves->commit(name);
    if(!committed){error=committed.error().diagnostic.message;ELYSIA_LOG_ERROR("fly.save",error);return false;}
    error.clear();return true;
}
bool Store::record_clear(int level,int stars_count,int collected,int crystal_count,int pickups,double elapsed) {
    if(level<1||level>60||stars_count<0||stars_count>3||elapsed<=0)throw std::invalid_argument("Invalid level clear result");
    const int i=level-1;data.stars[i]=std::max(data.stars[i],stars_count);
    if(data.best_time[i]==0||elapsed<data.best_time[i])data.best_time[i]=elapsed;
    const int score=original_score(level,stars_count,collected,elapsed,content::catalog().at("score").at("svrs"));
    data.score_verified[i]=true;
    data.scores[i]=std::max(data.scores[i],score);data.crystals+=crystal_count;
    data.no_pickup_clear|=pickups==0;data.evaluate_achievements();return save();
}
bool Store::record_death(int level,int collected) {++data.deaths.at(level-1);data.crystals+=collected;data.evaluate_achievements();return save();}
}
