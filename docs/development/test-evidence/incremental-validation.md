# 增量验证记录

以下均非最终验收。

## 2026-10-23 选关、存档、剧情与音频

- `game/tools/build-release.cmd`：Release 编译链接菜单、60 关选择、17 成就页面和引擎设置入口成功。`build-selection.log`。
- `ctest --test-dir out/build/release --output-on-failure`：进度/存档测试通过。覆盖 60 关依赖顺序、17 成就、非法/未来版本拒绝、磁盘写入、服务重启和损坏主档恢复；测试使用 `out/qa` 自有临时目录。
- 旋转错误修复：Cocos 数据角度顺时针，原导入误取负；改为正角度后第一关通道/角色/月亮不再被错误旋转植被遮挡。`rotation-retest.log`、`level1-flight.bmp`、`level1-flight.json`：实际输入通关，3 星、22 水晶分值。后续增加剧情后旧 6 秒回放不再适合验证完整流程，保留作为历史证据，新增完整教程回放。
- `story-ctest01.log`：首次失败。测试在 delay 恢复后立即按继续，没有记录刚出现的对白，且仅测试已完成章节分支。修正观察时机并增加未完成章节和第二次死亡分支，改为逐项匹配原脚本提取的全部对白键，未降低覆盖标准。
- `story-ctest02.log`：60 份脚本共 145 个对白键、剧情标记、章节结尾及死亡分支全部通过；最短阅读时间、已读跳过返回值通过。
- 首次音频场景启动失败：资源键 `fly.106-1` 不符合引擎 dotted-key 约束。修复游戏导入器为 `fly.voice_106_1`，不修改引擎校验。修复后的场景回放正在执行。
- Windows 桌面处于锁屏时原生窗口操作失败；没有尝试解锁。游戏自身 SDL 输入回放与渲染截图仍可正常运行。菜单鼠标、键盘焦点和启动 Logo 图像需要桌面可用时补验。

## 增量构建失效与堆损坏

- `build-story-scene01.log` 只重编译 `flight_objects.cpp` 和 `flight_scene.cpp`，没有重新编译包含新 `FlightScene` 类定义、负责模板实例化分配的 `game_module.cpp`。
- `ninja -C out/build/release -t deps game/CMakeFiles/game_lib.dir/application/game_module.cpp.obj` 得到 `#deps 0`，见 `header-deps-before.log`。CMake 生成的 `msvc_deps_prefix` 是中文编码乱码，而 MSVC 实际输出正常中文。故头文件修改没有触发重编译，旧分配尺寸与新构造函数不一致。
- Windows 应用日志记录 `0xc0000374` 堆损坏及清理阶段 `0xc0000005`。不是据此推断引擎缺陷。
- 单独设置 VSLANG 无效（本机只有中文编译器语言资源）；根构建配置最终使用 Python `game/tools/msvc_output.py` 作为 launcher，将中文 showIncludes 前缀转换为 Ninja 预期的英文，并保留编译器退出码；适用于从 helper 和直接 Ninja 构建。只改变项目构建配置，无引擎源码修改。
- 增加 MSVC AddressSanitizer 构建入口，工具链的 STL 容器注解要求主程序、游戏、引擎及 HarfBuzz 使用一致 ASan 编译选项；首次链接不匹配记录于 `build-asan*.log`，已修正构建目标配置。
- 另发现教程时序问题：原脚本在演示之后才定义 `onRoleReachedMoon`。协程运行器现缓存初始脚本执行期间的事件，待回调定义完成再派发；新增“事件先于回调定义”的单测。
- `story-asan02.log`：首关原对白、教程演示、复位、玩家输入、3 星通关、干净退出，进程退出码 0，无 ASan 报错。普通优化 Release 正在完整重建复测，不能用诊断版替代最终交付。

## 2026-10-24 普通 Release、机关与教程记录

