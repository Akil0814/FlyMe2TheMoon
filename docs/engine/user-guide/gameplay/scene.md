# GameplayScene 使用指南

本文帮助你判断何时使用 `GameplayScene`，并说明它相对普通场景增加的能力。先理解场景生命周期，再接入角色控制或碰撞，避免重复调用框架已经负责的调度流程。

`GameplayScene` 适用于需要角色控制或玩法碰撞的关卡。普通 `Scene` 提供对象管理、输入路由和 UI，并可按需启用固定步、相机和物理；`GameplayScene` 默认启用固定步与控制上下文，物理、玩法碰撞和相机仍须显式配置。菜单或纯展示场景不必为使用物理而改成 `GameplayScene`。

普通场景的生命周期、对象管理和切换规则见[场景系统](../scene/scene.md)。本页介绍 `GameplayScene` 增加的能力与扩展约束。

## 最小场景

以下类型定义放在游戏场景头文件中。它展示合法扩展点，不包含角色、资源和路由注册；注册方法见[游戏初始化](../game-initialization.md)。

```cpp
#include "engine/gameplay/scene/gameplay_scene.h"

class BattleScene final : public elysia::gameplay::GameplayScene
{
public:
    // 本例只展示固定步扩展点，暂不创建角色或绑定外部监听。
    void on_enter(const elysia::scene::ScenePayload&) override
    {
    }

    void on_exit() override
    {
    }

    void on_reset() override
    {
    }

protected:
    void on_game_fixed_update(std::uint64_t tick, double delta) override
    {
        (void)tick;
        (void)delta;
        // 放置需要在控制命令交付后执行的固定步游戏规则。
    }
};
```

空的进入、退出和重置实现只适合这个没有业务状态的示例。实际场景需要创建或恢复对象，并清理自己的监听器、回调与关联；不要把 `on_reset()` 当成引擎自动清空全部对象的操作。

## 运行时与扩展点

场景管理器在进入前激活控制上下文；只有显式启用了玩法碰撞时，才把本场景的碰撞运行时接到服务上；退出时先解除这些活动关联，再调用 `on_exit()`。所以退出回调中不要依赖全局玩法碰撞服务仍指向自己。可通过受保护的 `collision_runtime()` 清理本场景持有的玩法绑定。

`control_context()` 是本场景的控制上下文，其标记用于创建场景作用域控制器。上下文由场景拥有，不自行创建、删除或伪造标记。

| 扩展点 | 用途 |
| --- | --- |
| `on_enter`、`on_exit`、`on_reset` | 游戏自己的进入、退出与重置逻辑 |
| `on_game_fixed_update` | 控制器固定步命令交付后的游戏规则 |
| `on_control_target_removing` | 场景对象移除时处理业务引用；控制器清理已先执行 |
| `on_before_update` / `on_after_update` | 对象更新前 / 物理和相机推进后的逐帧逻辑；框架负责基础调度，末尾回收在 `on_after_update` 之后 |

不要重写 `on_routed_input`、`on_fixed_update`、`on_pause_changed`、`on_scene_object_removing` 及 `on_runtime_attach` / `on_runtime_detach` / `on_runtime_reset`，它们在 `GameplayScene` 中是 `final`。固定步先推进控制器，再调用 `on_game_fixed_update`，随后物理世界继续该步的参与者更新与模拟。

场景固定步钩子可以同步添加对象；进入物理模拟步后，参与者和碰撞回调中不能增删场景对象，应暂存请求到 `on_after_update`。

## 生命周期与暂停

- 暂停会取消控制输入并停止场景物理固定步；不是停止整个应用或所有 UI。
- 离开场景会解绑控制目标，但不等于销毁缓存场景。场景作用域控制器在上下文重置或销毁时释放；再次进入时需要按业务恢复绑定，避免无条件重复创建。
- Reset 路由会先重置控制上下文，再执行场景的 `on_reset()`；不会自动清空业务对象。Recreate 先构造候选，退出当前场景、销毁目标旧缓存后再激活候选。Session 控制器仍受游戏会话约束，不能把保留句柄等同于保留有效目标。
- 普通对象在更新末尾及退出 / 重置回调后回收。在 `on_exit()` 里调用 `destroy()` 不在调用处释放，但会在该次退出流程内回收。
- `on_enter` 失败不补调 `on_exit`；游戏外部订阅和回调应使用 RAII 或局部回滚。完整顺序和允许增删对象的阶段见[场景生命周期](../scene/scene.md)。

先建立游戏会话，再创建与绑定控制器；碰撞体实际注册后再关联玩法角色。具体操作分别见[控制器与命令](control.md)、[玩法碰撞](collision.md)。服务绑定失败、无效标记或错误路由应处理为接入错误，不能绕过状态检查继续使用。

## 菜单与玩法输入

普通菜单和加载场景使用 `Scene` 的 UI 与快捷操作，不必创建控制器。`GameplayScene` 默认使用 Pointer 模式；打开菜单时切换到 Navigation，关闭后恢复。UI 手柄通过 `set_ui_gamepad` 独立指定，不改变玩家设备归属。

`set_all_gameplay_input_blocked` 只屏蔽本地玩家玩法输入；`pause` 停止世界控制器并取消缓存。恢复后按键和摇杆需满足释放／回中条件。快捷操作和未分配设备加入处理见[输入指南](../systems/input.md)。

## 本地双玩家接入

游戏流程统一开始会话，为每名玩家配置键盘分区或手柄归属，建立各自的动作映射，再显式创建和绑定控制器。共享键盘不等于共享同一份完整键盘输入；分区权限与映射应一致。

仓库 Local multiplayer 示例保存 Session 作用域句柄。首次进入默认 WASD 控制玩家 1、方向键控制玩家 2，鼠标归玩家 1；未绑定手柄按 Start 可加入玩家 2。公共菜单支持调整键盘方案、鼠标、手柄归属和独立 UI 手柄。

进入场景时绑定蓝／红角色，离开演示后解绑但保留会话控制器，再进入时重新绑定。进入时保存外部键盘／鼠标配置，退出时恢复；手柄归属继续保留。返回主菜单结束会话，旧句柄随之失效。

示例共享相机，只展示两个角色；不提供分屏、联网或个人焦点树。底层玩家注册表与控制器管理不限制为两人。配置操作均应检查结果，不能把已受理的 Pending 当作绑定成功；完整步骤见[控制器指南](control.md)。

## 参考

- [公开定义](../../../../engine/gameplay/scene/gameplay_scene.h)与[实现](../../../../engine/gameplay/scene/gameplay_scene.cpp)
- [对象生命周期](../core_concepts.md)
- [返回使用指南](../README.md)
- [攻击命中与伤害示例](damage.md)
