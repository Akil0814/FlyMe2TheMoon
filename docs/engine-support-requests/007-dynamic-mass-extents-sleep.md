# 007：动态物理体质量设置后缺失形状尺度，纯旋转可能错误休眠

状态：已复现；游戏层已用公开的逐物体休眠策略缓解持续驱动机关。未修改引擎或 Box2D。

## 现象与最小复现

第 39 关 ID 0 为 64×8 的转杆，密度 0.5、居中转轴、30°/秒、100 N·m 电机。原生采样中质量为 0.25 kg，惯量约 86.6667 kg·EU²。启动后转过约 15°，在 0.5 秒时角速度变为 0；这发生在剧情暂停之前，恢复逻辑或再按 P 暂停/继续都不会自行重启。

`game/tests/visual_tests.cpp` 的 `check_driven_rotor_pause` 在真实 PhysicsWorld 中单独注册相同 Entity、静态 Pivot 与电机关节，无地图、角色、激光搭载或碰撞接触。修复游戏策略前稳定失败，状态为 `angular_velocity=0, awake=false`。因此不能归因于暂停键未恢复或地形卡住。运行时与采样在 `out/qa/level39-pause-audit/`。

## 源码证据

`PhysicsWorld::Impl::create_shape` 设置 `b2ShapeDef.updateBodyMass=false`。动态体的 `Impl::mass` 通过 `b2Body_SetMassData` 应用游戏质量数据；当前 Box2D 的该方法只设置质量、惯量、质心，不计算 `minExtent/maxExtent`。对应的 `b2UpdateBodyMassData` 才会从形状计算范围，但当前动态体路径没有调用它。非动态体路径已有 `b2Body_ApplyMassFromShapes`，动态体仍缺少相应范围维护。

Box2D 求解器以 `|linearVelocity| + |angularVelocity| * maxExtent` 判断物体运动；范围未维护时，原地旋转的角速度可能无法阻止休眠。源码链路与公开状态的隔离复现一致；本次没有在运行时读取 Box2D 私有字段。

## 建议的引擎处理

由引擎在动态体创建、形状更新与质量/质心覆盖时维护正确的形状尺度，同时保留现有独立质量、惯量和质心语义。建议增加默认密度质量、显式质量、独立惯量、非零质心，以及动态形状更新后的纯旋转休眠回归。具体内部实现由引擎负责，游戏不直接操作原生刚体。

已有公开接口足以缓解当前机关：`BodyDefinition.enable_sleep=false` 可以让持续通电的电机转杆保持活跃；游戏现仅对非零 Speed 的旋转机关使用该策略，31 个电机转杆受此策略覆盖。普通受推力的被动杆继续保留休眠。暂停仍停止整个物理步进，不覆盖角度、速度或关节参数，也不在恢复时强制灌入速度。该缓解可继续游玩，但不能替代引擎对动态体几何范围的维护。
