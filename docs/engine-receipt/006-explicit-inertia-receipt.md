# 独立质量属性对接回执（006）

请求日期：2026-10-24。交付日期：2026-10-25。对应 FlyMe2TheMoon 的 `docs/engine-support-requests/006-explicit-inertia.md`。本回执随 ElysiaEngine 保存，未修改游戏仓库。

## 交付能力与单位

`BodyDefinition::mass_properties` 使用 `MassProperties`，包含三个互斥选项：`DensityMass{}`、`ExplicitMass{mass}`、`BodyMassData{mass, local_center, rotational_inertia}`。默认按密度计算；指定总质量模式沿用原有同比缩放惯量的行为；完整质量数据独立指定三个属性。

质量为 kg；局部质心为相对刚体原点的 EU；惯量为绕质心的 kg·EU²。引擎内部将长度除以 `units_per_meter`、惯量除以其平方，游戏无需调用 Box2D。旧版质量数据若采用绕局部原点的惯量，必须先按 `I_center = I_origin - mass * |local_center|²` 转换，不能仅根据同名字段直接复制。

创建时使用上述配置。运行时 `PhysicsWorld::set_mass_properties(handle, properties)` 完整替换配置；`mass_properties(handle)` 读取已提交配置，`body_state(handle)` 读取实际质量、有效惯量、局部和世界质心。`PhysicsParticipant` 提供同名配置快捷接口。传入 `DensityMass{}` 恢复密度计算；待创建或失效身份的查询返回空。

仅 Dynamic 允许覆盖。覆盖质量有限且为正，质心有限，惯量有限且非负，换算结果及倒数必须可表示。零惯量不响应力矩；固定旋转的有效惯量始终为零，同时保留请求配置。输入非法时创建返回无效句柄、修改返回 false，原状态保持。

## 接入与迁移

原 `mass_policy + mass` 字段已移除：原密度策略改用 `DensityMass{}`，原指定总质量改用 `ExplicitMass{原质量}`。需要保留密度惯量时使用完整数据，不能继续依靠 `ExplicitMass`。

以下代码可编译；创建配置针对本次已核对的居中转杆。运行时函数保留读取时的有效惯量，不负责恢复已经被旧指定质量模式缩放掉的密度惯量。

```cpp
#include "engine/physics/contracts/physics_participant.h"
namespace mass_receipt_example {
namespace ph = elysia::physics;
ph::BodyDefinition create_definition() {
    ph::BodyDefinition body;
    body.fixed_rotation = false;
    body.mass_properties = ph::BodyMassData{0.5f, {}, 5466.666667f};
    return body;
}
bool change_mass(ph::PhysicsWorld& world, ph::PhysicsObjectHandle handle) {
    auto state = world.body_state(handle);
    if (!state) return false;
    return world.set_mass_properties(handle,
        ph::BodyMassData{0.5f, state->local_center, state->rotational_inertia});
}
bool restore_density(ph::PhysicsParticipant& object) {
    return object.set_mass_properties(ph::DensityMass{});
}
}
```

`256 × 8 EU`、`32 EU/m`、密度 `0.5 kg/m²` 的矩形具有密度质量 `1 kg`，绕质心惯量约 `5466.67 kg·EU²`。以 `BodyMassData{0.5f, {}, 5466.666667f}` 创建可保留该惯量；`ExplicitMass{0.5f}` 则得到约 `2733.33 kg·EU²`。

| 游戏关卡 / 对象 ID | 指定质量 | 当前 ExplicitMass 的惯量缩放 | 游戏侧建议 |
| --- | --- | --- | --- |
| 25 / 0 | 0.5 kg | 0.5 | 接入完整质量数据以保留密度惯量 |
| 30 / 1 | 0.5 kg | 0.5 | 接入完整质量数据以保留密度惯量 |
| 42 / 0、1、6、9、10 | 1 kg | 1 | 本项数值无差异，仍需迁移配置字段 |

七个对象均为类型 110。当前游戏碰撞形状居中，局部质心为零；偏置的 `Anchor` 是关节局部锚点，不会把质心移到支点。其他未指定 Mass 的转杆继续使用 `DensityMass{}`。引擎不硬编码关卡、类型编号或转杆尺寸。

## 执行、生命周期与后端修正

配置在碰撞体替换、密度或质量贡献变化、启停后重新应用。修改不重建刚体、碰撞体或关节，身份和局部关节锚点保持。改变质心时保留原点位姿、角速度和原点瞬时速度，通过调整质心线速度实现；不承诺动量或能量守恒。已启用刚体被唤醒，禁用刚体保持禁用。

回调中的配置按值复制进入 FIFO；true 表示校验通过并接受，查询在执行前仍返回旧配置。执行时再次检查身份和当时形状；已移除、Reset 或不再满足校验的请求跳过。队列分配异常向上传播，入队失败不提前提交配置。

保留内置 Box2D 3.1.1。本轮仅在原生质量设置路径共享并刷新按有效质心计算的尺寸缓存，用于 CCD 和休眠；原生 ID 保持私有。已知的禁用关节岛断言不属于本次修复。

## 自动验证与验收边界

实施前已在三配置运行完整 CTest 并保存基线。三配置全量构建及双语文档示例编译通过；物理测试全部通过，完整套件没有新增失败。

| 配置 | 构建及双语示例 | 物理测试 | 完整 CTest |
| --- | --- | --- | --- |
| Debug / ImGui ON | 通过 | 69/69 | 186/189 |
| Release / ImGui ON | 通过 | 69/69 | 186/189 |
| Release / ImGui OFF | 通过 | 69/69 | 183/187 |

新增测试覆盖三种策略、独立冲量／力矩公式、非零质心、原点速度连续、固定旋转、零惯量、碰撞体变化后的重应用、唤醒、禁用状态、FIFO、延迟创建、移除、Reset、异常清理、逐分配点故障及偏心 CCD；独立原生测试验证质量设置会刷新尺寸缓存。Inspector 使用拥有数据的质量快照，包含无碰撞体对象。

基线已有 `demo_scene_tests`、`engine_i18n_assets_tests`、`project_i18n_assets_tests` 失败；无 ImGui 另有 `application_startup_exception_mid` 失败。本次不修改这些无关行为或放宽断言。

原作质量覆盖行为依据游戏仓库的研究记录，本次未独立复核原二进制。未执行 FlyMe2TheMoon 接入、游戏关卡实跑或实际窗口手感验收；引擎公式测试不代表原作轨迹完全一致。游戏侧接入后应检查第 25、30 关的推力及碰撞角响应。

## 相关文档

- [物理指南](../user-guide/systems/physics.md)
- [单位、命令与状态](../reference/physics/units-and-state.md)
- [物理接口总览](../reference/physics/README.md)

验证证据保存在仓库构建输出：`out/mass-properties-baseline.md`、`out/mass-sdl3-*-physics.log`、`out/mass-sdl3-*-ctest.log` 及对应构建日志。文档对齐检查 87 中文 / 87 英文页、链接检查 176 Markdown 文件均为零错误，`git diff --check` 通过。Debug 曾遇到旧链接 PDB 容量错误，清理生成数据库并重链后通过，未关闭调试信息。
