# 构建、运行与测试

本文说明如何构建和运行仓库中的示例程序，以及如何运行测试。以下命令均从仓库根目录执行。

## 环境要求

- CMake 3.24 或更高版本。
- 支持 C++23 的编译器；Windows 推荐 MSVC x64。
- 对应生成器的构建工具，例如 Ninja 或 Visual Studio 的 MSBuild。
- 运行示例程序和 GPU 测试时，需要支持 SDL3 GPU renderer 的图形设备与驱动。

第三方依赖源码随仓库提供，正常配置和构建无需下载依赖。SDL3 使用共享库，SDL3_image、SDL3_ttf、SDL3_mixer、SDL3_gfx 及选定编解码器静态链接。具体版本和本地补丁见[依赖来源](../../../thirdparty/SDL3-DEPENDENCIES.md)。

## Windows 构建与运行

### MSVC + Ninja

安装 Visual Studio 的“使用 C++ 的桌面开发”工作负载，并准备 CMake 与 Ninja。打开 **x64 Native Tools Command Prompt**，切换到仓库根目录后执行：

```bat
cmake -S . -B out/build/msvc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/msvc-debug --parallel
.\out\build\msvc-debug\ElysiaEngine.exe
```

Release 使用单独的构建目录：

```bat
cmake -S . -B out/build/msvc-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/msvc-release --parallel
.\out\build\msvc-release\ElysiaEngine.exe
```

Ninja 在配置时通过 `CMAKE_BUILD_TYPE` 选择构建类型，可执行文件直接位于构建目录内。MSVC 的 x64 架构由开发者命令行环境确定。

### Visual Studio 生成器

以下示例使用 Visual Studio 2026 及其随附的 CMake；其他版本请通过 `cmake --help` 查看可用生成器，并替换 `-G` 后的名称。

```powershell
cmake -S . -B out/build/msvc-vs -G "Visual Studio 18 2026" -A x64
cmake --build out/build/msvc-vs --config Debug --parallel
.\out\build\msvc-vs\Debug\ElysiaEngine.exe
```

Visual Studio 是多配置生成器，同一构建目录可以构建 Debug 和 Release。构建时使用 `--config Release`，运行时使用对应的 `Release/` 子目录。

### 运行时文件

从仓库根目录启动，并保留完整的 `assets/` 目录，包括项目标记 `assets/.elysia_root` 和内容注册表 `assets/content_registry.json`。CMake 会在 Windows 构建后将 SDL3 DLL 复制到程序和测试可执行文件所在目录。

## 常用构建选项

| 选项 | 默认值 | 用途 |
| --- | --- | --- |
| `ELYSIA_ENABLE_IMGUI` | `ON` | 构建 Dear ImGui 开发覆盖层，运行时按 F2 显示或隐藏。 |
| `BUILD_TESTING` | `ON` | 构建并注册测试；只构建示例程序时可关闭。 |

在配置命令末尾添加 `-DELYSIA_ENABLE_IMGUI=OFF` 或 `-DBUILD_TESTING=OFF` 即可关闭对应功能。Debug 和 Release 都可以启用或关闭开发覆盖层。接口说明见[开发覆盖层](tools/development-panels.md)。

## 运行测试

默认构建包含测试。在完成构建后，使用对应构建目录运行：

```powershell
ctest --test-dir out/build/msvc-debug --output-on-failure
```

Visual Studio 生成器还需要指定配置：

```powershell
ctest --test-dir out/build/msvc-vs -C Debug --output-on-failure
```

可按标签或测试名称筛选；以下命令使用 Ninja 的 Debug 构建目录：

```powershell
ctest --test-dir out/build/msvc-debug -L gpu --output-on-failure
ctest --test-dir out/build/msvc-debug -L input --output-on-failure
ctest --test-dir out/build/msvc-debug -L ui --output-on-failure
ctest --test-dir out/build/msvc-debug -R "^scene_lifecycle_regression_tests$" --output-on-failure
```

GPU 测试需要真实图形设备，不能使用 dummy video 驱动。`sdl3_gpu_integration_tests` 会在测试工作目录生成 `sdl3-gpu-verification.png`；示例应用 smoke test 会自动注入退出事件。

`-LE '^gpu$'` 仅跳过标记为 `gpu` 的测试。需要 GPU 的应用生命周期分支同时标记 `application` 与 `gpu`；两个仅检查场景管理器的分支不需要 GPU。标签筛选不保证剩余用例满足所有无显示环境条件，仍须检查各测试的其他设备与文件系统要求。

修改指南中的示例后，可单独构建文档示例目标；它要求配置时保持 `BUILD_TESTING=ON`。

```powershell
cmake --build out/build/msvc-debug --target scene_documentation_examples
```

该目标直接从 Markdown 提取指定的完整示例进行编译，不运行游戏，也不覆盖所有概念片段。覆盖范围与维护方法见[测试与文档示例验证](../contributing/testing.md#3-编译文档示例)。

## 其他平台

Linux、macOS 和 MinGW 使用相同的仓库内依赖源码，还需准备对应的 C++23 工具链、构建工具及系统图形和音频开发库。Linux 的 Ninja 配置入口如下：

```bash
cmake -S . -B out/build/linux-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/linux-debug --parallel
./out/build/linux-debug/ElysiaEngine
ctest --test-dir out/build/linux-debug --output-on-failure
```

macOS 或 MinGW 请使用各自的构建目录和工具链。其他平台的构建与运行仍需分别验证；系统依赖要求可参考仓库内的 [SDL README](../../../thirdparty/SDL3/README.md)。

## 常见问题

- 找不到 `cmake`、`ninja` 或 MSVC 标准库头文件：检查工具安装与 PATH，并在 x64 开发者命令行中运行 MSVC + Ninja 构建。
- 生成器或架构不匹配：切换生成器、工具链或架构时使用新的构建目录。
- GPU 创建失败：检查系统图形驱动与 SDL GPU 后端支持；具体提示、处理结果和检查步骤见[启动失败与排查](game-initialization.md#7-启动失败与排查)。
- 找不到资源、项目根目录或启动加载失败：按[启动阶段排查](game-initialization.md#72-按阶段排查常见错误)，区分启动配置、预加载和项目内容加载。
- 找不到 SDL3 DLL：确认程序已完成构建，并保留可执行文件旁的 DLL；Debug 和 Release 文件应使用各自的构建输出。