- `build-dependency-fix02.log`：普通 Release 全量重建成功；`header-deps-after.log` 中 game_module 对象记录 182 项依赖。后续修改 FlightScene 头文件正确触发所有包含者重编译。
- `story-release-fixed.log`：普通 Release 的首关完整剧情、演示复位、键盘飞行、3 星通关及干净退出通过。
- `mechanisms-ctest02.log`：3 项 CTest 均通过；运动参数边界、转轴引擎约束、原始触发目标均有覆盖。
- `level4-moving.json`：第 4 关两平台实际 x 行程约 223.94 和 191.97；`level7-rotating.json`：第 7 关有多个实际旋转轴；`level4-spike-failure.json`：输入后触碰尖刺，失败并退出码 0。平台默认曲线和尖刺边编号仍属近似。
- `menu-snapshot`、`menu-selection`：实际启动并由 Enter 切换菜单到选关，渲染截图中中文正常，第一关三星存档与第二关解锁可见。
- 教程缺失判定更正：`tutorialRep_001.plist` 内三个键分别解码出 718、1422、1103 帧，按压帧 322、1052、357；每条记录由 8 字节头及 1 字节标记、按压时 8 字节浮点坐标组成，恰好消费所有源字节。`tutorial-import.json` 可核查。
- `level1-original-replay01.json`：首次原始输入回放失败，角色因旧连续角度映射偏左，触碰顶部而未到月亮，教程不能结束。保留失败与原预期；没有删除通关/三星/退出演示等断言。修复按角色配置定义中央竖直区域，外围平滑转向和上方按压拒绝；角度传递函数仍为推断，不能宣称精确原手感。

- `level1-original-replay02.json`：原始录制到月、角色复位和三星通关都已发生，但脚本末尾无条件 Enter 又进入第二关，最终关卡断言失败。为避免依赖结算时长，新增事件 `when` 条件，仅在指定对白可见时发送末尾确认；输入仍通过 SDL 正常队列，不改位置、存档或成功规则。
- `level1-original-replay03.json` / stderr.log：修正回归脚本后所有原预期（第一关、三星、成功、输入恢复、演示结束）通过，进程退出 0。`tutorial-ctest01.log` 3 项通过，包括原始回放完整遍历和角度边界。

- `wind-portal-ctest01.log`：3 项测试通过，新增风场旋转、强度、淡出与范围边界。`level10-wind.json`：风力最大 x=320，角色实际 x 位移约 124；`level13-portal.json`：正常输入飞行穿过 0→1 传送门，随后触发对象 2 的 Start，均退出 0，截图已查看。新增采样范围断言用于验证实际发生的过程，不仅检查最终状态。


## 2026-10-24：机关、60 关入口与显示

- `level20-projectiles`、`level24-homing`：实际生成发射弹/追踪弹；`level31-bomb04`：实际激活 3 个炸弹。后者只验证激活，不能证明最终失败一定由炸弹造成。
- `level31-bomb`、`02`、`03` 的失败保留：初版出生/旋转锚点不准确，随后修复载体角度 270°/-90° 的整圈差；第三轮输入太晚触碰危险物，第四轮提前正常飞行。没有移除危险物或直接改变玩家位置。
- `entry01-summary.json`：60/60 正常进入、资源加载、退出 0。未进行每关通关，不能以此代替验收。
- `menu-artwork-en`、`dialogue-ja`、`dialogue-zh`：进程运行与断言通过，生成的 BMP 已转换 PNG 并人工检查；英文菜单和中日剧情未见缺字或裁切。
- 原版射击时序、载体旋转边界、激光遮挡均纳入 mechanism_tests；详见对应 CTest 日志。

## 用户引擎更新后的适配

用户明确说明物理系统更新，更新后基线 641 文件另存，旧基线和差异保留。适配经过 `build-engine-update01.log`、4 项 `engine-update-ctest01.log`；旧 AabbShape、查询过滤和关节平铺参数按新文档迁移。游戏未修改引擎实现。

原版飞行计算、复合碰撞体与首关实机回归见 ../original-flight-research.md。`original-flight-ctest02.log` 还包含真实 PhysicsWorld 中的复合形状注册、线段落地、15 秒持续飞行终端速度检查。
