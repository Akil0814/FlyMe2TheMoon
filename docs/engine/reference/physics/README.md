# 物理系统：功能与接口总览

本项目的物理系统以 `elysia::physics::PhysicsWorld` 为唯一的场景内物理世界，底层使用 Box2D 3.1.1。游戏代码只依赖 Elysia 的定义、句柄、查询和事件类型，不直接保存 Box2D 对象或包含 Box2D 头文件。

`elysia::scene::Scene` 通过运行时特性按需创建 `FixedStepRuntime` 和 `PhysicsWorld`，物理必须同时启用固定步。实现 `PhysicsParticipant` 的对象加入场景时自动注册，回收前自动注销。内部生命周期更新入口在普通对象与 UI 呈现动画之后推进固定步：先执行场景固定步钩子，再调用 `PhysicsWorld::step`，帧末调用 `finalize_frame` 更新插值。

游戏扩展使用 `on_before_update` / `on_after_update`，不重写或手动调用基础调度。

本文面向需要查询物理能力与公开调用的开发者。每项功能先说明用途，再介绍接口和限制；运行机制与所有权详见[物理架构](../../architecture/subsystems/physics/02-target-architecture.md)。

## 能力清单

| 功能 | 用途 | 主要接口 |
| --- | --- | --- |
| 刚体模拟 | 静态、运动学、动态刚体；重力、阻尼、睡眠、旋转、质量 | `BodyDefinition`、`PhysicsWorld::set_velocity`、`apply_force`、`apply_impulse` |
| 碰撞体 | 四种有面积形状与双面 Segment；材质、过滤、实体与重叠 | `Collider`、`BoxShape`、`CircleShape`、`CapsuleShape`、`ConvexPolygonShape`、`SegmentShape` |
| 连续地形 | 静态／运动学开放、闭合单面边链 | `ChainDefinition`、`create_chain`、`chain_state`、`chain_collider` |
| 运动学目标 | 每固定步提交位置与角度 | `set_target_transform` |
| 连续碰撞 | 对高速动态刚体启用 CCD | `BodyDefinition::bullet`、`Collider::detection_mode` |
| 固定步调度 | 场景累计帧时间并限制补步次数 | `FixedStepConfig`、场景持有的 `FixedStepRuntime` |
| 物理模拟与帧末插值 | 固定步执行模拟，帧末更新插值系数 | `PhysicsWorld::step`、`finalize_frame` |
| 渲染快照 | 读取前后模拟姿态的插值结果 | `PhysicsWorld::render_pose` |
| 碰撞事件与接地状态 | Begin/Stay/End、接触流形、冲量、地面/墙/天花状态 | `ICollisionListener`、`CollisionEvent`、`contact_state`、`collect_contacts` |
| 空间查询 | 射线、线段、四种形状重叠与平移扫掠 | `raycast`、`segment_cast`、`overlap_shape`、`shape_cast`、`shape_cast_all` |
| TileMap 碰撞 | 方块、触发、单向平台、边界墙、局部脏区更新 | `ITileCollisionWorld`、`set_tile_world`、`update_tiles`、`request_pass_through` |
| 关节 | 八种关节及七类可变设置 | `create_*_joint`、`update_*_joint`、`joint_state`、`destroy_joint` |
| 游戏碰撞语义 | Body/PushBox/HurtBox/HitBox/Sensor，阵营过滤及一次攻击去重 | `GameplayCollisionRuntime`、`GameplayCollisionService`、`GameplayCollisionListener` |
| 调试与统计 | 碰撞体、原生 AABB、接触、速度、关节与步进统计 | `set_debug_capture`、`debug_snapshot`、`last_step_stats` |

## 1. 物理对象与刚体控制

实现 `elysia::physics::PhysicsParticipant` 的 `GameObject` 会在 `Scene::add_object` 时自动注册。实现 `PhysicsStepParticipant` 后，`fixed_update(double fixed_delta_seconds)` 会在每一个实际物理步执行，适合读取已锁存的输入并施加力或设置速度；不要在渲染帧里按帧率累积物理控制。

