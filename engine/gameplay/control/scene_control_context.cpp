#include "scene_control_context.h"
#include "controller_manager.h"
#include <utility>

namespace elysia::gameplay
{
SceneControlContext::SceneControlContext(GameplayScene &scene) : _scene(scene)
{
    static std::uint64_t next = 1;
    _instance = next++;
    ControllerManager::instance()->_contexts.emplace(_instance, this);
}
SceneControlContext::~SceneControlContext()
{
    close_noexcept();
    ControllerManager::instance()->_contexts.erase(_instance);
}
void SceneControlContext::activate()
{
    _active = true;
}
void SceneControlContext::deactivate()
{
    _active = false;
    const bool was_retiring = std::exchange(_retiring, true);
    struct Guard
    {
        bool& flag;
        bool previous;
        ~Guard() noexcept { flag = previous; }
    } guard{_retiring, was_retiring};
    ControllerManager::instance()->leave(*this, false);
}
void SceneControlContext::reset()
{
    _active = false;
    if (_retiring)
        return;
    _retiring = true;
    struct Guard
    {
        SceneControlContext& context;
        ~Guard() noexcept
        {
            ++context._generation;
            context._retiring = false;
        }
    } guard{*this};
    try { ControllerManager::instance()->leave(*this, true); }
    catch (...)
    {
        ControllerManager::instance()->close_context_noexcept(*this);
        throw;
    }
}
void SceneControlContext::close_noexcept() noexcept
{
    _active = false;
    _retiring = true;
    ControllerManager::instance()->close_context_noexcept(*this);
    ++_generation;
    _retiring = false;
}
} // namespace elysia::gameplay
