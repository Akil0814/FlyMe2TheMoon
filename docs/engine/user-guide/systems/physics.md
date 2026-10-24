# 物理对象、碰撞与查询

本文面向接入刚体、碰撞和空间查询的游戏开发者。先确认单位与场景能力，再创建物理对象，通过命令修改模拟状态，并在合适阶段消费结果。

场景按需创建 `PhysicsWorld`：在运行时特性中同时启用 fixed_step 和 physics，`GameplayScene` 默认已有 fixed_step，但仍须启用 physics。通常让 `GameObject` 实现 `PhysicsParticipant`，加入场景时自动注册，移除时自动注销。业务不直接持有 Box2D 对象，也不重复调用场景物理世界的模拟步 / finalize_frame。需要理解基础对象生命周期时先读[场景](../scene/scene.md)。

## 最小物理对象

把以下类型放在游戏头文件中，在场景进入时通过 `create_and_add_object<FallingBox>()` 创建并检查返回值。它会参与物理，但没有绘制命令；可按[游戏对象](../scene/game-objects.md)添加可视化，或启用[物理调试绘制](../tools/debug-draw.md)。

```cpp
#include "engine/core/game_object.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/physics/contracts/physics_step_participant.h"

#include <array>

class FallingBox final : public elysia::core::GameObject,
                         public elysia::physics::PhysicsParticipant,
                         public elysia::physics::PhysicsStepParticipant
{
public:
    FallingBox() : GameObject(elysia::core::DepthLayer::Item)
    {
        set_world_rect({100, 100, 32, 32});
        // 碰撞形状相对刚体放置；世界位置来自对象的 world_rect。
        _colliders[0].shape = elysia::physics::AabbShape{
            elysia::core::Rect{0, 0, 32, 32}};
    }

    elysia::physics::BodyDefinition body_definition() const override
    {
        elysia::physics::BodyDefinition body;
        body.type = elysia::physics::BodyType::Dynamic;
        body.fixed_rotation = true;
        return body;
    }

    std::span<const elysia::physics::Collider> collider_definitions() const override
    {
        return _colliders;
    }

    void fixed_update(double) override
    {
        if (!physics_world())
            return;
        // 固定步只设置速度，位置由物理世界推进，不能再手动移动一次。
        set_velocity_x(40.0f);
    }

private:
    std::array<elysia::physics::Collider, 1> _colliders{};
};
```

`BodyDefinition` 在注册时复制。Static 用于地面等静态形状，Kinematic 用于主动设速的运动物体，Dynamic 参与力与重力模拟。角色通常禁用旋转；质量可由密度计算，或使用 ExplicitMass 和正 mass。修改对象中的原始定义不会自动更新已注册刚体，应调用公开修改接口。

默认世界重力为零。固定步时长和每帧最大步数属于 `elysia::scene::FixedStepConfig`，字段为 `delta_seconds` 和 `max_steps_per_frame`；Box2D 子步、单位比例与重力属于 `PhysicsWorldConfig`。不能将物理配置直接作为 Scene 构造参数，应通过 `SceneRuntimeFeatures` 组合两种配置。

下面的完整场景类型使用默认的 1/60 秒固定步、每帧最多 8 步、4 个 Box2D 子步和每米 100 个世界单位，并显式提供向下重力：