完整物理对象、碰撞体初始化和固定步示例见[物理指南](../../user-guide/systems/physics.md#最小物理对象)。

`PhysicsParticipant` 已提供常用快捷接口：`physics_handle()`、`physics_collider(index)`、`physics_state()`、`velocity()`、`set_velocity(...)`、`set_velocity_x(...)`、`set_velocity_y(...)` 和 `update_physics_collider(index, definition)`。

需要直接操作世界时，使用下列 `PhysicsWorld` 接口。所有对象、碰撞体和关节 ID 都是不透明句柄；传入失效句柄的命令返回 `false`，状态读取返回 `std::nullopt`。

| 目标 | 接口 |
| --- | --- |
| 注册/查询/注销 | `register_object(owner, body, colliders)`、`unregister_object(handle)`、`object_handle(owner)`、`collider_id(handle, index)` |
| 读模拟状态 | `body_state(handle)`；读渲染状态用 `render_pose(handle)` |
| 线速度与角速度 | `set_velocity`、`set_velocity_x`、`set_velocity_y`、`set_angular_velocity` |
| 重力、启用、唤醒 | `set_gravity_scale`、`set_body_enabled`、`set_awake` |
| 力与冲量 | `apply_force`、`apply_impulse`、`apply_torque`、`apply_angular_impulse`；可选 `world_point` 是世界坐标施力点 |
| 瞬移 | `teleport_object(handle, position, mode)` 或 `set_transform(handle, pose, mode)`；`TeleportVelocityMode::Clear` 会清空速度 |

物理单位使用 EU（引擎 units）：位置/长度为 EU，速度为 EU/s，重力为 EU/s²，角度为弧度，质量为 kg。默认 `PhysicsWorldConfig::units_per_meter` 为 100，默认固定步长为 `1.0 / 60.0` 秒。`render_pose` 是前后两次模拟姿态插值后的展示值；判定、存档和游戏逻辑应读取 `body_state`，不要把普通 `GameObject` 的位置写入当作物理瞬移。

## 2. 碰撞体、过滤与响应

`Collider` 支持 `BoxShape`、`CircleShape`、`CapsuleShape`、`ConvexPolygonShape` 和 `SegmentShape`。形状均使用刚体局部坐标，一个刚体可带多个碰撞体；用 `collider_id(handle, index)` 或 `PhysicsParticipant::physics_collider(index)` 获取稳定 ID。

| 形状 | 定义 |
| --- | --- |
| 矩形 | `BoxShape{local_center, half_extents, local_angle}`，半尺寸必须为正；`BoxShape::from_rect(rect)` 从局部矩形构造零角度盒子 |
| 圆形 | `CircleShape{local_center, radius}`，半径必须为正 |
| 胶囊 | `CapsuleShape{local_first, local_second, radius}`；两端重合时按圆形处理；原生精度范围内的极短胶囊会折叠为中点圆形 |
| 凸多边形 | `ConvexPolygonShape` 的 `local_vertices` 最多 8 个，`vertex_count` 为 3–8；顶点沿边界排列，顺逆绕序均可 |
| 线段 | `SegmentShape{local_first, local_second}`，零厚度双面，仅 Static／Kinematic |

多边形拒绝凹形、自交、重复顶点、共线退化和非有限坐标；不会自动把凹形分解。注册和查询使用相同校验及单位转换；形状还须满足 Box2D 的几何精度及有限质量／惯量要求。`update_collider` 保留 `ColliderId`，非法替换保留原有形状。斜坡可用独立的静态凸多边形表达；Tile 格子仍是矩形。

碰撞体配置示例见[过滤与响应](../../user-guide/systems/physics.md#碰撞体过滤与响应)。

- `CollisionResponse::Block` 会产生实体接触和解算；`Overlap` 产生事件但不阻挡；`Ignore` 不参与。
- `CollisionFilter` 用 `category` / `mask` 双向匹配。相同且非零的 `group` 优先：正数强制碰撞，负数强制忽略。
- `Collider::enabled` 或 `set_collider_enabled(id, false)` 禁用形状；`update_collider(id, definition)` 替换整份已复制的定义。
- `PhysicsMaterial` 提供 `friction`、`restitution`。默认密度是 `SurfaceDensity`（kg/m²）；传感器默认不计入质量，除非设定 `sensor_contributes_mass = true`。
- `BodyDefinition::bullet = true` 与 `Collider::detection_mode = Continuous` 用于高速实体刚体。它们不会把 `Overlap` 传感器或游戏 HitBox 自动变为连续命中；高速攻击请使用第 4 节的显式扫掠查询。

## 3. 碰撞事件、接地和单向平台

实现 `ICollisionListener::on_collision_event(const CollisionEvent&)` 并用 `PhysicsWorld::add_listener(listener)` 注册。事件包含 `CollisionEventPhase::{Begin, Stay, End}`、排序稳定的 `CollisionPair`、法线/穿透/最多两个接触点的 `CollisionManifold`，以及法向和切向冲量。

监听器注册与接触查询的完整上下文见[监听、查询与清理示例](../../user-guide/systems/physics.md#监听查询与清理示例)。

`contact_state(PhysicsObjectHandle)` 会合并对象全部 `Collider` 的 `grounded`、`ceiling`、`wall_left`、`wall_right`；也可传入 `CollisionTarget`。`collect_contacts(target, out_contacts)` 读取该目标当前的接触快照。法线方向以 `CollisionPair::first` 指向 `second`；读取单个目标时应优先使用 `contact_state`，避免自行反转法线。

单向碰撞由 `Collider::one_way` 或 `TileCollisionCell::one_way` 配置，其中 `OneWayCollision::pass_through` 可组合 `Up`、`Down`、`Left`、`Right`。下落穿透不是修改速度：对每个当前支撑目标调用 `request_pass_through(actor_collider, target)`。该接口仅接受带单向规则的目标，会唤醒角色，并维持忽略直到两个形状分离；角色同时站在多个格子上时要逐格请求。

## 4. 无状态空间查询

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

## 5. TileMap 碰撞

实现 `ITileCollisionWorld`：提供地图原点、格子尺寸、行列数、越界策略和 `cell_at(TileCoordinate)`。`TileCollisionCell::type` 可为 `Empty`、`Block`、`Overlap` 或 `OneWay`，同时可携带独立的过滤器、材质、tag 与单向规则。

绑定、刷新和解除地图的操作步骤见[地图碰撞指南](../../user-guide/systems/tilemap-collision.md)。

TileMap 是借用引用，`ITileCollisionWorld` 实例必须比绑定持续更久。`TileOutOfBoundsPolicy::Block` 会生成外围四面边界墙，`Empty` 则不生成。相邻、过滤规则相同的实体格会抑制内部共享面；暴露面仍可碰撞。当前不包含斜坡、半格、分块流式加载或合并轮廓。

## 6. 八种关节与运行时设置

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
## 7. 面向游戏的碰撞运行时

物理层只报告形状接触；`elysia::gameplay::collision::GameplayCollisionRuntime` 把 `Collider` 绑定为角色、阵营和角色用途，并路由更适合玩法的事件。`GameplayScene` 自己持有该运行时；场景激活时由 `GameplayCollisionService` 挂接，因此一般玩法代码通过 `GameplayCollisionService::instance()` 调用，而不是另外创建运行时。

| `ColliderRole` | 绑定方式 | 事件 |
| --- | --- | --- |
| `Body` | `ActorCollisionRig::body` 或 `bind_collider` | `GameplayCollisionListener::on_body_contact` |
| `PushBox` | `ActorCollisionRig::push_box` 或 `bind_collider` | `on_push_box_overlap`（仅两个 PushBox） |
| `HurtBox` | `ActorCollisionRig::hurt_boxes` 或 `bind_collider` | 被 HitBox 命中时作为目标 |
| `HitBox` | `bind_hit_box(HitBoxBinding)` | `on_hit_overlap` |
| `Sensor` | `ActorCollisionRig::sensors` 或 `bind_collider` | `on_sensor_overlap`（仅与 Body 的 Overlap） |

角色、HitBox 和攻击实例的配套绑定示例见[玩法碰撞指南](../../user-guide/gameplay/collision.md)。

`HitBox` 只在 Begin 时对 `HurtBox` 路由，且仅阵营关系为 `Hostile` 时触发；同一个 `(attack_instance, hurt_actor)` 在攻击存续期内只会命中一次。攻击结束后必须调用 `end_attack_instance(id)` 清除去重记录和该攻击的 HitBox 绑定；角色或形状生命周期结束时调用 `unbind_actor(actor)` 或 `unbind_collider(collider)`。

默认先判断同一有效阵营为 Friendly（双方为 Neutral 也一样），再判断任一 Neutral 或无效阵营为 Neutral，其余不同有效阵营为 Hostile；命中判定使用已登记 instigator 的队伍，而非只读取 HitBox 的 team；可由 `GameplayCollisionRuntime::set_team_relation_resolver(...)` 替换。

`GameplayCollisionListener` 可覆写 `on_body_contact`、`on_push_box_overlap`、`on_hit_overlap` 和 `on_sensor_overlap`。监听器在事件回调中可以请求解绑等变更，当前事件批次保持稳定；监听对象本身必须在这批回调结束前仍然存活。

## 8. 调试、统计和生命周期注意事项

调试时用 `set_debug_capture(PhysicsDebugCapture::Shapes | PhysicsDebugCapture::Contacts)` 选择捕获项，再用 `debug_snapshot()` 读取。

`PhysicsDebugCapture` 支持 `Shapes`、`BroadPhase`、`Contacts`、`Velocities`、`Joints` 和 `All`；`submit_physics_debug_snapshot(snapshot, debug_draw)` 可提交至引擎调试绘制。

`PhysicsDebugShape` 同时保存前一/当前姿态和 `native_bounds`：后者是物理步的轴对齐原生包围盒，不是旋转 `Collider` 轮廓或 CCD 轨迹。

`last_step_stats()` 提供物理对象、`Collider`、刚体、关节、接触和步进统计；固定步累计、固定步、插值和丢弃步数属于独立的 `FixedStepRuntime`，通过场景 `fixed_step_stats()` 查询。`PhysicsWorld::reset()` 清空世界；场景 Reset 路由重置固定步运行时，再执行游戏 `on_reset()`，不会自动清空全部业务对象。

最后的实践边界：一个 `Scene` 只应使用它自身的 `PhysicsWorld`；不要缓存原生 Box2D ID；地图绑定和监听器都要先于世界销毁；普通位置直接改写后必须用 `teleport_object` 或 `set_transform` 同步物理；渲染读 `render_pose`，判定读 `body_state`。这样可以避免渲染、碰撞和游戏规则各自使用不同的位置来源。

## Segment 与 Chain 地形

`SegmentShape{local_first, local_second}` 是双面、零厚度的局部线段，仅可挂在 Static 或 Kinematic 对象上。它沿用普通 Collider 的过滤、材质、响应、启停和稳定 ID；不贡献质量。更新非法时保留原形状。线段之间不承诺产生物理解算，重叠与扫掠的查询源仍只接受矩形、圆、胶囊和凸多边形。

需要连续地形时，在已注册的 Static／Kinematic 对象上调用 `create_chain(owner, definition)`。`ChainDefinition` 复制并拥有 `local_vertices` 和 `tag`，统一使用 `filter` 和 `material`，始终是可被传感器检测的实体地形。开放链至少两个实际顶点，首尾 ghost 默认沿端边外推，也可通过 `ghost_before`／`ghost_after` 指定；ghost 不生成碰撞边。闭合链 `loop=true` 至少四个顶点，输入不重复首点。原生尺度下边长须至少 0.005 米；重复、退化、自交、非有限坐标和相邻折返会被拒绝。输入顺序保持不变。

局部边 `a→b` 的正面法线为 `(dy, -dx)`，随后随刚体旋转。在 Y 向下的坐标中，水平边从左到右时阻挡上方。调试绘制显示线段与正面箭头。Chain 的实体接触和射线使用单面规则；`overlap_shape` 与 `shape_cast` 是几何检测，从背面也可能命中。使用扫掠结果移动角色时须由玩法明确决定如何处理这一差异。

`chain_state(handle)` 返回 owner、开放／闭合模式和顶点／边段数量；`chain_collider(handle, index)` 取得每条实际边的 ColliderId；`chain_segment_info(id)` 反查 Chain、owner 与零起始边段索引。查询和事件仍返回 `CollisionTarget::Collider`，接触状态会合并所属对象的 Chain 子段；忽略 owner 会排除整组地形。普通 `collider_id(owner, index)` 只访问对象声明的 Collider。

Chain 几何、过滤和材质在创建后固定，修改时销毁再创建。`update_collider` 与 `set_collider_enabled` 拒绝单独操作子段；用 `set_body_enabled` 启停整组。`destroy_chain`、移除 owner 和 Reset 回收所有子段及映射。回调中创建／销毁按 FIFO 提交，定义按值保存；提交前 `chain_state` 为空。创建失败返回无效句柄，分配异常向上传播并清理未完成资源；不保留可用的原生 ID。

## 每固定步提交运动学目标

`set_target_transform(handle, PhysicsPose target, double fixed_delta_seconds)` 根据命令执行时的原生姿态计算速度。`PhysicsParticipant` 提供同名快捷方法。仅接受 Kinematic、有限目标和有效正时间步；非法请求返回 `false`。固定旋转只追踪位置，角度采用最短路径；同位姿或低于原生休眠阈值的目标清除旧速度。

在 `fixed_update(dt)` 中每步提交一个目标。它不瞬移，不保存持续追踪状态，也不保证单步精确到位；停止提交后仍沿用当前速度，需要停止时显式设速。回调参数按值复制，执行前检查对象仍有效且启用，与设速、角速度和传送命令遵循 FIFO；渲染继续读取 `render_pose`。平台示例见物理测试中心的 `target_pose` 页。

Filter 与 `collide_connected` 遵循内置 Box2D 的实体接触规则；它们不屏蔽原生 Sensor 重叠，也不改变无状态查询。需要排除传感器或查询目标时，使用 Collider 过滤器或 `CollisionQueryFilter`。
