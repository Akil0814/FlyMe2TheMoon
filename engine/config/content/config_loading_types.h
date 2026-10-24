#pragma once

#include "../config_types.h"
#include "../../io/json/json_loader.h"

#include <filesystem>
#include <string>
#include <vector>

namespace elysia::config
{
struct ConfigManifestEntry
{
    std::string key_namespace;
    std::filesystem::path document_path;
    ConfigOrigin origin;
};

struct ConfigManifest
{
    std::vector<ConfigManifestEntry> entries;
};

struct ConfigDocument
{
    std::string key_namespace;
    std::filesystem::path path;
    elysia::io::json root;
    ConfigOrigin origin;
};
}
