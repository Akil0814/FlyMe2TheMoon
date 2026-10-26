#include "game/content/content.h"
#include "game/content/level_music.h"
#include "game/tests/original_music_fixture.h"
#include "engine/io/path/path_manager.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition,const std::string& message){
    if(!condition)throw std::runtime_error(message);
}
}

int main(){try {
    using namespace game::content;
    require(elysia::io::PathManager::instance()->initialize(std::filesystem::current_path()).has_value(),"Path initialization failed");
    std::ifstream stream("assets/configs/manifests/audio_manifest.json");
    const auto manifest=Json::parse(stream);
    std::array<bool,60> covered{};
    for(const auto& [music,levels]:game::testing::original_level_music){
        require(manifest.at("music").contains(music),"Unregistered level music: "+music);
        require(std::filesystem::is_regular_file(std::filesystem::path("assets/audio")/manifest.at("music").at(music).at("path").get<std::string>()),"Missing level music file: "+music);
        for(int id:levels){
            require(!covered.at(id-1),"Duplicate level in music snapshot");covered[id-1]=true;
            const auto data=level(id);
            require(level_music(id,data.at("suite").get<int>())==music,"Wrong original music for level "+std::to_string(id));
        }
    }
    for(bool present:covered)require(present,"Missing level in music snapshot");
    for(int id=1;id<=3;++id)for(int suite=1;suite<=4;++suite)
        require(level_music(id,suite)=="fly.SymphonyofRush","Opening levels must override every suite");
    require(level_music(4,0)=="fly.SymphonyofRush"&&level_music(60,99)=="fly.SymphonyofRush","Original unknown-suite fallback changed");
    std::cout<<"PASS: all 60 original level music assignments, 13 Hectic levels, opening overrides and registered audio files\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
