# 游戏项目初始化指南

本文面向准备接入游戏代码的开发者，说明程序入口、游戏模块、应用配置和场景注册各自负责什么，以及如何把它们连接起来。阅读前应已能按[构建与运行指南](build-and-run.md)构建仓库。

程序入口把运行交给引擎，游戏模块提供配置并注册游戏场景，引擎负责初始化子系统和驱动场景。下面先介绍这一分工，再给出最小模块和启动加载路由示例。示例沿用仓库的 CMake 包含路径配置；模块代码可编译，但运行仍需要有效的项目配置与资源目录。

如果程序已经能构建，但启动报错、加载失败或没有进入目标场景，可直接阅读[启动失败与排查](#7-启动失败与排查)。

## 1. 初始化流程

引擎通过 `IGameModule` 获取应用描述并注册游戏场景。`ELYSIA_INITIALIZE_APP` 不只是保存描述：它还准备运行环境并启动初始场景。成功后，`ELYSIA_RUN_APP` 才开始持续处理输入、更新和绘制。资源中的只读游戏配置另由内容加载流程发布，见[配置指南](content/configuration.md)。

```text
main()
  |
  +-- 创建 GameModule
  |
  +-- ELYSIA_INITIALIZE_APP
  |     +-- 获取并校验 ApplicationDescriptor
  |     +-- 读取运行配置，初始化窗口、渲染器等子系统
  |     +-- 准备内置资源、输入和场景运行上下文
  |     +-- 注册内置场景，再调用 register_scenes() 注册游戏场景
  |     +-- 按 initial_route 启动初始场景
  |
  +-- 初始化成功后调用 ELYSIA_RUN_APP
  |     +-- 输入、更新、绘制
  |     +-- 退出时清理运行资源
  |
  +-- 返回程序退出状态
```

这个流程只列出接入游戏时需要理解的阶段，并非应用内部所有初始化步骤。游戏代码不需要在 `main()` 中逐个初始化引擎子系统。

## 2. 程序入口

下面保留仓库 [main.cpp](../../../main.cpp) 的启动方式和最外层异常处理。它使用现有示例游戏模块，可以作为替换游戏入口时的参考。

```cpp
#include <SDL3/SDL_main.h>
#include "engine/application/application.h"
#include "game/application/example_game_module.h"

#include <cstdlib>

int main(int argc, char** argv)
{
    try
    {
        example::application::GameModule game_module;
        if (!ELYSIA_INITIALIZE_APP(argc, argv, game_module))
            return EXIT_FAILURE;

        return ELYSIA_RUN_APP ==
                       elysia::application::ApplicationRunResult::NormalExit
                   ? EXIT_SUCCESS
                   : EXIT_FAILURE;
    }
    catch (...)
    {
        return EXIT_FAILURE;
    }
}
```

初始化返回 `false` 时，不应继续调用 `ELYSIA_RUN_APP`。运行结果为 `NormalExit` 时返回成功，其余结果返回失败。

引擎会在初始化和主要运行阶段捕获异常，并按对应边界处理失败。入口中的 `catch (...)` 则负责仍然传播到这里的 C++ 异常，例如游戏模块构造期间的异常；它只把程序退出码设为失败，不代表游戏恢复运行，也不会在这里补记日志。异常若试图逃出 `noexcept` 函数，会触发 `std::terminate()`，不能依靠这一层捕获继续清理。

## 3. 定义游戏模块

游戏模块继承 `elysia::application::IGameModule`，向引擎提供游戏专属配置和场景。接口定义见 [game_module.h](../../../engine/application/game_module.h)。

### 3.1 IGameModule 接口

| 方法 | 职责 | 是否必须实现 |
| --- | --- | --- |
| `descriptor() const` | 返回 `ApplicationDescriptor`，包含逻辑分辨率、呈现配置和初始路由 | 是 |
| `register_scenes(SceneManager&) const` | 向场景管理器注册游戏场景 | 是 |
| `create_development_overlay() const` | 返回拥有所有权的 `std::unique_ptr<IDevelopmentOverlay>`，供引擎接管开发工具覆盖层 | 否 |

`descriptor()` 提供数据，`register_scenes()` 建立场景标识符与场景类型的对应关系。注册场景不会在此处直接构造所有游戏场景；场景管理器在需要进入时创建实例。开发工具接口默认返回空指针，不使用覆盖层时无需重写。

### 3.2 创建自己的 GameModule

下面是完整的模块示例，可放入自己的头文件，例如 `game/application/my_game_module.h`。它注册一个空场景，并直接将该场景设为初始目标，用来说明配置与注册必须指向同一个有效标识符。

```cpp
#pragma once

#include "engine/application/game_module.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"

namespace initialization_example
{
// 示例选择的游戏场景键，须与本项目其他游戏场景保持唯一。
inline constexpr elysia::scene::SceneKey kEntry = 1;

class EntryScene final : public elysia::scene::Scene
{
protected:
    // 最小入口场景不持有业务对象，三个生命周期钩子在此为空实现。
    void on_enter(const elysia::scene::ScenePayload&) override
    {
    }

    void on_exit() override
    {
    }

    void on_reset() override
    {
    }
};

class GameModule final : public elysia::application::IGameModule
{
public:
    [[nodiscard]] elysia::application::ApplicationDescriptor
        descriptor() const override
    {
        elysia::application::ApplicationDescriptor descriptor;
        descriptor.logical_width = 1280;
        descriptor.logical_height = 720;
        // 场景注册完成后，应用通过该路由激活入口。
        descriptor.initial_route = {.target = kEntry};
        return descriptor;
    }

    void register_scenes(
        elysia::scene::SceneManager& scene_manager) const override
    {
        scene_manager.register_game_scene<EntryScene>(kEntry);
    }
};
}
```

使用这一模块时，将入口中的模块头文件和类型分别替换为自己的头文件和 `initialization_example::GameModule`。仓库的 `engine_lib`、`game_lib` 已把项目根目录加入包含路径，因此这里使用 `engine/...`，不依赖头文件恰好位于某个目录层级。

`EntryScene` 没有添加可见内容，也没有执行项目内容加载。直接进入这种场景适合验证模块接线；若场景依赖项目资源和项目字体，应按下一节接入启动加载流程。场景回调的职责与重置行为见[核心概念](core_concepts.md)和[场景系统](scene/scene.md)。

## 4. 配置应用

`descriptor()` 返回的 `ApplicationDescriptor` 决定应用启动时使用的配置。

| 字段 | 默认值及用途 |
| --- | --- |
| `logical_width` | 默认 `1280`，逻辑画面宽度；必须大于零 |
| `logical_height` | 默认 `720`，逻辑画面高度；必须大于零 |
| `initial_route` | 默认目标为 `SceneKeys::Invalid`，必须显式配置有效初始路由 |
| `presentation` | 包含纹理过滤方式、字体来源与字号、默认 UI 主题及启动引擎 Logo 选择 |

逻辑分辨率用于场景坐标和逻辑绘制，不等于窗口的实际像素尺寸。窗口设置与用户配置的关系见[配置指南](content/configuration.md)。呈现配置的完整字段与默认值见 [application_presentation_settings.h](../../../engine/application/application_presentation_settings.h)。

### 4.1 配置初始场景

上一节直接进入空场景。仓库示例则通过内置 `StartupLoading` 准备项目内容，再转到主菜单。下面的完整辅助函数创建这种路由；传入的成功路由必须指向已经注册的游戏场景。

```cpp
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/startup_loading_scene_payload.h"

#include <utility>

namespace initialization_example
{
elysia::scene::SceneRoute make_startup_route(
    elysia::scene::SceneRoute success_route)
{
    elysia::builtin::StartupLoadingScenePayload payload;
    // 正式内容准备完毕后才进入使用项目资源的目标场景。
    payload.success_route = std::move(success_route);
    payload.wait_for_logo_sequence = false;
    payload.wait_for_confirmation = true;
    return {
        .target = elysia::builtin::SceneKeys::StartupLoading,
        .payload = std::move(payload),
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    };
}
}
```

将辅助函数放在模块使用它的位置之前，或在头文件中声明后于实现文件中定义。随后把模块中的初始路由赋值改为 `descriptor.initial_route = make_startup_route({ .target = kEntry });`，即可在内容加载和确认完成后进入 `EntryScene`。

上面两项等待配置与仓库示例一致：资源加载成功后不必等 Logo 序列结束，但仍需用户确认。因此，示例游戏的流程是：

```text
StartupLoading
    |
    +-- 加载项目内容并激活项目字体
    |
    +-- 加载成功后显示确认提示
    |
    +-- 用户确认
    v
MainMenu
```

如果希望加载完成后自动进入目标，可把 `wait_for_confirmation` 设为 `false`。两项等待配置只控制成功流程的等待条件，不会跳过内容加载或失败处理。路由 Payload、Logo 和加载失败行为见[启动加载场景](builtin-scenes/startup-loading.md)；资源目录与清单的准备见[资源加载](content/resources.md)。

## 5. 注册游戏场景

`register_scenes()` 通过 `register_game_scene<T>(key)` 注册游戏场景。前面的模块示例使用 `kEntry`；实际游戏可以继续注册主菜单、关卡和结算等场景，并在路由中使用同一组标识符。

游戏场景的 `SceneKey` 范围为 `1` 到 `999`，同一标识符不能重复注册。引擎已在调用游戏模块前注册内置场景，游戏模块不需要重复注册 `StartupLoading`。

初始目标和后续切换目标都必须已注册。使用启动加载场景时，既要让 `initial_route` 指向内置加载场景，也要在游戏模块中注册其 `success_route` 指向的游戏场景。合法的数字不代表场景已经存在。

场景构造参数、切换、传参和生命周期见[场景系统](scene/scene.md)。游戏模块只负责注册；具体对象的创建和场景内容组织应放在场景中。

## 6. 自定义开发面板（可选）

开发工具覆盖层负责工具界面的初始化、事件处理和绘制；具体开发面板则通过它提供的 `IDevelopmentPanelRegistry::register_panel()` 注册。这是两个层次：创建覆盖层不等于已经添加自定义面板。

仓库示例重写 `create_development_overlay()`，返回 `ImGuiDevelopmentOverlay`。下面是该方法的局部实现片段，需配合模块类中的同名 `const override` 声明；启用 ImGui 的分支还需包含 `engine/tools/imgui/imgui_development_overlay.h`。

```cpp
std::unique_ptr<elysia::tools::IDevelopmentOverlay>
GameModule::create_development_overlay() const
{
#if ELYSIA_ENABLE_IMGUI
    return std::make_unique<elysia::tools::ImGuiDevelopmentOverlay>();
#else
    return {};
#endif
}
```

只有 CMake 选项 `ELYSIA_ENABLE_IMGUI` 启用时，应用才调用这个工厂并接管返回的覆盖层。关闭该选项时，即使模块重写了方法，应用也不会调用它。无需开发工具时，可以保留接口的默认空实现。

覆盖层由引擎初始化、驱动和关闭，游戏模块不应自行调用其运行周期方法。自定义实现还必须遵守 `shutdown() noexcept` 等接口约定，不能让异常逃出这些函数。完整接口见 [development_overlay.h](../../../engine/tools/development_overlay.h)，可工作的工厂声明与实现见仓库游戏模块。

注册与注销面板的完整场景示例见[开发面板指南](tools/development-panels.md)。

## 7. 启动失败与排查

排查时先保留本次启动的控制台输出、日志和错误提示，再判断失败发生在哪个阶段。下面列出接入项目时常见的问题；错误码表示失败类别，具体原因仍需结合其后的诊断条目判断。

### 7.1 先判断启动停在哪里

| 阶段 | 此时正在准备什么 | 优先查看的位置 |
| --- | --- | --- |
| 配置准备 | 游戏模块描述、项目根目录、注册表、应用与用户配置 | 控制台、启动弹窗、配置诊断 |
| 平台初始化 | SDL、音频设备、字体库、窗口和 GPU 渲染器 | `platform`、`audio` 分类日志与启动提示 |
| 基础资源与场景准备 | 开发覆盖层、内置资源、本地化、字体、预加载、场景注册 | `builtin`、`localization`、`typography`、`scene` 等分类日志 |
| 项目内容加载 | 清单与加载计划、项目配置、纹理、动画、音频、字体 | `CONTENT-…` 诊断、启动加载失败界面 |
| 进入目标场景 | 等待条件、成功路由、目标场景构造与进入 | 加载状态、路由配置、场景原始异常 |

这些阶段用于阅读，不是源码中的统一状态枚举。若初始路由使用 `StartupLoading`，内容清单解析和请求组装会在它的 `on_enter()` 中启动，后续加载由主循环继续推进。因此，`ELYSIA_INITIALIZE_APP` 返回成功不代表项目内容已加载成功，也不保证此时显示的是游戏目标场景。

加载成功后仍停在启动画面，先检查 `wait_for_confirmation` 和 `wait_for_logo_sequence`。按配置等待确认或 Logo 结束是正常流程，详见[启动加载场景](builtin-scenes/startup-loading.md)。

### 7.2 按阶段排查常见错误

下表中的英文文本是可搜索的错误码或日志片段，省略了动态路径、资源键和异常详情。“终止初始化”表示记录失败、尝试提示并清理，最终返回 `false`；“加载失败路由”和“场景错误边界”的行为见下一节。

#### 配置准备

| 错误码或日志关键词 | 常见原因 | 引擎处理 | 优先检查项 |
| --- | --- | --- | --- |
| `BOOTSTRAP-PROJECT-ROOT` | 从启动路径向上搜索时找不到项目标记 | 终止初始化 | 检查诊断列出的搜索路径与 `assets/.elysia_root`。应用优先传入可执行文件路径；相对路径还受工作目录影响，不要只调整 IDE 工作目录而忽略发布目录结构。 |
| `BOOTSTRAP-ASSET-DIRECTORY`、`BOOTSTRAP-PATH-ACCESS` | 必需资源目录缺失，或无法访问路径 | 终止初始化 | 检查诊断中的实际路径及权限；`assets/` 下必须有 `audio/`、`textures/`、`fonts/`、`configs/`，目录完整不代表其中资源文件已齐全。 |
| `BOOTSTRAP-RUNTIME-DIRECTORY`、`SAVE-INITIALIZE` | 运行数据目录创建或存档服务初始化失败 | 终止初始化 | 检查项目根目录下 `player_data/`、`player_data/saves/`、`logs/` 的权限，以及目标是否被同名文件占用；保留存档与诊断。 |
| `BOOTSTRAP-CONTENT-REGISTRY`、`BOOTSTRAP-APP-CONFIG` | 注册表或应用配置缺失、JSON 无效、字段不符合格式 | 终止初始化 | 从 `assets/content_registry.json` 的 `bootstrap.app_config` 追到实际文件，按诊断的 JSON Pointer 检查字段。不要把后续项目内容清单与启动应用配置混为一处。 |
| `BOOTSTRAP-USER-CONFIG` | 用户配置读取、恢复或写回遇到问题 | 可能恢复并警告，也可能终止初始化 | 查看日志级别及恢复条目，检查 `player_data/user_config.json` 和诊断列出的恢复副本；未来版本格式、访问失败和恢复写回失败不能按普通损坏自动忽略。 |
| `Game module descriptor failed`、`Game module logical viewport must be positive`、`Application default UI theme is invalid` | `descriptor()` 抛异常、逻辑尺寸非正数或默认主题无效 | 终止初始化 | 检查模块描述和 `presentation` 配置；尺寸应大于零，主题须为受支持的值。 |

配置关系见[配置指南](content/configuration.md)，注册表字段见[启动与注册表参考](../reference/resources/bootstrap-and-registry.md)。路径与配置错误码分别由 [PathManager](../../../engine/io/path/path_manager.cpp) 和 [BootstrapFailure](../../../engine/bootstrap/bootstrap_types.h) 的实现提供依据。

#### 平台初始化

| 错误码或日志关键词 | 常见原因 | 引擎处理 | 优先检查项 |
| --- | --- | --- | --- |
| `SDL3 Error`、`SDL_ttf Error` | SDL 子系统或字体库初始化失败 | 终止初始化 | 检查运行时依赖、系统环境及控制台信息；需要进一步定位时查看对应初始化调用，不能仅凭总提示推断具体设备原因。 |
| `SDL_mixer Error`、`AudioService initialization failed` | 混音库、音频设备或轨道准备失败 | 终止初始化 | 检查系统播放设备、驱动和运行依赖。这发生在项目音频加载前，不应先更改音效资源键。 |
| `SDL_CreateWindow Error`、`SDL GPU renderer creation failed`、`SDL VSync configuration failed` | 窗口、GPU 后端或呈现设置不可用 | 终止初始化，不自动退回软件渲染 | 检查显示环境、图形驱动、GPU 后端支持和用户窗口设置；设备要求见[构建指南](build-and-run.md#运行测试)。 |
| `STARTUP-WINDOW` | 启动窗口设置无法完整应用 | 按结果警告继续或终止初始化 | 阅读具体窗口操作及底层原因，区分可回退的设置与必需操作失败；不要仅凭同一诊断码判断是否退出。 |

这些提示来自 [Application 的启动流程](../../../engine/application/application.cpp)。它们不是项目素材解码错误；项目音频和字体文件的失败在内容加载阶段排查。

#### 基础资源与场景准备

| 错误码或日志关键词 | 常见原因 | 引擎处理 | 优先检查项 |
| --- | --- | --- | --- |
| `Development overlay creation failed`、`Development overlay initialization failed` | 模块覆盖层工厂或覆盖层初始化失败 | 终止初始化 | 检查 `create_development_overlay()`、实际启用的 ImGui 构建选项及自定义实现；接入方式见[开发面板](tools/development-panels.md)。 |
| `Built-in asset initialization failed`、`Built-in music player initialization failed` | 必需内置素材、字体或内置音乐播放设施不可用 | 终止初始化 | 按原始错误定位内置资源和运行依赖，不要把内置资源键补到项目清单中作为替代。 |
| `LOCALIZATION-MANIFEST`、`LOCALIZATION-LOCALE`、`LOCALIZATION-LANGUAGE` | 本地化清单、语言文件或语言配置不符合要求 | 终止初始化 | 从注册表的 `manifests.required.i18n` 检查语言清单与诊断指向的文件；不要把每一次语言回退都视为致命失败。 |
| `typography` 分类中的字体诊断 | 字体来源、字号或语言所需字体配置无效 | 终止初始化 | 检查 `descriptor.presentation.fonts` 和具体诊断；这个分类本身不是完整错误原因。项目字体的加载与激活还需检查下一张表。 |
| `BOOTSTRAP-PRELOAD` | 预加载清单或其纹理加载失败 | 终止初始化 | 从注册表的 `bootstrap.preload_manifest` 检查文件与路径；项目 Logo 必须引用预加载键，不能只放进后续项目纹理清单。 |

#### 项目内容加载

以下失败由启动加载场景处理；即使画面已经出现，也不能继续进入依赖未完成资源的游戏场景。

| 错误码或日志关键词 | 常见原因 | 引擎处理 | 优先检查项 |
| --- | --- | --- | --- |
| `CONTENT-CONFIG`、`CONTENT-MANIFEST` | 项目配置或内容清单无效 | 停止加载、清理项目内容，再走加载失败路由 | 从注册表引用追到诊断中的声明文件，按 JSON Pointer 检查字段；完整格式见[资源参考](../reference/resources/README.md)。 |
| `CONTENT-PLAN`，并含 `Duplicate` | 同一资源注册表生成了重复键 | 拒绝加载计划，走加载失败路由 | 同时查看首次和重复声明的位置，检查核心清单与附加模块、实体 ID 和命名空间；不同资源注册表可以使用同名键。 |
| `CONTENT-MISSING` | 请求生成后发现源文件缺失 | 停止加载，走加载失败路由 | 查看每项 `expected` 与 `declared_at`，检查模板展开后的目录、文件名、动画分段和帧编号，不只检查目录是否存在。 |
| `CONTENT-TEXTURE`、`CONTENT-ATLAS`、`CONTENT-FONT`、`CONTENT-AUDIO` | 文件读取、解码或对应运行资源创建失败 | 清理已加载项目内容，走加载失败路由 | 对照具体资源键、路径和底层原因检查文件格式及有效性；文件存在不代表能成功解码。 |
| `CONTENT-PLAN`、`CONTENT-ANIMATION`、`CONTENT-EFFECT` | 动画或特效的组装、校验、注册失败 | 停止加载，走加载失败路由 | 结合原因检查布局、帧数、分段、资源引用和特效映射；阶段码不能单独确定哪个字段错误，格式见[动画与特效参考](../reference/resources/animation-and-effects.md)。 |
| `Startup font activation failed` | 内容加载后，项目字体仍不能按配置激活 | 不执行成功切换，走加载失败路由 | 检查呈现配置要求的字体来源、资源键及字号与已加载字体是否匹配；不要仅用加载进度判断可进入游戏。 |

错误码与加载清理行为可对照 [ContentLoadFailure](../../../engine/loading/content_load_failure.h) 和 [GameContentLoader](../../../engine/loading/game_content_loader.cpp)。

#### 注册与进入目标场景

场景接入问题既可能发生在初始化末尾，也可能发生在加载完成后的成功切换中。

| 错误码或日志关键词 | 常见原因 | 引擎处理 | 优先检查项 |
| --- | --- | --- | --- |
| `Scene composition failed`，并含 `duplicate SceneKey`、`outside the game range` 或 `unregistered game SceneKey` | 重复注册、使用保留键，或初始目标未注册 | 初始化中的装配异常终止初始化 | 对照 `register_scenes()` 与 `initial_route`；游戏键使用 `1–999`，有效数字仍须注册。加载后才切换的未注册目标走运行期错误边界，不能依赖初始化提前发现。 |
| `StartupLoadingScene requires StartupLoadingScenePayload`、`StartupLoadingScene requires a valid success route`、`StartupLoadingScene failure route is invalid` | 加载场景 Payload 类型或路由无效 | 进入回调抛出接入错误，由场景错误边界处理 | 对照本文的路由示例，分别检查初始路由、成功路由和自定义失败路由。 |
| `scene` 分类中的原始异常 | 目标场景构造或 `on_enter()` 失败 | 按场景错误边界清理并尝试错误展示；无法恢复时故障退出 | 阅读异常原因及来源，检查 Payload、资源准备顺序和进入时的外部订阅；不要只看到错误场景就重新进入原场景。 |

### 7.3 理解引擎已经做了什么

**初始化失败**时，应用记录错误、尝试显示启动提示并关闭已建立的资源，`ELYSIA_INITIALIZE_APP` 返回 `false`；入口直接返回失败，不能继续调用 `ELYSIA_RUN_APP`。弹窗受环境和具体错误路径影响，不保证每次都能显示。

**启动内容加载失败**时，加载器停止后续工作并清理项目内容；启动加载场景优先请求 `failure_route`，未提供时使用内置 `ApplicationFailure`。自定义失败路由不会自动补充任意 Payload 的诊断字段。字体激活等展示失败同样转入失败流程，但不应假定每条失败路径都执行了相同的加载器回滚。具体行为见[启动加载场景](builtin-scenes/startup-loading.md#失败与重新进入)。

**场景进入异常**由场景管理器的错误边界处理；清理成功且错误展示可用时，应用可能进入错误场景，因此初始化成功也可能意味着已经进入错误处理流程。故障场景清理失败、错误场景再次失败或底层渲染无法继续时，不会无限重试。恢复到错误展示不等于恢复游戏状态，详见[应用错误场景](builtin-scenes/application-failure.md)。

**警告可以伴随成功启动**。例如，启动展示时发现可选项目 Logo 的键为空、纹理不可用，会记录警告并跳过；这不豁免预加载清单中的文件要求，已列入清单的文件加载失败仍会阻止初始化。必需的引擎启动 Logo 也不属于这项可选回退。

用户配置损坏后，成功归档并从有效副本恢复或按应用默认值重建，可能只留下 `BOOTSTRAP-USER-CONFIG` 警告。首次没有配置文件通常是正常创建，不是失败；无法读取、未来版本格式或恢复写回失败则可能阻止启动。保留原始诊断及恢复文件，按实际结果处理，不要把删除配置作为统一修复步骤。

### 7.4 读取完整诊断

默认文件日志位于**已识别项目根目录的 `logs/`**，每次运行创建类似 `Elysia-日期-时间-进程号.log` 的文件；并非固定在可执行文件旁。自定义 `LoggerConfig` 可以改变文件模式或禁用文件输出。

控制台日志先于文件日志建立。应用在启动配置解析成功后才初始化文件日志；早期目录或配置失败可能没有本次日志文件，文件打开失败也不会保证落盘。此时应从终端启动程序并保留控制台和启动提示；之后打开的文件日志不会补写早期控制台消息。Release 的启动提示和错误界面可能省略原始诊断，不能只凭界面短句定位问题。

下面是按诊断字段组织的**阅读示意**，不是实际运行记录；资源键、缺失文件及源码行号均为假设值：

```text
CONTENT-MISSING [startup]
某项纹理源文件缺失
[1] type=texture key=ui.example
    expected=assets/textures/ui/example.png
    declared_at=assets/configs/manifests/textures_manifest.json#/textures/ui.example/path
    reason=文件不存在
    source=resource_load_plan_preflight.cpp:123
```

先用错误码和分类确定加载阶段，再查看 `key` 对应哪个逻辑资源。`expected` 是需要存在的文件；`declared_at` 指向声明它的配置，`#` 后的 JSON Pointer 定位字段，示例表示 `textures` 对象中 `ui.example` 的 `path` 字段。`reason` 说明失败原因，`source` 是产生诊断的源码位置，不是要修改的 JSON 行号。重复键等问题会有多条诊断，应同时保留首次声明、重复声明及附加失败信息。

诊断字段并非每次都齐全，路径也可能按项目根目录缩短。向维护者报告时保留完整消息和相关条目，比只截取最后一行“启动失败”更有用。日志接口见[工具指南](tools/utilities.md#日志)，格式实现见 [FailureDiagnostic](../../../engine/core/diagnostics/failure_diagnostic.cpp)。

## 8. 完整参考实现

本文给出的空场景模块用于说明最小接线，不包含仓库示例游戏的全部配置。需要核对实际启动方式时，可从以下文件继续阅读：

| 文件 | 内容 |
| --- | --- |
| [main.cpp](../../../main.cpp) | 程序入口、运行结果与异常兜底 |
| [game_module.h](../../../engine/application/game_module.h) | 模块接口和应用描述结构 |
| [example_game_module.h](../../../game/application/example_game_module.h) | 示例模块的三个接口声明 |
| [example_game_module.cpp](../../../game/application/example_game_module.cpp) | 呈现配置、加载路由、场景注册和覆盖层工厂 |

完成模块接入后，可继续阅读 [GameplayScene 使用指南](gameplay/scene.md)，了解如何组织具体玩法场景。
