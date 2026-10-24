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
- 根构建配置用 `cmake -E env VSLANG=1033` 作为编译器 launcher，并将 showIncludes 前缀统一为英文；适用于从 helper 和直接 Ninja 构建。只改变项目构建配置，无引擎源码修改。
- 增加 MSVC AddressSanitizer 构建入口，工具链的 STL 容器注解要求主程序、游戏、引擎及 HarfBuzz 使用一致 ASan 编译选项；首次链接不匹配记录于 `build-asan*.log`，已修正构建目标配置。
- 另发现教程时序问题：原脚本在演示之后才定义 `onRoleReachedMoon`。协程运行器现缓存初始脚本执行期间的事件，待回调定义完成再派发；新增“事件先于回调定义”的单测。
- `story-asan02.log`：首关原对白、教程演示、复位、玩家输入、3 星通关、干净退出，进程退出码 0，无 ASan 报错。普通优化 Release 正在完整重建复测，不能用诊断版替代最终交付。
