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


## 旋转杆参数补充（2026-10-24）

原 `Seesaw.initWithXML:`（0xbe808）提供默认 Anchor=0.5、Speed=0、Torque=100、Mass=-1（不覆盖）、AngularDamping=1.6。`initExtraInfo...`（0xbe5e8）跳转表将 108/109/110/117 的物理宽度设为 64/128/256/512，支点比例限制使其距杆端至少 4 单位。`createInPhysicalWorld:`（0xbdaf0）设置半厚 0.125 m（4 EU）、密度 0.5、摩擦 0.8。

原刚体角是显示旋转角的负值，Speed 直接送给原 Y-up Box2D，故转为引擎 Y-down 后电机速度必须取反。此前方向直接沿用是实现错误，已修正 Game Layer。实体物理碰撞筛选也改为允许转杆与地形及实体阻挡，避免杆穿过环境。原 Stable 仅出现在 `update:` 的音效角位移计算，不能据此把杆固定或关闭转动。

原 Mass>0 分支保留旧转动惯量只改质量；当前引擎 ExplicitMass 会按质量缩放惯量，此处仍有差异（明确 Mass 的 7 个对象）。未改引擎。对默认质量改用密度计算。

证据：`out/analysis/seesaw-xml.log`、`seesaw-physics.log`、`seesaw-extra-tail.log`。跳转表字节不是指令，另从有效代码地址续读，未执行原版程序。


## 风场方向与采样修正（2026-10-24）

`Wind.initExtraInfoWithStrength:numBlocks:`（0xc9c64）明确 ori=(cos(-(Rotation+180)°),sin(-(Rotation+180)°))，因此 Y-down 下零角向左、90°向上；此前按零角向上是错误推断。`initWithXML:` 的 Strength 默认值是 20。`createInPhysicalWorld:`（0xc880c）宽度为 64×NumBlocks，中心相对第一格向左偏移 (1−NumBlocks)×32；此前向右扩展错误。高度 8/24/25 分别为 64/128/256。

`RoleBodyComponent.getEnvelopeAABB` 普通角色包络为 32×61 EU，在 Y-down 体锚点的 x±16、y∈[-45,16]。`Wind.getIntersectRateWithRole:` 在横向三等分的两个内部列、纵向四等分的三个内部行取 6 点，每个在风场内贡献 1/6。`Wind.update:` 直接以此比例乘 strength×ori 施力，Fade 只用于边缘贴图，不是原先推断的 32 单位力渐变。

修改 `wind_field.h`、风场中心和方向表现；更新原先建立在错误推断上的测试，保留范围外零力、旋转与强度检验，增加六点半覆盖和默认强度检验，不降低通关断言。完整叶片/流动素材表现仍为近似线条。

## 激光与移动尖刺校准（2026-10-24，待本轮实跑）

原程序静态证据 `LaserBeam.initWithXML:`（0xb7a60）、`update:`（0xb59e4）及 `updateOrientByVectorToRole:dt:`（0xb46a8）确认：
- 激光默认 ActiveRange 为 1000，-1 表示无限激活距离；射线长度为 1000 物理米，即 32000 游戏单位。
- FixedAngle 优先；存在 Cycle 时自动扫描，Rate 默认 1；两者都没有时跟踪角色身体锚点，局部角度限于 0..π，以 min(5×dt,1) 平滑。
- Lag 仅延迟自动转向，不延迟发光。当前 Game Layer 已修正这些分支，发射口的 24 单位偏移和光束表现仍为近似。
- `DynamicBox.initExtraInfoWithMovement:...`（0xb3930..0xb39dc）逐项读取 Spikes[0..3] 到 hasSpikeE/N/W/S，确认字符串顺序为右、上、左、下。此前错误采用上、右、下、左，已修正。第 8 关 wind02 之前的失败不能证明修正后难度或通路。

