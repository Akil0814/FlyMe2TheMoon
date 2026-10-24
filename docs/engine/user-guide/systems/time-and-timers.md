# 时间与计时器

本文说明应用时间、场景固定步与手动计时器如何分工。先选择时间来源，再决定由哪个更新阶段推进，避免把慢动作、暂停和停止计时混为一谈。

时间接口使用秒。`ELYSIA_TIME` 提供应用时间，`Timer` 是由调用方驱动的普通工具，两者不会因为同时存在而自动连接。

## 选择时间来源

| 接口 | 含义 |
| --- | --- |
| `raw_delta()` | 未乘全局倍率的本帧时间增量 |
| `delta()` | 本帧原始增量乘以全局 `time_scale()` |
| `total_time()` | 累积原始时间 |
| `scaled_total_time()` | 累积缩放后的时间 |
| `frame_count()` | 已开始的帧数，不是物理固定步数 |

应用每帧调用 `begin_frame()`；游戏不要再次调用它或随意 `reset()` 全局时间。正常场景更新收到的是缩放后的 `delta`。需要不受慢动作影响的功能可主动选择 `raw_delta()`，但调用该功能的更新若已被暂停跳过，原始时间本身不会替它恢复调度。

```cpp
#include "engine/core/time.h"

// 由游戏流程分别在效果开始和结束时调用。
void begin_slow_motion()
{
    ELYSIA_TIME->set_time_scale(0.5);
}
void end_slow_motion()
{
    ELYSIA_TIME->set_time_scale(1.0);
}
```

本例假定正常倍率为 1，并且只有一个流程管理慢动作。多个效果可能同时修改倍率时，由游戏统一协调，不能让任意效果结束都无条件覆盖其他效果。

倍率在 `begin_frame()` 时参与计算，因此帧中修改从下一帧生效。零倍率停止缩放后的时间推进，`raw_delta()`、`total_time()` 和帧计数仍继续变化；输入、渲染和收到零 delta 的更新也可能执行。不要用零时间增量替代明确的输入禁用策略。

## 物理、暂停与对象倍率

启用固定步的场景把缩放后的 delta 交给独立的 `FixedStepRuntime` 累积；实际固定步先调用场景钩子，再推进可选的 `PhysicsWorld`。全局慢动作会减少单位真实时间内的固定步数量，固定步长本身不变；一帧可执行零次或多次。固定步回调不要再乘全局倍率。单次推进有最大步数限制，过大的积压会丢弃，不能用超大倍率承诺无限精确快进。

`Scene::pause()` 会停止该场景物理推进，并按对象的暂停策略筛选普通更新与输入；不会停止全局时钟。`GameObject::set_time_scale()` 只是对象级倍率，普通场景调度不会自动替对象应用它。需要时在对象自己的逻辑中使用 `scaled_delta(delta)`；它不会自动改变刚体模拟速度。

`on_before_update()` 和 `on_after_update()` 在场景暂停时仍会调用。把计时器放在这些钩子中时，若希望暂停计时，应先检查 `is_paused()`，或显式调用计时器的 `pause()`。对象成员中的计时器则随拥有者是否收到更新而推进。

## 手动驱动 Timer

以下类可作为游戏对象的成员，在对象自己的更新中调用 `update(delta)`。它不自动注册到场景。成员计时器随拥有者销毁，避免独立长生命周期回调继续访问失效对象。

```cpp
#include "engine/tools/timer.h"

class Cooldown
{
public:
    Cooldown()
    {
        _timer.set_wait_time(2.0);
        _timer.set_one_shot(true);
        // 回调借用 this；计时器是成员，并禁止复制含此回调的拥有者。
        _timer.set_on_timeout([this]
        {
            _ready = true;
        });
        _timer.pause(); // 由 start() 明确开始第一轮计时。
    }

    Cooldown(const Cooldown&) = delete;
    Cooldown& operator=(const Cooldown&) = delete;

    // 每次使用技能后重新开始整段冷却。
    void start()
    {
        _ready = false;
        _timer.restart();
    }

    void update(double delta)
    {
        // 调用方决定使用哪种时间；Timer 不会自动订阅全局更新。
        _timer.update(delta);
    }

    bool ready() const
    {
        return _ready;
    }

    double remaining_seconds() const
    {
        return _timer.get_remaining_time();
    }

private:
    bool _ready = false;
    elysia::tools::Timer _timer;
};
```

`set_wait_time()` 设置间隔而不自动重启；`restart()` 清空累计时间和已触发标记并解除暂停。`pause()` 保留进度，`resume()` 恢复；单次计时器触发后会暂停，再开始一轮应使用 `restart()`。

读取倒计时状态时，`get_wait_time()` 返回配置的间隔，`get_elapsed_time()` 返回当前累计时间，`get_remaining_time()` 返回剩余时间，单位均为秒。单次计时完成后，已用时间返回当前配置的完整间隔，剩余时间为 `0`；此时修改间隔，已用时间也随配置变化。间隔为零时，剩余时间为 `0`，不会因此视为已经完成。

`is_one_shot()` 查询当前模式；`is_finished()` 仅在单次模式且已经触发时为 `true`。主动暂停不会标记完成，完成后调用 `resume()` 也不会清除完成标记，`restart()` 才会清除。循环模式始终返回未完成。修改间隔或在回调中中断补触发，可能使未完成计时器的累计时间超过间隔，此时剩余时间仍为 `0`。

循环计时器 `set_one_shot(false)` 在大 delta 下可能连续触发多次以消耗累计间隔。非正等待时间或非正 delta 不推进。回调中重启、暂停或修改等待时间会中止当前这一轮继续补触发；不要在回调中直接销毁正在执行的 `Timer` 所有者。

## 参考

- [Time](../../../../engine/core/time.h)、[Timer](../../../../engine/tools/timer.h)
- [核心更新概念](../core_concepts.md)、[返回使用指南](../README.md)
