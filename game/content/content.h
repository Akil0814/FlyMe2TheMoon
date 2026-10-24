#pragma once
#include "thirdparty/nlohmann/json.hpp"
#include <string>
#include <format>

namespace game::content {
using Json = nlohmann::json;
Json read(const std::string& relative);
const Json& catalog();
Json level(int id);
std::string language();
std::string ui(const std::string& key);
template<class... Args> std::string format(const std::string& key,Args&&... args){return std::vformat(ui(key),std::make_format_args(args...));}
}
