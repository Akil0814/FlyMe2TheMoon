# 请求：独立指定刚体质量与转动惯量

2026-10-24；非阻塞，还原精度改进。

原作 Seesaw.createInPhysicalWorld: 对明确 Mass 的 7 个转杆调用旧 Box2D SetMassData：只替换质量，保留按初始密度得到的转动惯量与质心。当前引擎 BodyDefinition 的 ExplicitMass 会将惯量随质量同比缩放，未提供独立惯量/质心设置接口。

已检查 BodyDefinition、PhysicsWorld 的注册、施力、关节及状态 API。建议提供可选的质量数据设置结构（质量、局部质心、转动惯量），由引擎完成单位转换和参数验证。预期改善这些转杆受推力时的角响应。

Game Layer 当前使用 ExplicitMass 保证质量正确，接受惯量差异并记录；默认未指定 Mass 的转杆已使用原密度 0.5，不受此差异影响。不直接调用 Box2D，不修改引擎，不以此停止其他开发。
