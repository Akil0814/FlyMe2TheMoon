# 启动加载场景

本文说明如何配置启动加载路线、项目标志与完成确认，并解释加载失败后的处理方式。展示完成与资源准备完成是独立条件，应按项目需要配置等待策略。

`StartupLoading` 负责进入游戏内容前的加载展示、项目 Logo 和完成确认。应用启动装配及内置资源准备仍由应用完成；它不是替代 `IGameModule` 的入口。项目资源清单格式见[资源加载文档](../content/resources.md)。

## 配置初始路由

以下函数放在游戏模块实现文件中，用其返回值设置 `ApplicationDescriptor::initial_route`。参数是已注册游戏场景的有效路由，例如主菜单；不要使用未注册的任意数字。

```cpp
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/startup_loading_scene_payload.h"

#include <utility>

elysia::scene::SceneRoute make_startup_route(
    elysia::scene::SceneRoute main_menu)
{
    elysia::builtin::StartupLoadingScenePayload payload;
    // 加载成功后交给调用方指定的主菜单；路由可携带其业务 Payload。
    payload.success_route = std::move(main_menu);
    // 本例只演示加载接入，不等待 Logo 播放或额外确认。
    payload.wait_for_logo_sequence = false;
    payload.wait_for_confirmation = false;
    return {
        .target = elysia::builtin::SceneKeys::StartupLoading,
        .payload = std::move(payload),
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    };
}
```

进入时必须携带 `StartupLoadingScenePayload`，且 `success_route` 有效；错误类型或无效路由会抛出接入错误。目标游戏场景还必须已注册，路由值合法不等于目标一定存在。

## Payload 字段

| 字段 | 用法 |
| --- | --- |
| `success_route` | 加载完成后进入的目标及其 Payload |
| `failure_route` | 可选的自定义失败路由；未提供时使用内置失败展示 |
| `project_logo` | 可选 `StartupLogoSlot`，包含纹理资源键及淡入、停留、淡出秒数 |
| `wait_for_logo_sequence` | 默认 `true`；设为 `false` 时资源加载完成后不再等待 Logo 序列结束 |
| `wait_for_confirmation` | 默认 `true`；完成加载后仍需用户确认，不能把“加载成功”当成已经切换 |

项目 Logo 使用预加载纹理的资源键，而非直接传入纹理文件路径；场景通过 `Bootstrapper` 查找预加载纹理，不能只把 Logo 放到后续内容加载清单。资源键为空或纹理不可用时会记录警告并跳过这个可选 Logo。

默认情况下，Logo 展示流程和内容加载都完成后才进入确认或成功切换阶段。将 `wait_for_logo_sequence` 设为 `false` 后，资源加载成功即可进入下一阶段：若 `wait_for_confirmation` 为 `true`，场景立即显示确认提示，Logo 动画可在后台继续播放；若两项都为 `false`，场景立即切换到 `success_route`。提前跳转不会绕过资源失败处理或项目字体激活。

## 失败与重新进入

内容加载失败会优先使用提供的 `failure_route`，否则构造内置 `ApplicationFailure` 路由携带诊断信息。自定义失败场景需要自行处理其路由 Payload；不能假设引擎自动为任意自定义 Payload 添加错误字段。

每次进入都会重新启动项目内容加载，并先清理旧项目内容。它不提供保留旧资源的热重载事务：失败后不能继续使用原来的资源指针。缓存游戏场景若仍持有旧纹理、图集或字体，必须先解除这些引用。开发时修改资源后重启应用更容易保证依赖完整。

项目预加载纹理在退出此场景时释放，再次进入也不会自动重新执行应用的预加载步骤。因此不要依赖重复进入仍能显示同一个项目 Logo。`content_registry.json` 仍使用应用启动时的快照；修改注册表需要重启应用。错误展示的能力边界见[应用错误场景](application-failure.md)。

## 加载失败的诊断

从启动提示定位配置、平台或资源问题，见[启动失败与排查](../game-initialization.md#7-启动失败与排查)；其中也说明了日志文件尚未建立时如何保留诊断。

内容准备线程的失败会交回主线程，停止后续加载并清理未完成资源，再按失败路由处理。游戏侧无需加入线程管理；应保留 `ContentLoadFailure` 中的阶段和资源诊断，定位文件、清单或加载步骤。加载器内部能够重置，不代表游戏里依赖旧资源的对象也自动恢复。

## 参考

- [Payload 定义](../../../../engine/builtin/scenes/startup_loading_scene_payload.h)
- [仓库游戏模块示例](../../../../game/application/example_game_module.cpp)
- [加载器实现](../../../../engine/loading/game_content_loader.cpp)
- [游戏初始化](../game-initialization.md)、[返回使用指南](../README.md)
