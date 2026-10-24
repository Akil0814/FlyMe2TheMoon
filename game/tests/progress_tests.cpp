#include "game/progress/progress.h"
#include "game/progress/scoring.h"
#include "engine/save/save_service.h"
#include <fstream>
#include <iostream>
#include <chrono>

void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main() {
    try {
        using game::progress::Progress;
        std::ifstream input("assets/configs/fly/catalog.json");const auto catalog=game::content::Json::parse(input);
        Progress p;const auto& deps=catalog.at("dependencies");
        const game::content::Json reference{{"1",10},{"30",20},{"60",999}};
        require(game::progress::original_score(1,0,0,20,reference)==10000,"Slow time score incorrect");
        require(game::progress::original_score(1,3,7,20,reference)==62100,"Fast score curve / crystal weights incorrect");
        require(game::progress::original_score(60,0,0,20,reference)==20000,"Original level clamp was not preserved");
        require(game::progress::original_score(1,0,0,0,reference)==40000,"Zero-time limit incorrect");
        require(p.unlocked(1,deps),"First level must be unlocked");
        for(int i=2;i<=60;++i)require(!p.unlocked(i,deps),"Fresh progress exposes locked content");
        for(int i=1;i<=60;++i){require(p.unlocked(i,deps),"Dependency graph is not traversable in numeric order");p.stars[i-1]=3;}
        p.evaluate_achievements();require(p.achievements.size()==9,"Chapter completion / stars achievements incomplete");
        p.scores.fill(75000);p.deaths[0]=1001;p.crystals=10001;p.no_pickup_clear=true;p.evaluate_achievements();
        require(p.achievements.size()==17,"All 17 original achievements must be reachable");
        require(Progress::decode(p.encode()).encode()==p.encode(),"Progress roundtrip changed fields");
        auto legacy=p.encode();legacy.erase("score_verified");legacy.erase("legacy_scores");const auto migrated=Progress::decode(legacy);
        require(migrated.legacy_scores==p.scores&&migrated.scores[0]==0&&migrated.stars==p.stars&&migrated.story_flags==p.story_flags,"Legacy approximate scores were not preserved separately");
        auto invalid=p.encode();invalid["stars"][3]=4;bool refused=false;
        try{(void)Progress::decode(invalid);}catch(...){refused=true;}require(refused,"Invalid star count accepted");
        invalid=p.encode();invalid["version"]=99;refused=false;try{(void)Progress::decode(invalid);}catch(...){refused=true;}require(refused,"Future version accepted");
        auto* saves=ELYSIA_SAVE;
        const auto dir=std::filesystem::path("out/qa")/std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(dir);
        require(saves->initialize(dir).has_value(),"Save initialize failed");
        require(saves->create("progress").has_value(),"Save create failed");
        require(saves->set("progress","game.progress",p.encode().dump()).has_value(),"Save edit failed");
        require(saves->commit("progress").has_value(),"Save commit failed");
        require(saves->close("progress").has_value(),"Save close failed");
        saves->shutdown();require(saves->initialize(dir).has_value(),"Save service restart failed");
        require(saves->open("progress").has_value(),"Save reopen failed");
        const auto read=saves->get<std::string>("progress","game.progress");require(read.has_value(),"Save read failed");
        require(Progress::decode(game::content::Json::parse(*read)).encode()==p.encode(),"Persisted progress mismatch");
        p.story_flags.insert("lvl1_b1");
        require(saves->set("progress","game.progress",p.encode().dump()).has_value(),"Second save edit failed");
        require(saves->commit("progress").has_value(),"Second save commit failed");
        require(saves->close("progress").has_value(),"Second close failed");
        {std::ofstream corrupt(dir/"progress.json");corrupt<<"{ broken";}
        const auto recovered=saves->open("progress");require(recovered.has_value()&&recovered->recovered,"Corrupt save did not recover backup");
        const auto backup=saves->get<std::string>("progress","game.progress");require(backup.has_value(),"Recovered payload missing");
        require(Progress::decode(game::content::Json::parse(*backup)).achievements.size()==17,"Recovered progress invalid");
        saves->shutdown();
        std::cout<<"PASS: 60-level dependency graph, 17 achievements, invalid/future saves rejected, disk roundtrip, service restart, corrupt-primary recovery\n";
        std::cout<<"Evidence directory: "<<dir.string()<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
