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
        _colliders[0].shape = elysia::physics::BoxShape::from_rect(
            elysia::core::Rect{0, 0, 32, 32});
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

`Collider` 支持 `BoxShape`、`CircleShape`、`CapsuleShape`、`ConvexPolygonShape` 和 `SegmentShape`。形状均使用刚体局部坐标，一个刚体可带多个碰撞体；用 `collider_id(handle, index)` 或 `PhysicsParticipant::physics_collider(index)` 获取稳定 ID。

| 形状 | 定义 |
| --- | --- |
| 矩形 | `BoxShape{local_center, half_extents, local_angle}`，半尺寸必须为正；`BoxShape::from_rect(rect)` 从局部矩形构造零角度盒子 |
| 圆形 | `CircleShape{local_center, radius}`，半径必须为正 |
| 胶囊 | `CapsuleShape{local_first, local_second, radius}`；两端重合时按圆形处理；原生精度范围内的极短胶囊会折叠为中点圆形 |
| 凸多边形 | `ConvexPolygonShape` 的 `local_vertices` 最多 8 个，`vertex_count` 为 3–8；顶点沿边界排列，顺逆绕序均可 |
| 线段 | `SegmentShape{local_first, local_second}`，零厚度双面，仅 Static／Kinematic |

多边形拒绝凹形、自交、重复顶点、共线退化和非有限坐标；不会自动把凹形分解。注册和查询使用相同校验及单位转换；形状还须满足 Box2D 的几何精度及有限质量／惯量要求。`update_collider` 保留 `ColliderId`，非法替换保留原有形状。斜坡可用独立的静态凸多边形表达；Tile 格子仍是矩形。

局部形状以对象 position 对应的刚体原点为基准。下例可放在包含 `engine/physics/physics_world.h` 的对象构造代码中，1、2、4 是示例游戏自行分配的过滤位，与 `DepthLayer` 无关。

