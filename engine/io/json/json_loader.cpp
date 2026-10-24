#include "json_loader.h"

namespace elysia::io
{
std::expected<void,JsonFileFailure> JsonLoader::open_file(
    const std::filesystem::path& path,std::source_location origin)
{
    reset();
    auto parsed = load_strict_json(path,origin);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    _root = std::move(*parsed);
    _loaded = true;
    return {};
}

void JsonLoader::reset()
{
    _root = json{};
    _loaded = false;
}

bool JsonLoader::is_loaded() const { return _loaded; }
const json& JsonLoader::root() const { return _root; }
}
