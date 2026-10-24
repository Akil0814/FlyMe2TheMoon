# 场景系统

本文面向编写游戏场景的开发者，说明如何创建与注册场景、选择扩展点，以及如何处理重新进入和失败清理。完整类型示例展示合法接口；局部调用片段需要放在其说明的场景阶段中。

`Scene` 负责组织游戏对象与 UI，并协调输入、逐帧更新、物理固定步、相机和绘制。菜单、展示页和简单关卡可以直接继承它；需要控制器与玩法碰撞时使用 [GameplayScene](../gameplay/scene.md)。先阅读[核心概念](../core_concepts.md)，再按本页组织具体代码。

## 创建与进入场景

以下类型可放入游戏代码的场景头文件。它只有一个不更新、不提交绘制命令的对象，用于展示复用与重置；如何让对象可见见[游戏对象与绘制](game-objects.md)。

```cpp
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/routing/scene_payload.h"

struct RoomPayload
{
    int score = 0;
};

class RoomScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload& payload) override
    {
        if (const RoomPayload* data = elysia::scene::try_scene_payload<RoomPayload>(payload))
            _score = data->score;
        // 本例允许空 Payload；需要必填参数的场景应显式拒绝类型不符。
        if (!_object)
            _object = create_and_add_object<elysia::core::GameObject>(
                elysia::core::DepthLayer::Item);
        if (!_object)
            return; // 创建未成功，不执行依赖对象的逻辑。
        _object->set_position({100.0f, 100.0f});
        resume();
    }

    void on_exit() override
    {
        // 本例保留对象供下次进入；外部订阅应在这里解除。
    }

    void on_reset() override
    {
        // 框架重置不会代替业务对象 reset；这里显式恢复本场景的数据。
        _score = 0;
        if (_object)
            _object->reset();
        resume();
    }

protected:
    void on_scene_object_removing(elysia::core::SceneObject& object) override
    {
        // 移除通知发生时对象仍有效，及时清空借用，避免留下悬空指针。
        if (&object == _object)
            _object = nullptr;
    }

private:
    elysia::core::GameObject* _object = nullptr; // 场景持有，这里只借用。
    int _score = 0;
};

// 由 IGameModule::register_scenes 调用；项目内确保标识符唯一。
inline void register_room(elysia::scene::SceneManager& scenes)
{
    scenes.register_game_scene<RoomScene>(1);
}
```

游戏 SceneKey 使用 `1..999`，不能复用引擎保留标识或重复注册。构造参数会被保存用于重建，须满足注册接口的可复制要求；借用依赖用 `std::ref`/`std::cref` 时自行保证生命期。完整应用入口见[游戏初始化](../game-initialization.md)。

## 常用操作与扩展点

| 接口 | 使用位置与规则 |
| --- | --- |
| `create_and_add_object<T>(...)` | 创建 `GameObject` 或 `UiElement` 派生对象并交给场景；返回借用指针，失败可返回空 |
| `add_object(std::unique_ptr<T>)` | 转移现有对象所有权；调用后不自行释放它 |
| `pause()` / `resume()` | 改变场景暂停状态；相机播放默认冻结，可由 `advance_when_paused` 覆盖 |
| `on_before_update(double)` / `on_after_update(double)` | 普通对象更新前 / 物理和相机推进后的业务逻辑；框架自动调度，不调用基类更新入口 |
| `on_reset()` | Reset 路由中的业务重置；引擎已重置输入、暂停状态、固定步和相机运行时 |
| `on_fixed_update(tick, delta)` | 普通场景的固定步规则；仅实际模拟步发生时执行 |
| `on_shortcuts(frame, events)` | UI 处理后的快捷操作；用 `consume_input(event)` 消费已处理事件 |
| `on_unassigned_input(event)` | 未分配设备输入，例如按键加入；返回是否处理 |
| `on_routed_input(snapshot)` | 经场景路由处理的输入；`GameplayScene` 已接管此扩展点 |
| `on_pause_changed(bool)` | 普通场景响应暂停变化；`GameplayScene` 有自己的 final 实现 |
| `on_scene_object_registered` / `on_scene_object_removing` | 同步业务关联；重写注册回调时保留基类行为；移除回调内不要销毁仍在执行的监听者 |
| `has_camera()` / `camera()` | 查询相机能力并访问最终呈现相机；未启用时强制访问会抛出契约异常 |
| `try_camera_runtime()` / `camera_runtime()` | 在派生场景中查询或访问相机运行时，规则与物理能力访问一致 |
| `resolve_camera_focus(slot)` | 为场景拥有的每个槽提供跟随目标，不保存相机内部状态 |
| `camera_runtime().cut_to()` / `blend_to()` | 立即选择或平滑切换最终呈现槽 |
| `camera_runtime().move_to()` / `play_path()` | 播放单段移动或多节点相机姿态轨迹 |

