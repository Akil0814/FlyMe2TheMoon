# 请求：独立指定刚体质量与转动惯量

2026-10-24；非阻塞，还原精度改进。

原作 Seesaw.createInPhysicalWorld: 对明确 Mass 的 7 个转杆调用旧 Box2D SetMassData：只替换质量，保留按初始密度得到的转动惯量与质心。当前引擎 BodyDefinition 的 ExplicitMass 会将惯量随质量同比缩放，未提供独立惯量/质心设置接口。

已检查 BodyDefinition、PhysicsWorld 的注册、施力、关节及状态 API。建议提供可选的质量数据设置结构（质量、局部质心、转动惯量），由引擎完成单位转换和参数验证。预期改善这些转杆受推力时的角响应。

Game Layer 当前使用 ExplicitMass 保证质量正确，接受惯量差异并记录；默认未指定 Mass 的转杆已使用原密度 0.5，不受此差异影响。不直接调用 Box2D，不修改引擎，不以此停止其他开发。

## 2026-10-25 对接进展

用户提供提交 `df28f9eb6ee5a82481ea39e03323f08c99941835` 和[更新回执](../engine-receipt/006-explicit-inertia-receipt.md)。引擎已交付 `MassProperties`，旧 `mass_policy` / `mass` 字段移除。游戏层已迁移玩家和普通弹的显式质量；七根带 Mass 的转杆改用 `BodyMassData`，保留居中矩形按密度得到的惯量。第 25、30 关的 256×8 转杆质量为 0.5 kg、惯量为 5466.67 kg·EU²；第 42 关五根的质量为 1 kg，惯量相同。关节锚点不作为质心。

当前状态：接口请求已完成并接入。`build-mass-receipt01.log` 构建成功，`mass-receipt-ctest01.log` 5/5 通过；真实 PhysicsWorld 测试覆盖全部七根转杆的质量、居中质心、5466.67 kg·EU² 惯量及独立线性/角冲量响应。第 25/30/42 关运行采样也读到一致质量和惯量；首关三星及第 13 关通关回归通过。

关卡验收仍独立进行：第 25 关短程飞行通过；第 42 关最初因激光出生死亡失败，修正游戏层炮口/炮座及载体自碰撞后短程飞行通过；第 30 关仍出生即被激光命中，不能算游玩通过，也不将其归因于质量接口。原失败证据保留在 test-evidence/history。
