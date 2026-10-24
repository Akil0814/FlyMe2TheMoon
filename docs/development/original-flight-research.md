# 原版飞行与角色几何核对

2026-10-24。只读取本地提供的 `Fly2theMoon` Mach-O 文件，未运行该程序。分析工具为 `game/tools/inspect_original.py`，输出在 `out/analysis/`。ARMv6 分片保留 Objective-C 方法元数据且未加密；Capstone 只用于离线反汇编，不是游戏依赖。

## 已核实的计算

- `RoleBodyComponent.applyPush:pushPoint:`（0xCBD00）：触点与刚体位置的差向量经角度限制、指数映射、角度衰减和距离衰减生成推力。原版 JSON 给出角度范围 50°、指数 1.6、衰减角 90°～120°、距离 51.2～64。
- 最大推力 28；若推力与速度的内积大于最大功率 51，按 `51 / dot(force, velocity)` 缩小推力。反向制动不受这个正功率上限限制。游戏使用每米 32 个世界单位，所以力乘 32、功率乘 1024。
- `applyPush:pushAngle:` 的显式方向分支使用最大推力的 0.8 倍，再执行功率限制。桌面键盘是新增输入方式，采用该分支的力幅度；键位映射不宣称属于 iPhone 原作。
- `update:isPushing:`：横向阻尼按 `clamp(1-dt*damping,0,1)` 相乘；未施力时再应用 extraNonFlyXDamping。maxXSpeed 为 5 m/s。未发现先前近似使用的 420 EU/s 竖直限速，已移除。
- `Game.loadMapHeaderFromXML:`（0xA50AC）的 BornPoint 分支在读取 XML 后分别加 X=25、Y=16，再传给 `Role.loadRole:`、`RoleModel.load:` 和刚体创建。后两个调用不再加偏移。另一种旧 BornPosition 分支不加偏移，本次 60 关使用 BornPoint。
- `RoleView.getCenterOffsetFromAnchor` 返回原坐标 `(0,13)`；转换到 Y 向下坐标后是 `(0,-13)`。
- `RoleBodyShaper.createBodyFixturesForRoleBody:`（0xCD27C）创建底部圆形（半径 0.56 m）、上身八边形和八边形传感器。上身轮廓由底边 ±0.5 m 和以 Y=0.90625 m 为中心的半径 0.5 m 半圆六个点组成。碰撞形状已通过引擎公开 API 表达；不再以单个半径 14 的圆近似全身。

反汇编含指令后的常量池，不能把常量池误当成逻辑。工具的寄存器注释只作辅助，结论同时核对实际 load/store、调用参数和 JSON 数值。物理积分器为新版 Box2D，与原版历史版本仍可能有小幅差异；整关手感仍须实际回归。

## 实际验证

- `build-engine-update01.log`：新版物理接口适配 Release 构建成功。
- `engine-update-ctest01.log`：4 项 CTest 通过。
- `build-original-flight01.log`、`original-flight-ctest01.log`：原版角度、距离、功率和阻尼计算构建及单测通过。
- `level1-original-physics01.json`：原始教程输入播放、恢复玩家操作、真实键盘飞行收集 3 星并通关，进程退出 0。未直接设置成功、位置或收集计数。
- 原先 `Central touch cone must remain vertical` 测试断言来自未经核实的近似。依据原程序实际指数映射，替换成竖直、微小偏角、对称性、触点距离和功率边界检查。旧证据保留，不修改成新算法已通过。
- 新版速度较慢，自动输入脚本将玩家按住 Space 的窗口从 4 秒延长到 13 秒以覆盖飞行与末尾对话等待；3 星、成功、关卡编号及输入状态断言不变。实际到达月亮发生在按键约 5 秒后。

## 引擎更新边界

用户在本轮明确告知引擎物理系统已更新。原始基线 636 文件保留；更新后的只读基线为 `test-evidence/protected-files-engine-update-baseline.csv`（641 文件），差异记录于 `engine-update-diff.json`。这批变化属于接收到的引擎更新，不是 Game Layer 修改。后续校验对新基线执行，并在最终报告中保留两次基线的来源区别。

Game Layer 适配了 BoxShape、CollisionQueryFilter、overlap_shape 和关节 settings；地形使用 SegmentShape，移动平台和载体使用 set_target_transform，弹丸使用圆形 shape_cast_all 并排除自身。引擎目录和原有文档只读。

## 背景颜色与鼠标整关回放

`BackgroundLayer.load:` 在 0xA71C8 起按 Suite 选择渐变色；0xA7644～0xA764C 明确跳过 SeqID=1，不请求该图片。故此前 60 个 Background_001 缺图记录已更正，`import-gradient.log` 的 unresolved_decor 为空。四套起止颜色由指令中的字节值和打包 RGBA 常量还原，原始 CCLayerGradient 默认方向为 `(0,-1)`。游戏当前使用公开矩形命令绘制渐变带；原版 0.1 视差与渐变空间关系尚未完全复现，记录为近似。

`level1-mouse-pilot01` 未通过：测试路线的最终点 Y=440 在月亮触发范围外，控制器到点悬停，已经收集 3 星但未碰到月亮。将最终目标改为实际月亮附近 Y=385 后，`level1-mouse-pilot02` 正常鼠标输入三星通关，退出 0；没有更改月亮碰撞范围或成功断言。

`level2-route01` 已完成全关鼠标飞行、触碰月亮与正常退出（22.987 秒关内计时，0 星、14 水晶分值）。这是通关测试，不是全收集测试。

路线工具 `plan_test_route.py` 从静态轮廓生成候选航点。`FlightPilot` 只读取测试采样状态并发送 SDL 鼠标/Enter 事件，不持有 Scene、PhysicsWorld、存档或剧情接口；每次运行仍由实际游戏机制决定成败。规划成功本身不算测试通过。新路线允许成功后停留 2 秒再保存证据和退出；所有成功断言照常执行，失败仍在时限后保留完整轨迹。


## 评分与炸弹补充

- `ScoreUtils.scoreByLevelID:nStars:time:nCrystalLarge:nCrystalSmall:`（0x9A574）：关卡索引限到 1..30 读取 svrs；令 r=(stars+1)*svr/time，r>1 时替换为 2-1/r。最终截断为整数 `(200*r + 100*stars + 15*large + 3*small)*100`。四个 double 常量读取验证为 1、-1、2、200；hFactor 返回 float 100。这里保留原程序的索引限制，不擅自推断 31..60 的另一套规则。
- 已用 `original_score` 替换临时计分，慢速/快速曲线、大小水晶等价权重、零时限、索引限制及存档迁移测试通过（original-score-ctest.log）。旧开发版近似分数迁入 legacy_scores 保留，不与原式成绩比较；星数、关卡解锁、最佳时间、剧情标记和已获得成就保留。
- `Bomb.createInPhysicalWorld:` 的触发圆半径 2 m、伤害圆半径 1.25 m；`initWithXML:` 默认 Alarm=1 秒、Power=3；`explode` 在爆炸开始只检测一次伤害，视觉结束不持续伤害。类型 116 的 27 帧（60 Hz）缩放/透明度表位于 0x1A04B8。已替换此前 80 单位距离触发、默认 1.5 秒及持续 0.5 秒伤害的推断。
- `Role.hitByAmount:` 在 manaEnabled=false 时立即销毁身体并切换死亡动画，所以本批 NoMana 关卡采用一次命中死亡有直接证据。
- 第 3/4/5 关鼠标路线均实际通关（各自 route01 日志）；第 4 关渐变背景截图已检查。炸弹命中原因与爆炸计数现在单独采样，后续实机测试须断言具体来源。
