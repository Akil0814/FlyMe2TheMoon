#pragma once

#include "strict_json.h"

namespace elysia::io
{
// Owns one strictly parsed document. Schema validation belongs to its loader.
class JsonLoader
{
public:
    [[nodiscard]] std::expected<void,JsonFileFailure> open_file(
        const std::filesystem::path& path,
        std::source_location origin = std::source_location::current());
    void reset();
    [[nodiscard]] bool is_loaded() const;
    [[nodiscard]] const json& root() const;

private:
    json _root;
    bool _loaded = false;
};
}