证据保存在 `out/analysis/laser-{xml,update,aim,auto,start,sprites}.log` 与 `out/analysis/platform-extra.log`。以上均为本地只读反汇编，没有运行参考可执行文件。新版构建与关卡验证结果另见增量验证记录。

## 杆体、平台与追踪花贴图（2026-10-25）

本轮继续只读分析原程序，并逐张核对拆分 PNG，确认目录中的编号图片不能统一视为 12 FPS 循环动画：

- `DynamicBox.initSprites`（0xb3c40）选普通底图或 Lock 底图；`initLightEffect`（0xb3b00）另建高亮层。105–107 的普通、锁定、高亮花纹不应逐帧轮播。当前固定选择普通/锁定底图；原版基于约束受力的高亮透明度尚未重建。
- `Seesaw.initSprites`（0xbe274）独立创建 `S01/001.png` 轴心，依据锚点是否靠近杆端选择杆体第二张图，右端锚点还需水平翻转。轴心跟随同一刚体位姿绘制，不能与杆体交替显示。本轮同时统一杆体、轴心和尖刺的插值显示位姿。
- `MissleLauncher.initSprites`（0xb96b8）为 113 型追踪花创建右花瓣（根目录第 2 张）和左花瓣（第 3 张），叠加在花茎（第 1 张）之上。花瓣使用左下锚点，原始局部坐标为 Y 向上；绘制次序为花茎、左瓣、右瓣。
- `MissleLauncher.updateAnimation:`（0xb8500）使用两张 60 项位置/旋转表，地址分别为 0x1a0dfc、0x1a10cc；发射后重置计时，结束后停留闭合状态。本轮以 60 Hz 采样这些表，`game/tools/import_launcher_visual.py` 可重新生成 `game/gameplay/launcher_visual.h`，不执行参考程序。发射时序、追踪与伤害判定保持原有游戏逻辑。

验证：新增平台状态、四种杆长/三类锚点、杆体与轴心插值一致性、追踪花三层持续显示及发射开合检查，连同既有拾取物/子弹检查覆盖连续 180 帧；5 项 CTest 通过。`out/qa/mechanism-visual-preview.bmp` 为实际 SDL 软件渲染预览，已查看普通/锁定花纹、杆体轴心以及闭合、开合、旋转花朵；该预览不替代全部关卡实机验收。

## 普通弹重力（2026-10-25）

`MissleLauncherMissle.createInPhysicalWorld:`（0xbab94）的刚体定义使用动态类型；该方法末尾（0xbb058）明确把弹体质量设为 0.1875 kg。此前游戏把普通弹和追踪弹都注册为无重力运动学刚体，导致普通弹沿初速度直线运动。

本轮将普通弹改为有重力的动态刚体，显式指定 0.1875 kg，并保留圆形扫掠与命中销毁逻辑。显式质量必不可少：当前弹体仅包含不贡献质量的传感器，自动质量为零；仓库 Box2D 的速度积分在逆质量为零时不施加重力。扫掠预判加入半步加速度位移，实际运动仍由 PhysicsWorld 积分。追踪弹保留项目现有运动学、无重力和转向控制；本次不代表全部原版发射/追踪参数已完成校准。

回归使用真实 PhysicsWorld：水平普通弹在 g=320 下的一秒速度/位移、向上发射后到达顶点并落回、下落时命中薄地形并销毁，以及追踪弹不受世界重力影响。修复前“普通弹应下落”的断言失败；仅改动态类型时进一步复现质量为零的问题；补充显式质量后上述检查通过。

## 传送门圆环绘制（2026-10-25）

`DynamicObject_115/001.png` 的 Alpha 全为零，只是透明底图。`Portal.initSprites`（0xbceac）从 S01（入口）或 S02（出口）各取两张纹理，分别创建 12 个子精灵，总计 24 个；不是把两张图片作为交替播放的帧。`initWithXML:`（0xbd2d4）以是否存在 TargetPortalID 区分入口与出口。

