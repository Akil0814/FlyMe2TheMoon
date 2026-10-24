# 开发面板接入

开发覆盖层用于 Inspector 等开发工具，正式菜单和 HUD 使用[UI 系统](../ui/README.md#create-window)。接入需要启用构建选项、让游戏模块创建覆盖层，再由场景注册面板。

## 启用覆盖层


当前仓库开发构建默认开启。发布构建如需完全移除开发工具，应显式关闭：

```powershell
cmake -S . -B build -DELYSIA_ENABLE_IMGUI=OFF
```

关闭时不会创建 `imgui_lib`，不会编译 ImGui 适配器或游戏侧 Physics Inspector，也不会链接 ImGui 符号。`Application` 中事件、逐帧和渲染接入点同样由编译条件移除；`SceneRuntimeContext::development_panels()` 始终返回空。因此显式关闭的发布构建没有 ImGui 上下文、字体图集、事件处理、统计采集或绘制成本。

也可以显式开启或修正已有 CMake 缓存：

```powershell
cmake -S . -B build-imgui -DELYSIA_ENABLE_IMGUI=ON
cmake --build build-imgui --config Debug
```

开启后使用仓库固定的 Dear ImGui 1.92.9、`imgui_impl_sdl3` 与 `imgui_impl_sdlrenderer3`。不编译 `imgui_demo.cpp`，不启用 停靠、多视口或手柄导航。覆盖层隐藏时仍保留已初始化的 上下文和默认字体内存，但不向 后端发送事件，不调用 `NewFrame`、面板回调或渲染。

修改 `option()` 的默认值不会覆盖已经存在的 `CMakeCache.txt`；旧构建目录需要重新配置并显式传入 `-DELYSIA_ENABLE_IMGUI=ON`。


还需在游戏模块的 `create_development_overlay() const` 返回适配器；启用编译选项本身不会替项目创建它。工厂实现见[游戏初始化](../game-initialization.md#6-自定义开发面板可选)。覆盖层由应用初始化和关闭，游戏不自行调用其帧循环。

## 注册与注销

以下完整场景类型只演示面板接线。回调中的绘制部分仅在启用 ImGui 的构建中编译；关闭覆盖层时，场景仍可正常进入。`imgui.h` 依赖限制在这个开发场景中。

```cpp
#include "engine/scene/scene.h"
#include "engine/tools/development_overlay.h"
#include "engine/tools/logger.h"
#if ELYSIA_ENABLE_IMGUI
#include <imgui.h>
#endif

class DevelopmentPanelScene final : public elysia::scene::Scene
{
public:
    ~DevelopmentPanelScene() override
    {
        // 应用先销毁场景，再关闭覆盖层；这是正常退出外的兜底路径。
        detach_panel();
    }

protected:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        detach_panel(); // 重入不重复注册同一 stable ID。
        _panels = runtime_context().development_panels();
        if (!_panels)
        {
            return; // 未创建覆盖层或编译时禁用，属于正常情况。
        }
        _panel = _panels->register_panel("project.example.inspector", [this]
        {
            draw_inspector();
        });
        if (!_panel.is_valid())
        {
            ELYSIA_LOG_ERROR("development", "Failed to register inspector panel.");
        }
    }

    void on_exit() override
    {
        detach_panel();
    }

    void on_reset() override
    {
        detach_panel();
    }

private:
    void detach_panel()
    {
        if (_panels && _panel.is_valid())
        {
            // 本例不在面板绘制回调内注销或销毁场景。
            (void)_panels->unregister_panel(_panel);
        }
        _panel = {};
        _panels = nullptr;
    }

    void draw_inspector()
    {
#if ELYSIA_ENABLE_IMGUI
        if (ImGui::Begin("Example inspector"))
        {
            ImGui::Text("Scene paused: %s", is_paused() ? "yes" : "no");
        }
        ImGui::End(); // 即使 Begin 返回 false，也必须配对 End。
#endif
    }

    elysia::tools::IDevelopmentPanelRegistry* _panels = nullptr;
    elysia::tools::DevelopmentPanelHandle _panel{};
};
```

面板 stable ID 在一个覆盖层生命周期内的活动集合中唯一；空 ID、空回调和重复 ID 会返回无效句柄并记录诊断，不抛异常。句柄单调分配且不复用。绘制期间注册或注销会排队，当前批次使用进入绘制时的快照，修改从下一帧生效。

面板回调属于调用方代码，契约要求不得抛异常。若回调抛出，异常会传播到 `Application` render boundary，记录 `UnhandledException` 并进入 FaultExit；引擎不尝试回滚任意 ImGui 或场景状态。


回调借用场景，因此必须先注销，再销毁捕获对象。绘制期间注销是延迟操作，当前批次快照中仍可能有回调；不要在面板绘制中立即释放被捕获的对象。示例的注销都发生在场景生命周期边界。

## 显示与调试

F2 显示或隐藏开发覆盖层。隐藏时保留已初始化上下文，但不执行面板回调。可见时鼠标和键盘可能被工具界面捕获，手柄仍交给游戏；这不等于暂停物理世界。



三个 Physics Combat Demo 由 `GameplayDemoSceneBase` 共同注册 `physics_demo.inspector`。F2 显示或隐藏覆盖层，F1 原有 `DebugDraw` 开关保持不变。Inspector 展示：

- frame delta、ImGui FPS 和场景名；
- 世界配置：固定步长、重力、每帧最大补步数和 Box2D `sub_steps`；
- 最近固定步统计：已注册对象、碰撞体、接触、清醒刚体、关节、物理步耗时，以及固定步运行时累计丢弃的步数；
- 当前按需物理调试快照的 Shapes、Contacts 和 Velocities 数量；
- `DebugDraw` 全局开关，以及渲染姿态碰撞体、接触、接触法线、Native AABB、前一／当前物理姿态、速度、关节锚点和 Gameplay 分类。

最大补步数限制场景一帧内推进多少个固定步；`sub_steps` 是 Box2D 在单次物理模拟中的子步数，不能当作同一参数或旧求解器迭代次数。Native AABB 与接触数据来自最近物理步，碰撞体轮廓使用渲染姿态；姿态历史用于对照前后模拟状态、诊断插值。关节锚点标出约束在相连刚体上的连接位置，也使用插值姿态，便于与绘制中的刚体对齐。具体单位与调试状态见[物理参考](../../reference/physics/units-and-state.md)。

复选框直接修改现有 `DebugDraw`，场景下一次更新再映射为 `PhysicsDebugCapture`。Inspector 不建立第二套物理调试状态，也不改变模拟结果。


## 使用边界

不提供布局持久化、自定义字体、远程调试、ImPlot、多窗口或 Viewport。面板的业务数据由游戏维护，绘制回调不要承担必须每帧执行的游戏逻辑，也不要抛异常。

内部所有权、输入捕获和渲染状态恢复见[开发覆盖层架构](../../architecture/subsystems/development-overlay.md)；调试图元见[DebugDraw](debug-draw.md)。