```cpp
#include "engine/scene/scene.h"

class ConfiguredPhysicsScene final : public elysia::scene::Scene
{
public:
    ConfiguredPhysicsScene()
        : Scene(elysia::scene::SceneRuntimeFeatures{
            .fixed_step = elysia::scene::FixedStepConfig{
                .delta_seconds = 1.0 / 60.0,
                .max_steps_per_frame = 8},
            .physics = elysia::physics::PhysicsWorldConfig{
                .sub_steps = 4,
                .units_per_meter = 100,
                .gravity = {0.0f, 980.0f}}})
    {
    }

    // 本类型只展示物理配置，游戏在 on_enter 中按需添加 FallingBox。
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

坐标 Y 向下，正 Y 重力表示向下。配置在场景构造时确定；需要绘制世界内容时，还须显式配置相机。

创建后用对象的 `physics_state()` 或世界的 `contains_object(handle)` 确认物理注册有效，不能仅凭非空 `GameObject` 指针断言注册成功。无效句柄不继续执行依赖它的玩法绑定。

## 固定步与操作

`PhysicsStepParticipant` 的 fixed_update 在实际物理步中执行；固定步一帧可能零次或多次，输入的一次性事件不能每个补跑步重复执行。普通更新与全局缩放、场景暂停的关系见[时间](time-and-timers.md)。

`PhysicsParticipant` 已提供常用快捷接口：`physics_handle()`、`physics_collider(index)`、`physics_state()`、`velocity()`、`set_velocity(...)`、`set_velocity_x(...)`、`set_velocity_y(...)` 和 `update_physics_collider(index, definition)`。

需要直接操作世界时，使用下列 `PhysicsWorld` 接口。对象、碰撞体和关节使用各自的句柄或标识，不能互换。返回 `bool` 的修改操作用 `false` 表示失败，`body_state`、`render_pose`、`joint_state` 无结果时返回 `std::nullopt`；创建和查找标识还需检查各类型的无效值。

| 目标 | 接口 |
| --- | --- |
| 注册/查询/注销 | `register_object(owner, body, colliders)`、`unregister_object(handle)`、`object_handle(owner)`、`collider_id(handle, index)` |
| 读模拟状态 | `body_state(handle)`；读渲染状态用 `render_pose(handle)` |
| 线速度与角速度 | `set_velocity`、`set_velocity_x`、`set_velocity_y`、`set_angular_velocity` |
| 重力、启用、唤醒 | `set_gravity_scale`、`set_body_enabled`、`set_awake` |
| 力与冲量 | `apply_force`、`apply_impulse`、`apply_torque`、`apply_angular_impulse`；可选 `world_point` 是世界坐标施力点 |
| 瞬移 | `teleport_object(handle, position, mode)` 或 `set_transform(handle, pose, mode)`；`TeleportVelocityMode::Clear` 会清空速度 |

物理单位使用 EU（引擎 units）：位置/长度为 EU，速度为 EU/s，重力为 EU/s²，角度为弧度，质量为 kg。默认 `PhysicsWorldConfig::units_per_meter` 为 100，默认固定步长为 `1.0 / 60.0` 秒。`render_pose` 是前后两次模拟姿态插值后的展示值；判定、存档和游戏逻辑应读取 `body_state`，不要把普通 `GameObject` 的位置写入当作物理瞬移。

## 碰撞体、过滤与响应

`Collider` 支持两种形状：`AabbShape{ Rect local_rect }` 和 `CircleShape{ Vector2 local_center, float radius }`。一个刚体可带多个 `Collider`；调用 `collider_id(handle, index)` 或 `PhysicsParticipant::physics_collider(index)` 获取其稳定 ID。

局部形状以对象 position 对应的刚体原点为基准。下例可放在包含 `engine/physics/physics_world.h` 的对象构造代码中，1、2、4 是示例游戏自行分配的过滤位，与 `DepthLayer` 无关。

```cpp
elysia::physics::Collider body_collider{
    .shape = elysia::physics::AabbShape{{-16.0f, -28.0f, 32.0f, 56.0f}},
    .filter = {.category = std::uint32_t{1}, .mask = std::uint32_t{2} | std::uint32_t{4}},
    .response = elysia::physics::CollisionResponse::Block,
    .material = {.friction = 0.4f, .restitution = 0.0f},
    .tag = "player-body",
};
```

- `CollisionResponse::Block` 会产生实体接触和解算；`Overlap` 产生事件但不阻挡；`Ignore` 不参与。
- `CollisionFilter` 用 `category` / `mask` 双向匹配。相同且非零的 `group` 优先：正数强制碰撞，负数强制忽略。
- `Collider::enabled` 或 `set_collider_enabled(id, false)` 禁用形状；`update_collider(id, definition)` 替换整份已复制的定义。
- `PhysicsMaterial` 提供 `friction`、`restitution`。默认密度是 `SurfaceDensity`（kg/m²）；传感器默认不计入质量，除非设定 `sensor_contributes_mass = true`。
- `BodyDefinition::bullet = true` 与 `Collider::detection_mode = Continuous` 用于高速实体刚体。它们不会把 `Overlap` 传感器或游戏 HitBox 自动变为连续命中；高速攻击请使用下文的显式扫掠查询。

## 碰撞事件、接地和单向平台

实现 `ICollisionListener::on_collision_event(const CollisionEvent&)` 并用 `PhysicsWorld::add_listener(listener)` 注册。事件包含 `CollisionEventPhase::{Begin, Stay, End}`、排序稳定的 `CollisionPair`、法线/穿透/最多两个接触点的 `CollisionManifold`，以及法向和切向冲量。

在 fixed_update 中先检查 physics_world()，再读取 contact_state(physics_handle()).grounded 决定是否允许起跳；它是已有模拟结果，不能预知尚未执行的本步接触。

`contact_state(PhysicsObjectHandle)` 会合并对象全部 `Collider` 的 `grounded`、`ceiling`、`wall_left`、`wall_right`；也可传入 `CollisionTarget`。`collect_contacts(target, out_contacts)` 读取该目标当前的接触快照。法线方向以 `CollisionPair::first` 指向 `second`；读取单个目标时应优先使用 `contact_state`，避免自行反转法线。

单向碰撞由 `Collider::one_way` 或 `TileCollisionCell::one_way` 配置，其中 `OneWayCollision::pass_through` 可组合 `Up`、`Down`、`Left`、`Right`。下落穿透不是修改速度：对每个当前支撑目标调用 `request_pass_through(actor_collider, target)`。该接口仅接受带单向规则的目标，会唤醒角色，并维持忽略直到两个形状分离；角色同时站在多个格子上时要逐格请求。

## 无状态空间查询

查询全部经过 `CollisionFilter`，忽略禁用形状和 `CollisionResponse::Ignore`。`raycast`、`segment_cast` 与 `sweep_aabb` 返回最近命中（无命中为 `std::nullopt`）；`*_all` 把结果写入调用者提供的 `std::vector`，按距离排序并按目标去重。

| 需求 | 查询结构与接口 | 返回 |
| --- | --- | --- |
| 激光/视线 | `RayCastQuery` + `raycast` / `raycast_all` | `CollisionQueryHit` |
| 起点到终点的直线 | `SegmentCastQuery` + `segment_cast` / `segment_cast_all` | `CollisionQueryHit` |
| 范围内方形选取 | `AabbOverlapQuery` + `overlap_aabb` | `CollisionOverlapQueryHit` |
| 范围内圆形选取 | `CircleOverlapQuery` + `overlap_circle` | `CollisionOverlapQueryHit` |
| 高速子弹或角色盒子预判 | `AabbSweepQuery` + `sweep_aabb` | 最近 `CollisionQueryHit` |

`CollisionQueryHit` 给出 `target`（`Collider` 或 Tile）、命中 `point`、`normal`、距离 `distance`、0–1 的 `fraction` 和对方的 `response`。查询不会产生碰撞事件、伤害或物理响应；它适合把高速攻击的“命中时刻”明确交给游戏规则层处理。

## TileMap 碰撞

实现 `ITileCollisionWorld`：提供地图原点、格子尺寸、行列数、越界策略和 `cell_at(TileCoordinate)`。`TileCollisionCell::type` 可为 `Empty`、`Block`、`Overlap` 或 `OneWay`，同时可携带独立的过滤器、材质、tag 与单向规则。

在场景进入且地图数据有效后调用 `physics_world().set_tile_world(map)`，检查 bool；改动格子后调用 `update_tiles(begin, end)` 并检查结果，范围为闭区间。退出或释放地图前调用 `clear_tile_world(map)`，传入同一实例。

TileMap 是借用引用，`ITileCollisionWorld` 实例必须比绑定持续更久。`TileOutOfBoundsPolicy::Block` 会生成外围四面边界墙，`Empty` 则不生成。相邻、过滤规则相同的实体格会抑制内部共享面；暴露面仍可碰撞。当前不包含斜坡、半格、分块流式加载或合并轮廓。

## 关节

`create_distance_joint(const DistanceJointDefinition&)` 创建两刚体之间的固定距离或弹簧；`create_revolute_joint(const RevoluteJointDefinition&)` 创建转轴，可选角度限制和电机。创建成功返回 `JointHandle`；使用 `joint_state(handle)` 读取两端锚点、反作用力和扭矩，使用 `destroy_joint(handle)` 删除。

两个定义都用局部锚点，且 `collide_connected` 默认 `false`。距离关节可设置 `spring`、`frequency_hz`、`damping_ratio`；转轴关节可设置 `enable_limit`、`lower_angle`、`upper_angle`、`enable_motor`、`motor_speed`、`max_motor_torque`。

## 玩法碰撞

物理接触不自动扣血。角色、阵营、HitBox/HurtBox 与攻击去重的完整用法见[玩法碰撞](../gameplay/collision.md)。

## 调试、统计和生命周期注意事项

调试时用 `set_debug_capture(PhysicsDebugCapture::Shapes | PhysicsDebugCapture::Contacts)` 选择捕获项，再用 `debug_snapshot()` 读取。

`PhysicsDebugCapture` 支持 `Shapes`、`BroadPhase`、`Contacts`、`Velocities`、`Joints` 和 `All`；`submit_physics_debug_snapshot(snapshot, debug_draw)` 可提交至引擎调试绘制。

`PhysicsDebugShape` 同时保存前一/当前姿态和 `native_bounds`：后者是物理步的轴对齐原生包围盒，不是旋转 `Collider` 轮廓或 CCD 轨迹。

`PhysicsWorld::last_step_stats()` 提供已注册对象与碰撞体、醒着的刚体、关节、接触数量及步进耗时。固定步执行次数、累计丢弃步数和插值比例属于场景的 `FixedStepStats`，通过 `Scene::fixed_step_stats()` 查询；未启用固定步时返回空指针。当前没有公开的 `accumulator_seconds()` 查询接口。`PhysicsWorld::reset()` 清空物理对象、地图、关节、接触和监听登记，但不重置场景的固定步累积器。

游戏对象应使用所属场景的 `PhysicsWorld`，不要缓存原生 Box2D ID。地图和监听器由世界借用，释放它们之前先解除绑定。物理对象瞬移直接使用 `teleport_object` 或 `set_transform`，不要先通过 `set_position` 改写一份与模拟不一致的位置。渲染读取 `render_pose`（普通对象可用 `render_rect()`），判定读取 `body_state`。

## 监听、查询与清理示例

以下辅助函数用于有有效场景物理世界的业务函数。查询只返回结果，不造成伤害；目标可能是 `Collider` 或 Tile，按 target 的类型处理，不把它强转为 `GameObject`。

```cpp
#include "engine/physics/physics_world.h"