`refreshV2SpritesByIdxFrame`（0xbc6e0）逐个应用位置、横纵缩放、旋转与透明度，入口表地址为 0x1a6e78，出口为 0x1a1478；每帧包含 24 组 24 字节记录。`updateAnimation:`（0xbc858）以 60 Hz 采样 40 帧，周期 2/3 秒。位置表以 Y-up 表示，子精灵 Y 还需加 64；游戏绘制转换成以门中心为原点的 Y-down 坐标。

本轮通过 `game/tools/import_portal_visual.py` 只读提取上述表到 `game/gameplay/portal_visual.h`，在 Game Layer 直接提交 24 个纹理绘制命令，恢复黄绿色入口和紫色出口的动态环形效果。原程序无需执行；该圆环使用贴图组合，当前引擎公开渲染 API 足以实现。传送过程中的角色吸入/吐出和镜头过渡效果不在本次恢复范围。

验证：入口/出口各连续 180 帧检查部件数量、两组纹理、四向环绕与持续可见性、动画变化、40 帧循环及整体旋转；5 项 CTest 全部通过。`out/qa/portal-visual-preview.bmp` 为实际 SDL 渲染的两种门各四个时刻，已查看。

新版主程序第 13 关输入回放 `level13-portal-visual01` 通过，观测到一次传送并正常退出；结果与截图保存在 `out/qa/`。

## 爆炸物常态组合贴图（2026-10-25）

116 型爆炸物的根贴图同样全透明。`Bomb.initSprites`（0xb018c）用 `S02/001.png` 创建六个独立花瓣，用 `S01/001.png` 创建上层中央图案；两者在原版中均以精灵默认中心锚点放在物体局部中心附近。S01 第二张是中央高亮层，S03 两张分别为预警整体花瓣及其高亮层，不能用透明根图代替触发前的部件。

`refreshV2SleepLeafPosByIdxFrame`（0xaf2ec）使用 0x19e8e0 的 90×6 组位置/角度数据；`updateSleepAnimation:`（0xaf414）以 60 Hz、90 帧循环更新，形成轻微呼吸动画。本轮新增只读导入工具 `game/tools/import_bomb_visual.py` 和生成表 `game/gameplay/bomb_visual.h`，常态持续绘制六瓣及中央图案，并随物体整体旋转。触发、爆炸伤害和现有预警/爆炸绘制分支保持原有逻辑；本轮没有完成预警高亮层的原表校准。

验证：常态连续 180 帧检查七个部件的纹理、透明度、尺寸及位置，检查呼吸变化、90 帧循环和整体旋转；绑定真实 PhysicsWorld 后由重叠对象触发，验证预警、爆炸、消失阶段不残留常态花瓣。5 项 CTest 全部通过，主程序构建成功。`out/qa/bomb-idle-visual-preview.bmp` 为实际 SDL 渲染预览，已查看。

新版主程序第 31 关输入回放 `level31-bomb-idle-visual01` 通过，观测到爆炸物进入预警状态并正常退出；结果与截图保存在 `out/qa/`。

## 风场流动与叶片素材（2026-10-25）

008、024、025 的根目录首张图均全透明，后两张分别为 768×256 像素气流纹理和 23×20 像素叶片。此前 `Entity::submit_render_commands` 提前返回近似风线，实际没有使用这些图。本轮接入独立 `WindVisual`，保留既有风力、角色覆盖采样和物理区域逻辑。

只读核对 `Wind.initSprites`（0xc9ad8）、`initFlowEffectSprites`（0xc8be0）、`initFadeSprites:`（0xc8fe0）、`initLeafSprites`（0xc93e4）及对应更新方法：

