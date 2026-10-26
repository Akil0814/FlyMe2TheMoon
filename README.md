# FlyMe2TheMoon

## 手柄操作

支持 SDL3 能识别的手柄，可在运行中连接或拔出。首次使用时先按一下任意手柄按钮接管菜单，再操作；按钮名称按 Xbox 布局标注，其他手柄对应相同位置。

| 场景 | 操作 |
| --- | --- |
| 菜单、暂停和结算 | 方向键导航，A 确认，B 返回；结算时 A 执行当前选中按钮 |
| 飞行 | 左摇杆向上、左或右推动即可喷射，横向偏转控制倾角；方向键 ↑ / ← / → 也可飞行 |
| 垂直喷射 | 按住 A，或渐进按下 RT；可同时用左摇杆控制倾角 |
| 暂停 / 恢复 | Start；暂停菜单中也可按 B 恢复 |
| 重试当前关卡 | X |
| 剧情 | A 继续，Y 跳过已读剧情，仍遵循原有等待和跳过限制 |
| 制作名单 | 左摇杆滚动，X 暂停 / 继续自动滚动，B 返回 |

左摇杆和 RT 的死区均为 20%，越过死区后推力连续增加。向下推摇杆不会单独喷射。键盘飞行输入优先于手柄，鼠标按住喷射时优先于两者。飞行和剧情提示会随输入设备切换。

当前启动流程：加载资源 → 按任意键确认 → 最小主页。主页仅显示项目名和 `Exit` 按钮，尚未添加玩法。

## Windows 构建与启动

需要支持 C++23 的 MSVC、Windows SDK、CMake 3.24 或更新版本，以及 Ninja。依赖源码已包含在 `thirdparty/`，配置时无需下载。

在 Visual Studio 的 **x64 Native Tools Command Prompt** 中，进入项目根目录后执行：

```bat
cmake -S . -B out/build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/debug --target FlyMe2TheMoon --parallel
out\build\debug\FlyMe2TheMoon.exe
```

也可以使用 Visual Studio 的“打开文件夹”功能构建根目录的 CMake 项目，启动目标选择 `FlyMe2TheMoon`。

MSVC Debug 使用 `/Z7` 内嵌调试信息，避免编译器 PDB 服务引发的 `C1902`。构建会自动将 `SDL3.dll` 复制到可执行文件旁。

启动时会向上查找项目的 `assets/`，因此可以从项目根目录或上述构建目录运行；请保留项目资源目录。运行日志位于 `logs/`，用户配置位于 `player_data/`。

加载完成后按任意键进入主页。点击 `Exit`、按 `Enter` 确认退出按钮、按 `Esc` 或关闭窗口均可退出。默认保留 ImGui 开发覆盖层；如需禁用，在配置命令中添加 `-DELYSIA_ENABLE_IMGUI=OFF`。

## 局内开发面板

运行启用 ImGui 的构建后，按 **F2** 显示或隐藏开发面板，菜单与关卡中均可使用：

- `Enable debug draw`：开启调试绘制，可分别选择碰撞体、接触点/法线、AABB、速度、姿态历史、关节等类别；也可一键全选、清空或恢复默认。物理图元会在进入关卡后显示，Camera/Gameplay 类别需要相应系统提交图元。
- `Show FPS overlay`：显示 FPS、帧耗时；按 F2 收起面板后，右上角统计仍会保留，且不拦截游戏输入。面板内另有最近 120 帧的耗时曲线。
- `Time scale`：在 0–2 倍速之间调整模拟速度，`Pause` 暂停、`Normal speed` 恢复。暂停时开发面板仍可操作，FPS 使用真实帧时间。
- 关卡内还会显示 `Camera`：`Zoom` 可在 0.1–10 倍之间实时调整世界相机缩放，并提供 0.5/1/2 倍快捷按钮；`Reset zoom` 恢复关卡默认的 1.5 倍。游戏 HUD 大小保持不变，重新进入关卡会恢复默认缩放。

这些选项仅在本次运行中有效。开发构建可直接运行 `game\tools\build-development.cmd`，然后启动 `out\build\x64-Debug\FlyMe2TheMoon.exe`。现有发布/诊断构建脚本会显式关闭 ImGui；需要面板时请使用开发构建。
