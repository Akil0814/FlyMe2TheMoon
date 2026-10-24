#pragma once
#include "thirdparty/nlohmann/json.hpp"
#include <string>

namespace game::content {
using Json = nlohmann::json;
Json read(const std::string& relative);
const Json& catalog();
Json level(int id);
}