```cpp
elysia::physics::Collider body_collider{
    .shape = elysia::physics::BoxShape::from_rect({-16.0f, -28.0f, 32.0f, 56.0f}),
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

查询统一使用 `CollisionQueryFilter`：`collision` 保存类别、掩码和分组；`ignored_object` 排除一个对象的全部碰撞体；`include_sensors` 默认 `true`。禁用形状和 `Ignore` 始终被排除。查询不执行单向平台的运动响应规则，命中后由业务决定如何处理。

| 需求 | 接口 | 返回 |
| --- | --- | --- |
| 射线 | `RayCastQuery` + `raycast` / `raycast_all` | `CollisionQueryHit` |
| 起点到终点 | `SegmentCastQuery` + `segment_cast` / `segment_cast_all` | `CollisionQueryHit` |
| 任意支持形状的范围检测 | `ShapeOverlapQuery{shape, pose, filter}` + `overlap_shape` | 输出 `CollisionOverlapQueryHit` 数组 |
| 形状平移扫掠 | `ShapeCastQuery{shape, pose, displacement, filter}` + `shape_cast` / `shape_cast_all` | 最近命中或全部 `CollisionQueryHit` |

`shape` 使用 `ColliderShape` 中四种有面积形状（不接受 Segment 查询源），`pose` 是世界姿态，`displacement` 是世界位移。扫掠保持角度不变，不做旋转扫掠。最近命中与全部结果首项一致；全部结果按 `fraction`、目标标识排序并按目标去重，不会合并同一对象的不同 Collider。

命中包含 `target`（Collider 或 Tile）、`point`、`normal`、`distance`、`fraction`、`response` 和 `initial_overlap`。起点已在目标内时，`initial_overlap=true`，距离和比例为零，点为查询起点或姿态原点，法线为零；此时点和法线不能作为接触流形。零位移形状扫掠仅检测初始重叠。非法输入返回空结果并清空输出容器。

查询不会产生碰撞事件、伤害或物理响应。高速 Sensor/HitBox 仍须由玩法显式扫掠并消费命中，刚体 CCD 不会自动提供连续攻击判定。

## TileMap 碰撞

实现 `ITileCollisionWorld`：提供地图原点、格子尺寸、行列数、越界策略和 `cell_at(TileCoordinate)`。`TileCollisionCell::type` 可为 `Empty`、`Block`、`Overlap` 或 `OneWay`，同时可携带独立的过滤器、材质、tag 与单向规则。

在场景进入且地图数据有效后调用 `physics_world().set_tile_world(map)`，检查 bool；改动格子后调用 `update_tiles(begin, end)` 并检查结果，范围为闭区间。退出或释放地图前调用 `clear_tile_world(map)`，传入同一实例。

TileMap 是借用引用，`ITileCollisionWorld` 实例必须比绑定持续更久。`TileOutOfBoundsPolicy::Block` 会生成外围四面边界墙，`Empty` 则不生成。相邻、过滤规则相同的实体格会抑制内部共享面；暴露面仍可碰撞。当前不包含斜坡、半格、分块流式加载或合并轮廓。

## 八种关节与运行时设置

所有创建函数返回 `JointHandle`。定义固定两端对象、局部锚点、参考角和局部轴；轴接受有限非零向量并规范化。机械关节至少一端 Dynamic；Mouse 要求第一端 Static、第二端 Dynamic；Filter 只要求两个不同的有效刚体。下表中的可变设置均由对应 `update_*_joint(handle, settings)` 完整替换，保持原关节并唤醒连接刚体。Filter 没有空设置接口。

| 类型／创建接口 | Settings 主要字段 | `JointState.details` |
| --- | --- | --- |
| `create_distance_joint` | `length`、弹簧、长度限位、线性电机 | `DistanceJointState`：当前长度、速度、电机力 |
| `create_revolute_joint` | `target_angle`、弹簧、角限位、旋转电机 | `RevoluteJointState`：角度、角速度、电机扭矩 |
| `create_weld_joint` | `linear_frequency_hz`、`angular_frequency_hz`、对应 `*_damping_ratio` | `WeldJointState`：`linear_error`、`angular_error` |
| `create_prismatic_joint` | `enable_spring`、`target_translation`、频率／阻尼、`enable_limit`、`lower_translation`／`upper_translation`、`enable_motor`、`motor_speed`／`max_motor_force` | `PrismaticJointState`：`translation`、`speed`、`motor_force` |
| `create_wheel_joint` | 悬挂 `enable_spring`、频率／阻尼、平移限位、旋转电机 `motor_speed`／`max_motor_torque` | `WheelJointState`：悬挂 `translation`／`speed`、`angular_velocity`、`motor_torque` |
| `create_mouse_joint` | 世界 `target`、`frequency_hz`／`damping_ratio`、`max_force` | `MouseJointState`：`drag_point`、`error`；初始目标决定固定拖拽锚点 |
| `create_motor_joint` | 第一端局部 `linear_offset`、相对 `angular_offset`、`max_force`／`max_torque`、`correction_factor` | `MotorJointState`：局部位移误差与角度误差 |
| `create_filter_joint` | 无 | `FilterJointState`：配对碰撞已屏蔽 |

Settings 均默认 `collide_connected=false`。长度／平移速度／力／扭矩遵循 EU 单位；角度用弧度。频率、阻尼和最大输出非负，限位有序，校正系数在 `[0,1]`。距离长度必须为正；关闭距离弹簧时限位与电机不生效。转轴角限位在 ±0.99π 内。

Prismatic 默认关闭弹簧、限位和电机；Wheel 默认开启悬挂弹簧。通用弹簧为 2 Hz／阻尼比 0.7；Weld 两项频率为零、阻尼比 1。Mouse 默认最大力 1000、4 Hz、阻尼比 1；Motor 默认最大力 100、最大扭矩 10000、校正系数 0.3。其余电机默认关闭、速度和输出为零。

`joint_state` 的公共字段包括 `first`／`second` 对象身份、世界锚点、`reaction_force` 与 `reaction_torque`。Filter 不施加机械约束，反作用力和扭矩为零，调试虚线只标记过滤关系；销毁后恢复通常碰撞规则。约束有限输出可能保留误差，状态用于观察实际结果。

非法参数、错误类型和失效句柄返回 `false` 或空 `optional`，保留原设置。回调中的参数复制进 FIFO；创建后可立即更新，执行前再次检查对象和类型。创建尚未提交、销毁、移除两端或 Reset 后状态为空。物理测试中心的六个 `joint_*` 页提供设置切换、状态显示；Mouse 可用左键拖动目标，Filter 可切换配对碰撞。
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

关节示例：在两个刚体注册成功后，创建 `DistanceJointDefinition{.first = a, .second = b, .settings = {.length = 100.0f}}` 并交给 create_distance_joint；检查返回句柄的 is_valid()。完成使用时 destroy_joint，后续读取 joint_state 仍要检查 optional。关节依赖两端刚体，不能跨不同 `PhysicsWorld` 连接。

`TileCollisionCell` 的 tag 是 string_view，提供地图的对象和其标签存储都必须覆盖绑定期。物理世界 reset 会清空注册，不要在仍有场景对象依赖其绑定时单独重置世界；重置整个关卡可选择场景 Recreate。

常见误用是重复注册场景已注册的对象、用 set_position 瞬移物理刚体、在渲染帧重复施加本应按固定步消费的冲量，以及忘记解除地图或监听器借用。

完整公开契约见[接口参考](../../reference/physics/README.md)。

地图绑定、脏区刷新与单向平台操作见[地图碰撞指南](tilemap-collision.md)。

## 参考

- [物理公开接口](../../../../engine/physics/physics_world.h)、[查询参数](../../../../engine/physics/collision/collision_query.h)
- [玩法碰撞](../gameplay/collision.md)、[对象查询](object-query.md)、[返回使用指南](../README.md)

## 形状扫掠示例

```cpp
#include "engine/physics/physics_world.h"