- 008 高 64、024 高 128；025 用两行 128 高的裁剪窗口组成 256 高区域。各块宽 64，向局部左侧延展。纹理按原始半尺寸裁剪，不将整张图拉伸铺满风区；流动源窗口在 384−64=320 单位范围循环。
- `getFrameRate`（0xc758c）返回 `120×clamp(Strength/20,0.25,2)`，每周期 60 步；`getWindSpeedForLeafs`（0xc7604）返回 `320×clamp(Strength/20,0.25,2)`。`getOpacity`（0xc767c）把强度比限于 0.5..1，透明度约为 `255×(0.236+0.764×ratio)`。
- Fade[0] 控制局部右端流入边缘，Fade[1] 控制局部左端流出边缘。每端用十二条宽 2 的条带，跨越物理边界内外各 12 单位，按原版对称 1.6 次幂曲线渐隐；核心流动窗口相应缩短，Fade 不改变施力。
- 叶片数为 `floor(NumBlocks/2)×倍率`，008/024/025 倍率为 1/2/3。`WindLeaf.startRandomly`（0xca290）与 `update:`（0xca5c0）确认左移、随机起始等待、旋转、垂直回复加速度、16 单位旋转扰动及翻转压缩效果；两端 48 单位范围淡入淡出。游戏沿用这些参数，按对象 ID 初始化独立随机数生成器以便测试复现，不声称逐次匹配原程序全局 rand 序列。

回归覆盖三类高度、1/2/4 块、四种 Fade 组合连续 180 帧，验证实际纹理、部件数、源窗口范围和裁剪比例、滚动与叶片持续更新、90°整体旋转和 Strength 对速度/透明度的影响。风场先以原素材分辨率合成为透明纹理，再整体投影，避免分块目标矩形各自取整而产生斜向/缩放接缝；合成使用预乘透明度，并恢复渲染目标、绘制颜色和素材状态。未修改引擎渲染器。

加速后端使用上述合成路径；SDL 软件后端的离屏 Alpha 存储与加速后端存在差异，保留逐部件绘制作为兼容路径。软件预览使用游戏当前的 1 倍缩放；合成路径的旋转接缝与透明度以 Direct3D12 主程序截图验收，不将软件预览视为两条路径逐像素一致的证明。


## 质量回执、炮口与载体碰撞（2026-10-25）

用户提交 df28f9e 与回执 006 提供独立 BodyMassData；游戏已迁移全部质量字段。七根明确 Mass 的转杆保留初始密度惯量的差异现已解决，详见支持请求 006 与增量验证记录。

`LaserBeam.initSprites` 0xb728c～0xb72f8 从 ARM 地址 0x1f3078 读取 61 项局部 Y-up 炮口点；索引按 `(180 - orientDegrees) / 3` 截断并限于 0～60。`updateLaserStartVec` 使用节点变换得到世界起点，而不是从图片中心沿方向偏移 24。`createShape` 0xb6190 建立相对 (21,41.5) 锚点的五边形，顶点物理米为 (0.25,-0.9)、(0,-0.1)、(-0.25,-0.9)、(-0.25,-1)、(0.25,-1)。游戏保持已有图片中心坐标，在局部换算后提交引擎多边形。导入脚本 `import_laser_geometry.py` 只读取参考二进制，不执行它。

原 `createInCarrier` 将炮座形状装在载体刚体上。游戏的独立运动学附属实体曾与载体互撞，导致第 30 关电动转杆停转；现以每个载体独立的负碰撞组排除本体和兄弟实体，仍保留玩家/环境碰撞。运动学目标方式仍有跟随步长和接触反作用方面的近似，未宣称完全等价于复合刚体。

`RoleModel.load:` 0xce494～0xce4a4 和 `loadRoleState` 0xce384～0xce390 明确将 timeImmune 清零，不能据此添加出生无敌。激光 ray callback 0xb802c 忽略传感器并记录最近命中，未见角色过滤例外。第 42 关短程出生/飞行检查修复后通过；第 30 关仍出生激光死亡，继续调查。

## 墙体、前景与远景的镜像锚点（2026-10-25）

本轮逐项核对全部 60 关、17,727 个装饰对象的资源组、SeqID、位置、旋转与水平镜像标记，当前资源编号绑定与原始 XML 一致。视觉墙体与碰撞轮廓错位的明确原因是镜像变换：`TextureLayer.load:`（0xac268）和 `ForegroundLayer.load:`（0xa8ed8）先设置左下锚点 (0,0)，再以 `setScaleX:-1` / `setScaleY:-1` 翻转，并非仅在原矩形内部改变纹理采样方向。

