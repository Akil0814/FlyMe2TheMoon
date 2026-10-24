# 游戏玩法碰撞

本文说明如何把物理接触关联到游戏中的角色和攻击。它重点解释绑定顺序、命中去重与监听器寿命，不替游戏决定伤害或背包规则。

物理系统负责刚体、几何碰撞和接触；玩法碰撞把实际 ColliderId 关联到角色、阵营与 Body/PushBox/HurtBox/HitBox/Sensor 语义，并发布玩法事件。它不自动扣血、播放动画或实现背包逻辑。

要把这些接口组合成完整玩法，请阅读[从攻击命中到接收伤害](damage.md)，其中包含攻击窗口、生命值修改、死亡及清理的配套示例。

## 前提与接入流程

使用标准场景接入时，在 `GameplaySceneFeatures` 中同时提供 `physics` 并将 `gameplay_collision` 设为 `true`。默认 `GameplayScene` 没有玩法碰撞运行时；只开启物理也不会自动创建它。开启玩法碰撞但没有物理配置会在构造时抛出 `std::invalid_argument`。

仅用于说明接入条件的完整场景类型如下：

```cpp
#include "engine/gameplay/scene/gameplay_scene.h"

class CollisionExampleScene final : public elysia::gameplay::GameplayScene
{
public:
    CollisionExampleScene()
        : GameplayScene(elysia::gameplay::GameplaySceneFeatures{
            .physics = elysia::physics::PhysicsWorldConfig{},
            .gameplay_collision = true})
    {
    }

    // 此场景只启用运行时；下方辅助函数演示如何注册已存在的碰撞体。
    void on_enter(const elysia::scene::ScenePayload&) override
    {
    }

    void on_exit() override
    {
    }

    void on_reset() override
    {
    }
};
```

场景管理器只在运行时已创建时激活它。未启用时 `has_collision_runtime()` 为 false，派生场景可用 `try_collision_runtime()` 查询；强制访问 `collision_runtime()` 会抛出 `std::logic_error`。游戏操作通过 `elysia::gameplay::collision::GameplayCollisionService::instance()` 访问当前活动运行时，或在派生场景内通过 `collision_runtime()` 操作自己的运行时。

先按[物理接口指南](../systems/physics.md)创建并注册物理对象，取得真实 ColliderId。随后分配非零 ActorId、TeamId，将碰撞体绑定到玩法角色。仅绑定玩法语义不会创建物理形状，也不会替代物理层与掩码过滤。

## 绑定角色

以下辅助函数放在游戏代码中。`body`、`hurt` 是活动场景物理世界中已注册的碰撞体，`actor` 是游戏分配的非零角色标识。

```cpp
#include "engine/gameplay/collision/gameplay_collision_service.h"

bool bind_player(elysia::gameplay::collision::ActorId actor,
                 elysia::physics::ColliderId body,
                 elysia::physics::ColliderId hurt)
{
    using namespace elysia::gameplay::collision;
    GameplayCollisionService* service = GameplayCollisionService::instance();
    if (!service->has_active_runtime())
        return false;
    // 传入的是已注册的物理碰撞体 ID；这里添加角色归属与玩法角色。
    ActorCollisionRig rig;
    rig.owner = actor;
    rig.team = teams::Player;
    rig.body = body;
    rig.hurt_boxes = {hurt};
    return service->bind_actor(rig);
}
```

返回 `false` 时停止后续依赖这次绑定的操作，检查标识、碰撞体所属世界及绑定条件。`body` 与 `hurt` 应是承担不同角色的独立碰撞体；同一个 ColliderId 不能同时表示 Body 与 HurtBox。单个角色也可包含 PushBox 和 Sensor；独立碰撞体使用 `bind_collider(ColliderBinding)`。

## 攻击与去重

下面函数在攻击开始时调用，hit 是已经注册的 Overlap 碰撞体，所有者和实例、定义标识均由游戏提供。物理 filter 必须允许它与目标 HurtBox 接触。

