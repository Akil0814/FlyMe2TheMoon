#pragma once
#include <compare>
#include <cstdint>
namespace elysia::gameplay
{
class GameplayScene;
class ControllerManager;
struct SceneControlToken
{
    std::uint64_t instance = 0, generation = 0;
    auto operator<=>(const SceneControlToken &) const = default;
};
class SceneControlContext
{
  public:
    ~SceneControlContext();
    SceneControlContext(const SceneControlContext &) = delete;
    SceneControlToken token() const
    {
        return {_instance, _generation};
    }
    bool active() const
    {
        return _active;
    }

  private:
    friend class GameplayScene;
    friend class ControllerManager;
    explicit SceneControlContext(GameplayScene &scene);
    void activate();
    void deactivate();
    void reset();
    void close_noexcept() noexcept;
    GameplayScene &_scene;
    std::uint64_t _instance, _generation = 1;
    bool _active = false, _retiring = false;
};
} // namespace elysia::gameplay
