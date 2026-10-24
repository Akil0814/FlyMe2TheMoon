# FlyMe2TheMoon

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
