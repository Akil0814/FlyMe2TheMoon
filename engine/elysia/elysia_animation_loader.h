#pragma once
#include "../resources/texture/surface_loader.h"
#include "../resources/texture/texture_loader.h"

#include <string>

namespace elysia::realm
{
enum class ElysiaAnimationLoaderState
{
    Unloaded,
    Loading,
    Ready,
    Failed
};

class ElysiaAnimationLoader final
{
public:
    ElysiaAnimationLoader() = default;
    ~ElysiaAnimationLoader() = default;

    ElysiaAnimationLoader(const ElysiaAnimationLoader&) = delete;
    ElysiaAnimationLoader& operator=(const ElysiaAnimationLoader&) = delete;
    ElysiaAnimationLoader(ElysiaAnimationLoader&&) = delete;
    ElysiaAnimationLoader& operator=(ElysiaAnimationLoader&&) = delete;

    void start() noexcept;
    void update() noexcept;
    void unload() noexcept;

    [[nodiscard]] ElysiaAnimationLoaderState state() const noexcept;
    [[nodiscard]] bool is_loading() const noexcept;
    [[nodiscard]] bool is_ready() const noexcept;
    [[nodiscard]] bool has_failed() const noexcept;

private:
    ElysiaAnimationLoaderState _state =ElysiaAnimationLoaderState::Unloaded;
};
}
