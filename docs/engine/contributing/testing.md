# 测试与文档示例验证

本文面向修改引擎和测试的开发者，说明如何验证当前变更。自动化测试由 CTest 管理，源码位于 `tests/`；文档中的完整场景示例另有编译目标。

## 1. 配置与构建

先按照[构建指南](../user-guide/build-and-run.md)配置工程，并保持 `BUILD_TESTING=ON`。下面以 `out/build/sdl3-Debug` 为例；已有目录使用其他生成器时，应沿用其配置，不要直接切换生成器。

```powershell
cmake --build out/build/sdl3-Debug --config Debug --parallel
ctest --test-dir out/build/sdl3-Debug -C Debug --output-on-failure
```

测试目标通过 `add_elysia_test` 链接 `engine_lib`，部分集成测试还链接 `game_lib`。Windows 构建会为这些目标复制 SDL 运行时 DLL。`--output-on-failure` 显示失败测试的输出，便于定位问题。

## 2. 按标签选择测试

普通测试使用 `application`、`audio`、`bootstrap`、`builtin`、`camera`、`config`、`core`、`effects`、`elysia`、`gameplay_ui`、`gpu`、`input`、`loading`、`localization`、`object_query`、`persistence`、`physics`、`save`、`scene`、`tools`、`typography` 和 `ui` 标签。

物理场景和可选压力案例另有 `physics_scenario`、`physics_stress` 标签。

```powershell
# 查看当前配置实际注册的测试和标签
ctest --test-dir out/build/sdl3-Debug -C Debug -N
ctest --test-dir out/build/sdl3-Debug -C Debug --print-labels

# -L 使用正则表达式；加边界可以只匹配指定标签
ctest --test-dir out/build/sdl3-Debug -C Debug -L '^physics$' --output-on-failure
ctest --test-dir out/build/sdl3-Debug -C Debug -L '^ui$' --output-on-failure
```

设备要求统一见[构建指南](../user-guide/build-and-run.md#运行测试)。`gpu` 测试需要实际图形设备与可用驱动，不能用 dummy 视频驱动替代。`-LE '^gpu$'` 只排除带该标签的用例，不自动验证剩余测试的所有环境要求。需要 GPU 的应用生命周期分支同时标记 `application` 与 `gpu`；两个 `manager_*` 分支仅标记 `application`。无显示环境应按实际设备、音频与文件系统要求选择用例，并记录排除范围。软件渲染测试不等于实际 GPU 或窗口交互验收。

## 3. 编译文档示例

```powershell
cmake --build out/build/sdl3-Debug --config Debug --target scene_documentation_examples scene_documentation_examples_en
```

该目标由 [Markdown 提取脚本](../../../tests/scene/compile_scene_documentation.cmake)直接提取文档中指定的 C++ 代码块，检查场景、对象、服务调用和 UI 示例是否仍符合公开接口。覆盖文件与代码块数量以脚本中的列表为准，概念片段和需要调用方上下文的局部片段不全部纳入。它是编译目标，不是 CTest 测试；不能只运行 `ctest` 就认为示例已验证。

修改被提取的文档时，应保留脚本指定的代码块顺序和示例类型。若确实要调整提取方式，需要同步脚本；新增局部片段不要插到被提取的代码块之前。概念片段应注明上下文和省略部分，不能标为可独立运行程序。

## 4. 可选物理验证

`ELYSIA_ENABLE_FULL_PHYSICS_STRESS_TESTS=ON` 额外注册中、高档压力测试。这些案例使用 `physics_stress` 标签，串行运行且允许更长的超时；日常默认配置不注册它们。

`ELYSIA_BUILD_PHYSICS_BENCHMARK=ON` 构建独立的 `physics_benchmark`。性能测量应使用 Release，并记录工具链、环境和测量范围。具体历史结果见[物理验证与性能](../architecture/subsystems/physics/08-implementation-roadmap-and-tests.md)。

## 5. 变更后的检查

代码变更先运行受影响子系统的测试；涉及多个子系统的运行契约时，再运行完整测试集。Development Demo 用于实际观察和操作，不代替自动化回归。

文档变更应运行链接检查；涉及完整示例时还要编译示例。语言特性盘点使用统计脚本验证：

```powershell
python scripts/check_documentation_links.py
python scripts/check_documentation_links.py --self-test
python scripts/check_documentation_parity.py
python scripts/check_documentation_parity.py --self-test
pwsh -File scripts/audit_cpp_language_features.ps1 -SelfTest
pwsh -File scripts/audit_cpp_language_features.ps1 -Language zh-CN -Check
pwsh -File scripts/audit_cpp_language_features.ps1 -Language en -Check
git diff --check
```

`python` 需要 Python 3，统计脚本需要 PowerShell 7。检查报告应区分编译结果、运行测试、截图检查和人工体验。历史验收数字不能替代当前变更的验证结果。

无参数链接命令完整检查中英文目录及两份根 README，并核对其他语言指向这两个目录的入站链接；不完整检查日文。仍支持显式路径参数，例如 `python scripts/check_documentation_links.py docs/zh-CN README.zh-CN.md`。检查范围为本地行内 Markdown 链接、标题锚点和围栏，不验证外链、HTML 图片或引用式链接。双语检查验证相对路径、标题层级、围栏语言与顺序、JSON 内容及除注释和空白外的 C++ 标记（保留字符串字面量），并检查英文入口可达性。两个编译目标使用同一提取清单，分别从中文和英文生成文件；未纳入的概念片段不宣称通过编译。语言特性统计的 `-Language` 默认为 `zh-CN`；生成和更新英文时分别使用 `-Language en` 和 `-Language en -Update`。
