#pragma once

#include "config_snapshot.h"
#include "../config_types.h"
#include "../../io/json/json_loader.h"

#include <map>
#include <string>

namespace elysia::config
{
struct ConfigSnapshotNode
{
    elysia::io::json value;
    ConfigOrigin origin;
};

struct ConfigSnapshot::Impl
{
    std::map<std::string,ConfigSnapshotNode> nodes;
};
}
