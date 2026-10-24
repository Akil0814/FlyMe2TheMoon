#include "elysia_animation_loader.h"
#include "../tools/logger.h"

namespace
{
    const std::string animation_path = "elysia";
    static constexpr int texture_per_upload = 4;
}


namespace elysia::realm
{
void ElysiaAnimationLoader::start() noexcept
{
    _state = ElysiaAnimationLoaderState::Loading;
}

void ElysiaAnimationLoader::update() noexcept
{
}

void ElysiaAnimationLoader::unload() noexcept
{
    _state = ElysiaAnimationLoaderState::Unloaded;
}

ElysiaAnimationLoaderState ElysiaAnimationLoader::state() const noexcept
{
    return _state;
}

bool ElysiaAnimationLoader::is_loading() const noexcept
{
    return _state == ElysiaAnimationLoaderState::Loading;
}

bool ElysiaAnimationLoader::is_ready() const noexcept
{
    return _state == ElysiaAnimationLoaderState::Ready;
}

bool ElysiaAnimationLoader::has_failed() const noexcept
{
    return _state == ElysiaAnimationLoaderState::Failed;
}

}