```cpp
#include "engine/gameplay/collision/gameplay_collision_service.h"

bool begin_hit(elysia::physics::ColliderId hit,
               elysia::gameplay::collision::ActorId owner,
               elysia::gameplay::collision::AttackInstanceId instance,
               elysia::gameplay::collision::AttackDefinitionId definition)
{
    using namespace elysia::gameplay::collision;
    GameplayCollisionService* service = GameplayCollisionService::instance();
    if (!service->has_active_runtime())
        return false;
    // instance 标识本次挥击，definition 标识攻击种类，不能互相替代。
    return service->bind_hit_box({
        .collider = {hit, owner, teams::Player, ColliderRole::HitBox},
        .instigator = owner,
        .attack_instance = instance,
        .attack_definition = definition
    });
}
```

失败时不把攻击标记为已绑定；成功后记录 instance，在攻击结束或取消时 end_attack_instance。HitBox 的一次命中只在 Begin 阶段对敌对 HurtBox 路由；游戏仍负责判断目标是否存活并计算伤害。

攻击碰撞体使用 `bind_hit_box(HitBoxBinding)`，其中 `collider.role` 为 `ColliderRole::HitBox`，并提供有效的所有者、team、instigator、attack_instance 和 attack_definition。攻击实例表示这一次攻击，攻击定义表示攻击种类，不应把所有攻击永久共用一个实例号。

HitBox 与 HurtBox 的命中按“攻击实例 + 受击角色”去重，同一实例不会因为多个受击碰撞体而重复发放命中事件。一次攻击结束后调用 `end_attack_instance(id)`，清除去重记录及该实例的 HitBox 玩法绑定；它不会替你销毁物理碰撞体。下一次攻击使用新的实例标识。

### 阵营关系与攻击来源

默认关系按顺序判断：同一有效阵营为 `Friendly`（包括双方都是 `teams::Neutral`）；其余情况中任一方为 Neutral 或无效阵营时为 `Neutral`；其他不同有效阵营为 `Hostile`。

命中时会查找已通过 `bind_actor` 登记的 `instigator`，使用其阵营判断是否敌对。仅有非零来源标识或 HitBox 自身的 team，不足以产生攻击命中事件。

需要自定义关系时，在派生场景中调用 `collision_runtime().set_team_relation_resolver(...)`，查询使用 `team_relation(...)`。这两个接口属于 `GameplayCollisionRuntime`，全局服务不直接提供。resolver 是借用指针，必须活过使用它的关联期；释放前先传入 `nullptr` 解除关联。

## 监听与释放

继承 `GameplayCollisionListener`，按需覆盖 `on_body_contact`、`on_push_box_overlap`、`on_hit_overlap` 或 `on_sensor_overlap`。通过 `add_listener(listener)` 注册并检查结果，回调中根据事件的值数据更新游戏规则。监听器不会因为注册而转移所有权。

退出或销毁监听器之前调用 `remove_listener`。场景退出时全局服务已可能被解绑，因此派生场景的 `on_exit()` 中应通过自身 `collision_runtime()` 解除监听和绑定。分发期间监听增删会延后处理，不要在回调中立即释放仍可能处于本轮分发快照中的监听器。

角色移除时调用 `unbind_actor`，独立碰撞体用 `unbind_collider`；仍需由物理对象所有者完成物理生命周期管理。End 事件可能在解除绑定后到达，不能通过事件中的角色标识盲目解引用已销毁对象。重置整局时按业务清理运行时绑定、监听和攻击状态，再恢复所需注册。

## 常见误用

- 没有活动运行时就调用全局服务：不会自动创建一个运行时。
- 只配置 HitBox/HurtBox 名称却未注册物理碰撞体：不会产生接触。
- 在事件中直接重复发放同一攻击效果：先使用攻击实例规则，并维护自己的业务有效性。
- 释放监听器却不注销，或认为解绑会销毁物理对象：两套生命周期需分别处理。

## 参考

- [服务与接口](../../../../engine/gameplay/collision/gameplay_collision_service.h)、[绑定类型](../../../../engine/gameplay/collision/gameplay_collision_types.h)
- [事件定义](../../../../engine/gameplay/collision/gameplay_collision_events.h)、[运行时参考](../../architecture/subsystems/physics/07-gameplay-collision-runtime.md)
- [GameplayScene](scene.md)、[返回使用指南](../README.md)