std::optional<elysia::physics::CollisionQueryHit> cast_capsule(
    const elysia::physics::PhysicsWorld& world,
    elysia::physics::PhysicsObjectHandle actor,
    elysia::core::Vector2 position, elysia::core::Vector2 displacement)
{
    return world.shape_cast({
        .shape = elysia::physics::CapsuleShape{{0, -12}, {0, 12}, 8},
        .pose = {position, 0},
        .displacement = displacement,
        .filter = {.ignored_object = actor}});
}
```

## 运行中调整关节

物理测试中心的关节页可调整长度和反转电机。点击控制按钮会进入持续手动模拟；重置后返回自动验证。

```cpp
#include "engine/physics/physics_world.h"

bool reverse_joint_motor(elysia::physics::PhysicsWorld& world,
                         elysia::physics::JointHandle handle)
{
    auto state = world.joint_state(handle);
    if (!state)
        return false;
    const auto* hinge = std::get_if<elysia::physics::RevoluteJointState>(&state->details);
    if (!hinge)
        return false;
    auto settings = hinge->settings;
    settings.motor_speed = -settings.motor_speed;
    return world.update_revolute_joint(handle, settings);
}
```

## Segment 与 Chain 地形

`SegmentShape{local_first, local_second}` 是双面、零厚度的局部线段，仅可挂在 Static 或 Kinematic 对象上。它沿用普通 Collider 的过滤、材质、响应、启停和稳定 ID；不贡献质量。更新非法时保留原形状。线段之间不承诺产生物理解算，重叠与扫掠的查询源仍只接受矩形、圆、胶囊和凸多边形。

需要连续地形时，在已注册的 Static／Kinematic 对象上调用 `create_chain(owner, definition)`。`ChainDefinition` 复制并拥有 `local_vertices` 和 `tag`，统一使用 `filter` 和 `material`，始终是可被传感器检测的实体地形。开放链至少两个实际顶点，首尾 ghost 默认沿端边外推，也可通过 `ghost_before`／`ghost_after` 指定；ghost 不生成碰撞边。闭合链 `loop=true` 至少四个顶点，输入不重复首点。原生尺度下边长须至少 0.005 米；重复、退化、自交、非有限坐标和相邻折返会被拒绝。输入顺序保持不变。

局部边 `a→b` 的正面法线为 `(dy, -dx)`，随后随刚体旋转。在 Y 向下的坐标中，水平边从左到右时阻挡上方。调试绘制显示线段与正面箭头。Chain 的实体接触和射线使用单面规则；`overlap_shape` 与 `shape_cast` 是几何检测，从背面也可能命中。使用扫掠结果移动角色时须由玩法明确决定如何处理这一差异。

`chain_state(handle)` 返回 owner、开放／闭合模式和顶点／边段数量；`chain_collider(handle, index)` 取得每条实际边的 ColliderId；`chain_segment_info(id)` 反查 Chain、owner 与零起始边段索引。查询和事件仍返回 `CollisionTarget::Collider`，接触状态会合并所属对象的 Chain 子段；忽略 owner 会排除整组地形。普通 `collider_id(owner, index)` 只访问对象声明的 Collider。

Chain 几何、过滤和材质在创建后固定，修改时销毁再创建。`update_collider` 与 `set_collider_enabled` 拒绝单独操作子段；用 `set_body_enabled` 启停整组。`destroy_chain`、移除 owner 和 Reset 回收所有子段及映射。回调中创建／销毁按 FIFO 提交，定义按值保存；提交前 `chain_state` 为空。创建失败返回无效句柄，分配异常向上传播并清理未完成资源；不保留可用的原生 ID。

## 每固定步提交运动学目标

`set_target_transform(handle, PhysicsPose target, double fixed_delta_seconds)` 根据命令执行时的原生姿态计算速度。`PhysicsParticipant` 提供同名快捷方法。仅接受 Kinematic、有限目标和有效正时间步；非法请求返回 `false`。固定旋转只追踪位置，角度采用最短路径；同位姿或低于原生休眠阈值的目标清除旧速度。

在 `fixed_update(dt)` 中每步提交一个目标。它不瞬移，不保存持续追踪状态，也不保证单步精确到位；停止提交后仍沿用当前速度，需要停止时显式设速。回调参数按值复制，执行前检查对象仍有效且启用，与设速、角速度和传送命令遵循 FIFO；渲染继续读取 `render_pose`。平台示例见物理测试中心的 `target_pose` 页。

下例在已经注册的对象上建立地形、提交平台目标及创建滑动关节；调用方检查无效句柄和 bool 返回值。

```cpp
#include "engine/physics/physics_world.h"

elysia::physics::ChainHandle add_terrain(
    elysia::physics::PhysicsWorld& world, elysia::physics::PhysicsObjectHandle owner)
{
    elysia::physics::ChainDefinition chain;
    chain.local_vertices = {{0, 0}, {200, 0}, {300, -50}};
    chain.tag = "terrain";
    return world.create_chain(owner, chain); // owner must be Static or Kinematic
}

bool command_platform(elysia::physics::PhysicsWorld& world,
    elysia::physics::PhysicsObjectHandle platform, double dt)
{
    return world.set_target_transform(platform, {{200, 100}, 0.15f}, dt);
}

elysia::physics::JointHandle add_slider(elysia::physics::PhysicsWorld& world,
    elysia::physics::PhysicsObjectHandle anchor, elysia::physics::PhysicsObjectHandle body)
{
    elysia::physics::PrismaticJointDefinition joint{anchor, body};
    joint.local_axis = {1, 0};
    joint.settings.enable_motor = true;
    joint.settings.motor_speed = 30;
    joint.settings.max_motor_force = 1000;
    return world.create_prismatic_joint(joint);
}
```
