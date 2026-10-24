#pragma once
#include "../../tools/singleton.h"
#include "controller.h"
#include "controller_types.h"
#include <expected>
#include <optional>
namespace elysia::core
{
class GameObject;
}
namespace elysia::gameplay
{
class ControllerService final : public elysia::tools::Singleton<ControllerService>
{
    friend class elysia::tools::Singleton<ControllerService>;

  public:
    std::expected<void, ControllerError> begin_session();
    void end_session();
    bool session_active() const;
    template <class T, class... Args>
    std::expected<ControllerHandle, ControllerError> create(ControllerCreateInfo info, Args &&...args)
    {
        static_assert(std::is_base_of_v<Controller, T>);
        if (!session_active())
            return std::unexpected(ControllerError::NoSession);
        return create_impl(info, std::make_unique<T>(std::forward<Args>(args)...));
    }
    template <class T = Controller> T *get(ControllerHandle handle) const
    {
        return dynamic_cast<T *>(get_impl(handle));
    }
    std::optional<ControllerDescription> describe(ControllerHandle) const;
    std::expected<void, ControllerError> remove(ControllerHandle);
    [[nodiscard]] ControllerOperation unbind_target(ControllerHandle);
    [[nodiscard]] ControllerOperation bind_target(ControllerHandle, SceneControlContext &,
                                                  elysia::core::GameObject &);
    [[nodiscard]] ControllerOperation replace_input_map(ControllerHandle, elysia::input::InputActionMap);

  private:
    ControllerService() = default;
    Controller *get_impl(ControllerHandle) const;
    std::expected<ControllerHandle, ControllerError> create_impl(ControllerCreateInfo,
                                                                 std::unique_ptr<Controller>);
};
} // namespace elysia::gameplay