输入细节见[输入](../systems/input.md)，固定步与暂停见[时间](../systems/time-and-timers.md)，物理与坐标分别见[物理](../systems/physics.md)、[相机](../systems/camera.md)。场景的渲染入口是私有非虚函数；通过对象提交命令，不能重写 `on_render()`。

相机与物理一样是按需能力。纯 UI 场景不配置相机；需要提交世界绘制命令或世界 `DebugDraw` 的场景必须在 `SceneRuntimeFeatures` 或 `GameplaySceneFeatures` 中显式提供 `CameraSceneConfig`。默认 `GameplayScene` 只启用固定步与控制能力。

`add_object` 同步转移所有权并注册接口，不是延迟命令。禁止修改的阶段会在修改容器前抛出契约异常；把生成请求暂存到 `on_before_update` 或 `on_after_update` 再执行。`clear_scene_objects()` 也必须在安全阶段调用。

| 当前阶段 | 同步添加场景对象 | 清空场景对象 |
| --- | --- | --- |
| `on_enter`、`on_reset`、`on_exit` | 允许，但退出阶段通常只做清理 | 允许 |
| `on_before_update` / `on_after_update` | 允许 | 允许 |
| 场景 `on_fixed_update` / `on_game_fixed_update`，尚未进入物理模拟步 | 允许 | 允许 |
| 使用快照分发的输入回调 | 允许 | 允许，分发会跳过已移除对象 |
| 对象 `update()`、UI 呈现动画遍历 | 禁止 | 禁止 |
| 世界 / UI 绘制命令提交、对象查询 visitor 或谓词 | 禁止 | 禁止 |
| 物理模拟步内的参与者、碰撞及过滤回调 | 禁止 | 禁止 |
| 对象注册回调 | 允许嵌套添加 | 禁止，避免释放尚在注册的对象 |
| 对象移除回调、场景销毁准备 | 禁止 | 禁止重入清理 |

物理查询中的 `noexcept` 回调必须只读：不要依赖抛出异常来退出这种回调，更不能在回调中执行场景增删。`destroy()` 只设置标记，但查询和绘制回调仍应保持只读。输入快照只保护当前分发；回调清空对象后，自己的代码仍须停止使用失效的借用指针。

一次更新的顺序为：`on_before_update` → 普通对象更新 → UI 呈现动画 → 零次或多次固定步（场景固定步钩子 → 可选物理模拟步）→ 物理插值收尾 → 可选相机推进 → `on_after_update` → 回收已标记销毁的对象。`on_after_update` 在回收之前，不能假定已标记对象已经释放。即使业务更新抛异常，框架也会尝试末尾回收。

## 切换、返回与退出

下面是派生场景输入/更新函数中的片段，目标 2 必须已注册并接受 RoomPayload。调用后结束本次业务分支，避免同一处理周期再发请求。

```cpp
request_scene_switch(2, RoomPayload{42}, elysia::scene::SceneReloadMode::Reset);
return;
```

也可传入 `SceneRoute{.target = ..., .payload = ..., .reload_mode = ...}`，将返回目的地作为值保存。退出应用使用 `request_quit()`。这些受保护接口只在派生场景的正常输入/更新阶段调用；不要在 `on_enter`、`on_exit`、`on_reset`、绘制或析构中发请求。

