# 应用错误场景

本文说明何时可以显示内置错误界面、如何传递诊断，以及用户确认后的退出行为。它适用于游戏已无法继续、但窗口、渲染和必要内置资源仍可工作的情况。

`ApplicationFailure` 用于展示启动资源失败或运行时致命错误的诊断，并提供退出流程。它不是通用重试、回档或恢复游戏的机制；可恢复的业务失败应由游戏自己的流程处理。

如果是在启动时遇到错误，先按[启动失败与排查](../game-initialization.md#7-启动失败与排查)定位发生阶段、查找日志和检查项目配置；本文侧重错误场景的接入与能力边界。

## 在场景中请求错误展示

下面是一个不含业务状态的派生场景类型。仅从正常输入或更新阶段调用 `show_fatal_error()`；引擎及内置 UI 资源必须已可用。路由接收 `FailureDiagnostic`，字符串需显式转换为诊断。

```cpp
#include "engine/scene/scene.h"
#include "engine/builtin/scenes/application_failure_scene_payload.h"

class FailureExampleScene final : public elysia::scene::Scene
{
public:
    // 本例只演示失败路由，不创建额外业务对象。
    void on_enter(const elysia::scene::ScenePayload&) override
    {
    }

    void on_exit() override
    {
    }

    void on_reset() override
    {
    }

    void show_fatal_error()
    {
        // 仅用于渲染和内置资源仍可用的失败；保留完整诊断交给错误场景。
        request_scene_switch(elysia::builtin::make_application_failure_route(
            elysia::builtin::ApplicationFailurePresentation::RuntimeFatal,
            "gameplay",
            elysia::core::make_failure_diagnostic(
                "Required game state could not be restored.")));
    }
};
```

通过 `ContentLoadFailure` 的辅助函数重载可生成启动加载失败路由，保留错误类别、代码和诊断。不要直接传字符串代替 `ApplicationFailureScenePayload`。

## 诊断内容

Payload 包含 `presentation`、`reason`、`error_code`、`category` 和 `diagnostic`。`StartupLoading` 与 `RuntimeFatal` 是展示模式，决定对应的文案和表现，不表示任意两种错误都能够恢复。

场景进入时检查 Payload 类型及展示模式。它会停用项目字体，使用内置展示资源降低对已失败项目内容的依赖。诊断仍应提供可追踪的类别与原因，不能仅展示“失败了”而丢失定位信息。

传入完整的 `FailureDiagnostic`，保留消息、来源和附加错误；不要只截取一段消息再重新组装失败。引擎的场景边界错误可用 `make_application_failure_route(failure)` 转换，启动内容错误则使用对应的 `ContentLoadFailure` 重载。

界面呈现与完整日志是两层信息：Release 界面会隐藏原始诊断，确认退出前仍会记录完整诊断。排查失败时应查看日志，不能仅根据界面短提示判断原因。

## 退出与能力边界

用户确认退出会向终止管理器提交终止请求，由应用统一处理退出，不是返回之前的场景。场景自身没有成功返回路由，也没有通用“重新加载并继续游戏”的按钮。

只有渲染、窗口、必要内置资源和错误 UI 仍可工作时才能展示该界面。过早的初始化失败、错误界面构建失败或无法继续安全渲染的故障不能靠路由解决。也不要假定任意异常都会自动转成这个场景；应遵循应用已有错误边界，保留日志并处理最终退出结果。

不要在 `on_enter()`、`on_exit()`、`on_reset()` 或渲染阶段递归请求切换；也不要在已请求致命错误后继续提交依赖损坏状态的业务操作。

## 与场景异常恢复的区别

场景管理器可以在游戏逻辑异常后清理故障场景，再按失败路由进入另一个场景。这种“恢复”是恢复到可处理错误的流程，不是撤销伤害、恢复存档或继续原场景。

故障场景清理失败，或恢复场景也失败时，管理器进入 `Faulted`，不会递归创建更多恢复场景。`on_enter()` 抛异常不补调 `on_exit()`，游戏侧外部订阅和回调应通过 RAII 或局部回滚解除。完整生命周期见[场景系统](../scene/scene.md#进入退出失败与关闭顺序)。

## 底层渲染失败

SDL 渲染后端失败通过 `RenderBackendError` 交给应用，直接进入故障退出，不尝试依赖同一渲染器显示错误界面。应用会停止后续绘制并关闭资源；游戏不应捕获后继续提交原来的绘制命令。

普通游戏代码通过场景对象提交绘制命令，无需直接驱动底层执行器。维护自定义渲染代码时，应检查执行器返回的 `expected<void, RenderFailure>`，保留原始失败及状态恢复错误。相关定义见 [render_failure.h](../../../../engine/core/render/render_failure.h)。

## 参考

- [诊断 Payload 与辅助函数](../../../../engine/builtin/scenes/application_failure_scene_payload.h)
- [错误场景实现](../../../../engine/builtin/scenes/application_failure_scene.cpp)
- [启动加载](startup-loading.md)、[日志](../tools/utilities.md)、[返回使用指南](../README.md)
