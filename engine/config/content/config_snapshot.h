#pragma once

#include <memory>
#include <utility>

namespace elysia::config
{
class ConfigService;
class ConfigSnapshotBuilder;

class ConfigSnapshot final
{
public:
    struct Impl;

private:
    friend class ConfigService;
    friend class ConfigSnapshotBuilder;
    explicit ConfigSnapshot(std::shared_ptr<const Impl> impl) : _impl(std::move(impl)) {}
    std::shared_ptr<const Impl> _impl;
};
}