std::optional<elysia::physics::CollisionQueryHit> find_obstacle(
    elysia::physics::PhysicsWorld& world,
    elysia::core::Vector2 start, elysia::core::Vector2 end)
{
    // 这里只查询，不产生碰撞事件；调用方仍需处理没有命中的情况。
    return world.segment_cast({.start = start, .end = end});
}
```

无命中为 nullopt，是正常结果；根据玩法设置查询 filter，避免命中不关心的层。范围查询使用输出 vector，结果是物理目标标识，不拥有目标。

监听器派生 `ICollisionListener` 并实现 `on_collision_event`，在场景进入时 `add_listener(*listener)` 并检查结果，退出和析构前 `remove_listener(*listener)`。世界只借用监听器；回调中记录待办操作，返回后再销毁监听者或重组对象集合。事件中的句柄也可能在后续对象移除后失效。

关节示例：在两个刚体注册成功后，创建 `DistanceJointDefinition{.first = a, .second = b, .length = 100.0f}` 并交给 create_distance_joint；检查返回句柄的 is_valid()。完成使用时 destroy_joint，后续读取 joint_state 仍要检查 optional。关节依赖两端刚体，不能跨不同 `PhysicsWorld` 连接。

`TileCollisionCell` 的 tag 是 string_view，提供地图的对象和其标签存储都必须覆盖绑定期。物理世界 reset 会清空注册，不要在仍有场景对象依赖其绑定时单独重置世界；重置整个关卡可选择场景 Recreate。

常见误用是重复注册场景已注册的对象、用 set_position 瞬移物理刚体、在渲染帧重复施加本应按固定步消费的冲量，以及忘记解除地图或监听器借用。

完整公开契约见[接口参考](../../reference/physics/README.md)。

地图绑定、脏区刷新与单向平台操作见[地图碰撞指南](tilemap-collision.md)。

## 参考

- [物理公开接口](../../../../engine/physics/physics_world.h)、[查询参数](../../../../engine/physics/collision/collision_query.h)
- [玩法碰撞](../gameplay/collision.md)、[对象查询](object-query.md)、[返回使用指南](../README.md)