旧版 `Decor` / `Parallax` 只传 SDL 水平翻转，矩形仍向锚点右侧延伸，导致镜像墙体落在错误一侧。垂直翻转也未导入。本轮新增 `decor_transform.h`，水平镜像时矩形左移一个图宽并使用右侧旋转锚点，垂直镜像时矩形下移一个图高并使用顶部锚点；组合镜像、旋转和视差坐标转换共用同一规则。碰撞数据及物理对象没有按显示错误重新挪动。

原始装饰共 1,013 处水平镜像（含远景 11 处）和 348 处垂直镜像。`import_decor_transforms.py` 仅补入已有装饰记录的垂直翻转/透明度元数据，49 份关卡 JSON 需要更新；所有现存资源键、坐标、角度以及装饰以外的数据保持原值。源 XML 的 240 个装饰层全部已按 Depth 升序排列，故当前关卡的深度顺序并非此次错位根因；标准导入器补充显式稳定排序，避免以后依赖 XML 的排列。

回归检查四种镜像组合与 0°、±90°、45°、180°旋转的四角坐标，独立以原作锚点加有符号局部坐标计算期望位置；同时遍历 60 关的地形和前景绘制命令，核对资源绑定、绘制顺序及锚点不变。`out/qa/decor-anchor-before-after.bmp` 左侧模拟旧变换、右侧为修复后的显示，青色线条为相同的原始碰撞轮廓。

额外将补充元数据后的 60 份 JSON 与当前 Git 基线逐项比较：除装饰的 flip_y/alpha 外完全一致。配套平台回归夹具不再依赖旧版 BodyDefinition 默认类型，明确把测试锚点和墙体声明为 Static；不以移动测试墙来验证平台被阻挡。

最终主程序构建与 5 项 CTest 全部通过。Direct3D12 第 10 关回放 `level10-decor-anchor04` 通过，观测到向上风力 -320 及角色上升，已查看前景/墙体和斜向风场的实机截图。结果保存在 `out/qa/`；这属于全关数据/变换检查及局部实跑，未宣称 60 关全部人工验收。

## 桌面键盘满推力适配（2026-10-25）

用户要求键盘也能达到满输入。`FlightScene::on_routed_input` 移除键盘方向向量的 0.8 倍率：Space/W/↑ 竖直方向幅度为 1，A/D/←/→ 的 ±50°方向幅度同样为 1。与鼠标满触点一致，最高可请求 28 N 推力；共享原有 51 W 正功率限制、角度范围和物理参数。此前文中“键盘沿用原显式方向分支 0.8”的描述是调整前状态；原分支系数仍是原作事实，但桌面键盘不再沿用该倍率。

主程序构建、`fly_mechanisms` 回归通过。第 6 关原生键盘回放 `keyboard-full-thrust02` 依次发送 Space/W/↑ 与 A/D，观测竖直输入达到 -1、左右输入达到 ±sin(50°)，所有非零键盘样本幅度为 1，回放断言通过并正常退出。第一次运行在资源加载阶段超时；第二次加载较慢但最终完成验证，日志和结果均保存在 `out/qa/`。

## 三类角色状态特效（2026-10-25）

已补充出生/复位展开、到达月亮收拢以及传送门吸入/镜头移动/出口展开。原方法、帧表、动作时长、缓动曲线与衔接策略的证据边界见 [role-transition-effects.md](role-transition-effects.md)。角色音效不包含在此次视觉补充内。

最终 Debug 主程序和五个测试目标构建通过；五项 CTest 全部通过（37.89 秒）。新增检查覆盖真实图集逐帧绘制、角色显隐、物理体关闭/启用与速度恢复、零距离传送、终点延迟边界和原 Lua 分支。软件效果总览 `out/qa/role-transition-effects.bmp` 已查看。