请求在当前输入/更新处理结束后执行，不是函数调用时立即换场景。一个处理周期最多一个请求；重复或重入请求、无效或未注册目标属于接入错误，会抛异常，不会自动改用另一个正常游戏目标。异常可能继续进入场景失败处理流程，不能把它当作路由成功。启动失败与不可恢复运行错误见[应用错误场景](../builtin-scenes/application-failure.md)。

## 复用、重置与释放

| 路由模式 | 目标场景行为 |
| --- | --- |
| `Reuse` | 使用缓存实例，保留业务状态，再次调用 `on_enter`；即使目标是当前实例，也会先退出再进入 |
| `Reset` | 使用实例，先重置框架运行时，再调用 `on_reset()`，之后进入；业务对象和状态由游戏选择如何重置 |
| `Recreate` | 先构造新候选，再退出当前场景、销毁目标旧缓存，最后激活候选；旧对象引用全部失效 |

离开场景不等于销毁它。`on_exit()` 负责解除外部监听、停止场景专属声音、清理失效关联；可按玩法保留场景对象。不要每次进入无条件重新创建一套对象或订阅。

`destroy()` 只标记对象；场景在更新末尾、`on_exit()` 之后和 `on_reset()` 之后回收已标记对象。因此退出回调中标记的场景对象会在退出流程内释放，不需要等待再次进入。UI 子节点由所属容器清理。`on_reset()` 不会自动清空整个对象列表；需要全新对象图时可以选择 Recreate。

## 进入、退出、失败与关闭顺序

正常进入先绑定应用运行时上下文和本地玩家，再绑定活动特效、对象查询及场景运行时服务，最后执行 `on_enter(payload)`。因此 `on_enter` 可以创建对象、绑定控制器以及请求当前场景特效；构造函数不能假定这些活动服务已指向候选场景。

正常退出先移除路由观察者、重置输入交互、解除运行时服务和活动查询 / 特效绑定，再执行 `on_exit()`，回收已标记对象并取消相机轨迹与混合，最后进入 Inactive。退出时屏幕特效被清除；`on_exit` 不能依赖活动服务仍指向本场景，应使用自己持有的运行时清理关联。未销毁的对象可随缓存场景保留。

Reset 在进入前完成：重置输入、解除暂停、清空固定步累计与固定步、重置相机、执行运行时重置钩子，再执行 `on_reset()` 和标记对象回收。`GameplayScene` 的运行时重置会释放场景作用域控制器并更新上下文代次；其他业务对象不会因此自动重建。

Recreate 的候选构造发生在当前场景退出之前。构造函数应只建立自身状态；外部服务接入放在 `on_enter`。成功进入后候选才存入缓存并成为当前场景，失败的候选会解除已建立的关联并清理。

运行时异常恢复先解除并退出当前场景，再销毁故障场景及其对象；清理成功后才调用失败路由工厂、构造和进入恢复场景。任何清理步骤抛异常时，框架仍尝试后续必要清理，但最终进入 Faulted，不调用失败路由工厂。恢复场景失败也进入 Faulted，不递归创建另一个恢复场景。底层渲染失败直接交给应用故障退出。

`on_enter` 抛异常不补调 `on_exit`。引擎会撤销自己的绑定并清理候选；外部订阅、借用回调、音频等游戏侧副作用必须通过 RAII 或进入函数中的局部回滚处理，不能只依赖 `on_exit`。

应用关闭先退出当前场景，再对全部缓存场景执行销毁准备：在对象仍存活时通知移除、解绑控制与物理、清理登记并释放所有权，然后清空场景运行时上下文。全部场景销毁后，应用才释放上下文、字体、内容资源、Renderer、窗口和 SDL。析构兜底只断开引用、释放资源，不调用游戏的虚生命周期回调；正常逻辑清理应由上述受管理流程完成。

## 参考

- [Scene 接口](../../../../engine/scene/scene.h)、[场景路由](../../../../engine/scene/routing/scene_route.h)
- [GameplayScene](../gameplay/scene.md)、[返回使用指南](../README.md)
