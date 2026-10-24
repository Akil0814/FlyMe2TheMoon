#include "controller_service.h"
#include "controller_manager.h"
namespace elysia::gameplay
{
std::expected<void, ControllerError> ControllerService::begin_session()
{
    return ControllerManager::instance()->begin_session();
}
void ControllerService::end_session()
{
    ControllerManager::instance()->end_session();
}
bool ControllerService::session_active() const
{
    return ControllerManager::instance()->_session;
}
std::expected<ControllerHandle, ControllerError> ControllerService::create_impl(
    ControllerCreateInfo info, std::unique_ptr<Controller> controller)
{
    return ControllerManager::instance()->add(info, std::move(controller));
}
Controller *ControllerService::get_impl(ControllerHandle handle) const
{
    auto *entry = ControllerManager::instance()->find(handle);
    return entry ? entry->controller.get() : nullptr;
}
std::optional<ControllerDescription> ControllerService::describe(ControllerHandle handle) const
{
    auto *e = ControllerManager::instance()->find(handle);
    if (!e)
        return {};
    return ControllerDescription{
        handle, e->scope, e->owner, e->bound_scene, e->command.binding_generation, e->target != nullptr};
}
std::expected<void, ControllerError> ControllerService::remove(ControllerHandle handle)
{
    return ControllerManager::instance()->remove(handle);
}
ControllerOperation ControllerService::unbind_target(ControllerHandle handle)
{
    return ControllerManager::instance()->unbind(handle);
}
ControllerOperation ControllerService::bind_target(ControllerHandle handle, SceneControlContext &context,
                                                   elysia::core::GameObject &target)
{
    return ControllerManager::instance()->bind(handle, context.token(), target);
}
ControllerOperation ControllerService::replace_input_map(ControllerHandle handle,
                                                         elysia::input::InputActionMap map)
{
    return ControllerManager::instance()->replace_map(handle, std::move(map));
}
} // namespace elysia::gameplay