Direct3D12 实跑 `transition-portal02` 通过，使用普通 SDL 鼠标输入完成第 13 关首组传送。采样依次出现吸入、镜头移动、出口展开，三个阶段角色物理体均关闭、推力均为零，完成后物理体和入口速度恢复。第一次定时键盘方案 `transition-portal01` 未通过，因为短暂按键没有将角色送达入口；保留失败结果，改用持续跟踪路线的鼠标回放，未放宽传送断言。

Direct3D12 实跑 `transition-moon01` 通过：原首关教程演示到达月亮后完成动画、复位与后续对白，再由普通鼠标输入通关；最终 3 星、22 水晶分值，飞行时间约 14.855 秒。采样确认两次月亮过渡期间物理体关闭、结果面板尚未显示，随后正常进入结算；出生/复位特效至少执行两次。第 13 关完成后的画面与第 1 关结算画面已查看。

日志、原生断言结果、采样与截图保存在 `out/qa/transition-effects-audit/`；可复用的场景输入位于 `game/testing/role-transition-portal.json` 和 `role-transition-moon.json`。这属于三类视觉流程与两关局部验证，不代表全 60 关通关验收。

随后根据用户指出的月亮高亮遗漏，核实 `Moon.initLightEffect` 取第二张纹理并作为独立子图层，`updateAnimation:` 使用 0x1a139c 的 220 项透明度表。已补入到达时触发、渐亮、保持和关闭；`import_moon_visual.py` 生成原表，底图保持可见。主程序及测试目标重新构建通过，`fly_idle_visuals` 包含本次逐帧高亮/图层对齐/重播回归，通过（32.68 秒）；已查看 `out/qa/moon-highlight-preview.bmp`。前述第 1、13 关原生回放发生在此高亮补充之前。

## 普通子弹发射器的 S01 炮头（2026-10-25）

用户对照截图指出 `DynamicObject_112` 只有棕色底座。核实原 `Thrower.initSprites`（0xc14a0）单独创建 `DynamicObject_112/S01/001.png`：该组只有一张 75×77 图片，逻辑尺寸 37.5×38.5；锚点为原 Y-up 的 (0.5,0.344)。`getBaseCenterPoint`（0xc0058）给出底座图片坐标 (21.75,37.25)，换算到当前居中且 Y-down 的底座为 (0,-21)。炮头围绕此连接点旋转，绘制在底座之上。

`Thrower.updateAnimation:`（0xc0474）对炮头应用 ARM 地址 0x1ac8dc 的 20 项横纵缩放表，开火时 `throwThrown`（0xc00a0）重置计时；峰值为第 9 帧的 (1.11,0.85)，第 19 帧恢复 (1,1)。它通过变形单张贴图产生动作，不能把底座与 S01 当成轮播帧。`import_thrower_visual.py` 仅读取参考文件生成 `thrower_visual.h`。

游戏已增加独立炮头，恢复上述锚点与发射回弹；固定角使用 `FixedAngle`，自动瞄准的炮头使用原 `updateOrientByVec:dt:`（0xc0378）的前向半圆限制与 `min(5*dt,1)` 平滑。底座与炮头共享公开物理渲染姿态，载体运动时保持拼接。此次未调整子弹生成点、速度、发射调度或原有直接指向目标的弹道策略；自动瞄准的弹道与炮头平滑仍有已有策略差异，不宣称完整复原 Thrower 的全部物理行为。

新增视觉回归检查六种底座角度、真实 S01 绑定、连接点、发射峰值/恢复/重复触发、瞄准平滑，以及实际运动学物理体的显示插值。`out/qa/thrower-head-preview.bmp` 上排常态、下排开火回弹，展示向左、向上、向右三种朝向。

主程序及五个测试目标构建成功，五项 CTest 全部通过（103.99 秒），已查看上述软件绘制预览；本次未增加原生窗口逐关回放。
