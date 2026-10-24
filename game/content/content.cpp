#include "content.h"
#include "engine/io/path/path_manager.h"
#include <fstream>
#include <format>
#include <stdexcept>
#include "engine/localization/localization_service.h"

namespace game::content {
Json read(const std::string& relative) {
    const auto path = elysia::io::PathManager::instance()->configs() / "fly" / relative;
    std::ifstream stream(path);
    if (!stream) throw std::runtime_error("Missing game data: " + path.string());
    return Json::parse(stream);
}
const Json& catalog() { static const Json data = read("catalog.json"); return data; }
Json level(int id) {
    if(id<1 || id>60) throw std::out_of_range("Level id outside 1..60");
    return read(std::format("levels/{:03}.json",id));
}
std::string language(){const auto& language=ELYSIA_LOCALIZATION->current_language();return language=="ja"?"ja":language.starts_with("zh")?"zh-Hans":"en";}
std::string ui(const std::string& key){return std::string(ELYSIA_LOCALIZATION->tr("fly."+key));}
}
