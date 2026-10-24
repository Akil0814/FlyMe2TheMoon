# 物理单位、命令与状态快照

本文说明当前 Box2D 后端的公开约定。游戏层应通过引擎命令修改物理状态，再通过快照读取已提交结果。

## 1. 空间单位与方向

每个世界使用一个不可变的 `units_per_meter` 比例，默认 100。EU 表示引擎世界单位；时间为秒，角度为弧度，质量为千克。`SurfaceDensity` 表示 kg/m²，不另设独立的单位倍率。

设 `S = units_per_meter`，输入到 Box2D 时按下表转换：

| 量 | 公开单位 | 输入换算 |
| --- | --- | --- |
| 位置、长度 | EU | 除以 S |
| 速度 | EU/s | 除以 S |
| 加速度、重力 | EU/s² | 除以 S |
| 力 | kg·EU/s² | 除以 S |
| 线性冲量 | kg·EU/s | 除以 S |
| 力矩 | kg·EU²/s² | 除以 S² |
| 角冲量 | kg·EU²/s | 除以 S² |
| 绕质心转动惯量 | kg·EU² | 除以 S² |
| 角度、角速度 | rad、rad/s | 不变 |
| 时间、质量、密度 | s、kg、kg/m² | 不变 |

输出使用反向转换。关节锚点、反作用力和马达力矩遵循同一张表。弹簧频率为 Hz，阻尼比、摩擦和恢复系数无量纲；线性与角阻尼系数的单位是秒的倒数。

坐标保持 X 向右、Y 向下，正旋转、角速度和力矩在屏幕上表现为顺时针。适配层不翻转 Y 轴；需要角度制的渲染接口只在绘制边界转换。

## 2. 创建、修改与读取

`PhysicsWorld::register_object(owner, body_definition, collider_definitions)` 复制定义并返回稳定的引擎句柄。`collider_id(handle, index)` 查询形状身份；创建定义本身不包含存活对象的标识。

`PhysicsParticipant` 提供创建描述并接收世界与句柄绑定，不公开可写模拟状态。`PhysicsStepParticipant::fixed_update` 在每个模拟步提供控制。

修改状态使用 `set_velocity`、`set_angular_velocity`、`apply_force`、`apply_impulse`、`apply_torque`、`apply_angular_impulse`、`set_transform` 或 `teleport_object`。力与冲量的可选作用点为世界坐标，省略时作用于质心。直接改写已注册 `GameObject` 的位置不等于提交物理命令。

`body_state`、`render_pose` 和 `joint_state` 返回快照，无效句柄返回空 `optional`。游戏规则使用已提交状态，绘制使用插值姿态。`update_collider` 替换复制的形状描述，`set_collider_enabled` 只改变启用状态；相同描述不会重建形状。

## 3. 质量与关节

`FromDensity` 根据以米计的形状面积求质量。`ExplicitMass` 保留形状计算的质心，并按请求的总质量缩放转动惯量。传感器默认不贡献质量，只有显式启用时才参与；现有角色使用显式质量和固定旋转。

距离关节提供刚性静止长度，或带频率与阻尼比的弹簧。旋转关节提供局部锚点、参考角、角度限位、马达速度和最大力矩；相连刚体默认不互相碰撞。原生约束刚度有限，极大马达力矩可能越过名义角度限位。

摩擦、接触、休眠与 CCD 使用 Box2D 行为，不保证保留旧后端的数值轨迹。

## 4. 调试绘制

默认碰撞体轮廓与普通绘制使用相同的插值位置和最短角度旋转。绿色表示清醒刚体，蓝色表示休眠或静态刚体，紫色表示传感器；禁用刚体不显示。关节锚点也使用插值姿态。

Inspector 的 `Native AABB (physics step)` 单独显示黄色轴对齐原生边界。它不是旋转碰撞体轮廓，也不是 CCD 扫掠路径。`Previous / current physics poses` 用灰色显示前一姿态、白色显示最新模拟姿态；两项默认关闭。

接触点、法线和速度矢量表示最近完成的物理步，刻意不做插值。延迟修改提交后或调试模式变化时会刷新捕获，暂停时也一样。没有物理步的渲染帧仍更新碰撞体的插值系数。

## 地形与目标命令

Chain 顶点、ghost 与 Segment 端点也使用 EU，正面为局部 `(dy,-dx)` 随刚体旋转。Chain 子段不可通过 Collider 修改，身份由 `chain_collider` 与 `chain_segment_info` 查询。`set_target_transform(handle, pose, dt)` 仅针对运动学刚体，每步提交一次；不提交时保留当前速度，同目标或原生休眠阈值内目标清除旧速度。

八类 `JointState.details` 提供类型化设置与测量，公共 `first`、`second` 标识两端；Filter 无设置，反作用力／扭矩为零。Motor 的位置目标在第一端局部坐标中，Mouse 的目标在世界坐标中，二者误差按各自坐标表达。

Filter 与 `collide_connected` 遵循内置 Box2D 的实体接触规则；它们不屏蔽原生 Sensor 重叠，也不改变无状态查询。需要排除传感器或查询目标时，使用 Collider 过滤器或 `CollisionQueryFilter`。
