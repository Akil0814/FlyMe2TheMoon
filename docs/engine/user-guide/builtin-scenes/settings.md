# 设置场景

本文说明如何从游戏打开内置设置场景，以及草稿、实际运行状态和磁盘设置如何分开管理。返回路由决定游戏场景恢复方式，保存结果则决定哪些设置真正生效。

内置 `Settings` 场景提供窗口、帧率、垂直同步、音量和语言设置。应用需要已经初始化 `UserConfigService`，场景必须收到有效的 `SettingsScenePayload`。它是切换到的独立场景，不是自动叠加在当前游戏画面上的暂停面板。

## 从游戏场景打开

下面用一个空场景展示打开设置的方法。把 `open_settings()` 放入自己的派生场景，从按钮回调或正常输入、更新阶段调用；`return_route` 指向已经注册的返回场景，必要时携带它自己的 Payload。

```cpp
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/settings_scene_payload.h"
#include "engine/scene/scene.h"

#include <utility>

class SettingsExampleScene final : public elysia::scene::Scene
{
public:
    void open_settings(elysia::scene::SceneRoute return_route)
    {
        elysia::builtin::SettingsScenePayload payload;
        // 返回路由连同业务 Payload 一起移入，保留调用场景的恢复方式。
        payload.return_route = std::move(return_route);
        payload.visibility.target_fps = false;
        // 这里只发出路由请求，实际切换由场景管理器在安全阶段执行。
        request_scene_switch(elysia::scene::SceneRoute{
            .target = elysia::builtin::SceneKeys::Settings,
            .payload = std::move(payload),
            .reload_mode = elysia::scene::SceneReloadMode::Reuse
        });
    }

protected:
    // 空场景只演示打开设置，不持有需要初始化或清理的业务对象。
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
```

`request_scene_switch` 是场景的受保护接口，不能从普通自由函数调用。不要在 `on_enter()`、`on_exit()`、`on_reset()` 或渲染回调中请求场景切换。缺少正确 Payload、返回路由无效或用户配置服务未初始化，都会导致接入错误。

## 可见项与保存

`visibility` 的 `window_mode`、`target_fps`、`vsync`、`master_volume`、`music_volume`、`sound_volume`、`language` 默认均为 `true`。它们控制界面提供哪些项目，不是新增应用能力，也不是修改配置默认值的接口。

面板维护草稿；改变控件不会自动等同于已保存。点击保存时，场景从当前配置快照组装请求，并调用 `apply_and_save_user_config` 应用及持久化。

显示、音频和通用设置位于同一可滚动区域，标题、状态消息及 Save/Back 操作保持固定。内容超过可用高度时向下滚动查看，不需要切换页签。

- 成功：刷新运行状态基线与面板草稿，显示已保存或需要重启的提示；不会自动返回游戏。
- 失败：显示本地化的简短失败提示，完整原因写入日志。服务会尝试回滚；场景以操作结束后的实际运行快照刷新草稿，不能假定所有设置都维持请求值或全部恢复旧值。
- 返回或取消：放弃尚未保存的草稿，使用 `return_route` 离开；不会撤销此前已经成功保存的设置。

语言候选来自本地化服务支持列表，窗口尺寸候选参考显示器可用区域。不要把隐藏某项理解为删除该设置，也不要假定所有设置都能立即无条件生效。

FPS 上限、音量和语言等设置可在运行时应用；VSync 保存后需要重启。把 VSync 改回本次启动采用的值会取消待重启状态。设置的文件格式和单项修改接口见[配置读取与用户设置](../content/configuration.md)。

## 返回与生命周期

返回路由的 Reuse/Reset/Recreate 决定目标场景如何恢复。希望保留关卡状态时，不要误用 Recreate。离开设置场景会取消面板与窗口关联并隐藏停用 UI；缓存实例可再次使用。游戏自己的输入绑定与重新进入逻辑仍由游戏负责。

## 保存失败时如何处理

窗口修改、运行时设置应用与磁盘保存都可能失败。引擎会尝试恢复操作前的状态，但恢复也可能失败；不要在调用失败后直接把草稿当作实际状态。内置页面已负责重新读取状态，自定义页面应采用相同做法。

`UserConfigCommitFailure::cause` 保留原始失败，`rollback_failure` 描述运行时回滚失败，可选的 `persistence` 描述磁盘恢复。磁盘恢复成功不代表窗口、音量等运行状态也恢复成功。需要自定义日志时，通过 `to_failure_diagnostic()` 汇总诊断，示例见[配置指南](../content/configuration.md#修改并保存玩家设置)。

文件访问失败不会被当作文件损坏而覆盖默认值；无法安全恢复的临时副本也会保留。调用方应提示失败并保留诊断，再决定是否重试。非预期异常仍可能传播到应用错误边界，不能把 `expected` 理解为所有失败都已被吞掉。

## 参考

- [Payload](../../../../engine/builtin/scenes/settings_scene_payload.h)、[可见项与草稿](../../../../engine/ui/presets/settings_panel.h)
- [运行时配置](../content/configuration.md)、[本地化](../systems/localization.md)
- [提交与回滚实现](../../../../engine/config/user_config_service.cpp)、[返回使用指南](../README.md)
