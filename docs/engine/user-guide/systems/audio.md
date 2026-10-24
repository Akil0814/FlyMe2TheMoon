# 音频播放

本文说明如何查询并播放项目音频，以及如何管理播放句柄、延迟请求和音量。资源是否存在、请求是否被接受和声音是否正在播放是不同状态，下面分别介绍。

使用 `ELYSIA_AUDIO` 播放已加载音效和音乐。应用负责初始化、逐帧更新和关闭；游戏不要重复调用这些生命周期接口。先在[资源清单](../content/resources.md)中声明 Sound/Music，等待内容加载成功。

## 播放并管理一个音效

以下函数放在游戏代码中，在输入处理或游戏事件中调用。示例资源键需在音频资源清单中配置；将返回句柄保存为所属技能或场景的成员。

```cpp
#include "engine/audio/audio_service.h"
#include "engine/tools/logger.h"

#include <optional>
#include <chrono>

std::optional<elysia::audio::SoundHandle> play_explosion()
{
    using namespace std::chrono_literals;
    // 预约由应用的音频调度推进；返回句柄不代表已经发声。
    const elysia::audio::SoundRequestResult result = ELYSIA_AUDIO->request_sound("skill.explosion", {
        .group = elysia::audio::SoundGroup::Gameplay,
        .start_delay = 300ms
    });
    if (result.status == elysia::audio::SoundRequestStatus::Rejected)
    {
        ELYSIA_LOG_WARN("audio", "Explosion sound rejected.");
        return std::nullopt;
    }
    // 调用方保存句柄，在技能取消或退出场景时显式取消预约/停止播放。
    return result.handle;
}

// 技能取消、场景退出或所有者销毁前调用。
void cancel_sound(std::optional<elysia::audio::SoundHandle>& handle)
{
    if (!handle)
        return;
    // false 也可能表示已经自然结束，无需重复停止。
    (void)ELYSIA_AUDIO->stop_sound(*handle);
    handle.reset();
}
```

服务不按场景或对象自动清理播放请求。句柄不延长资源或对象生命；Scheduled 只是预约成功，不保证到期一定播放。

## 播放 UI 提示与配置组规则

下面的局部片段位于已初始化的游戏逻辑中，需包含音频服务和日志头文件。`audio` 是 `elysia::audio::AudioService::instance()` 返回的借用指针；所有毫秒字面量均使用 `std::chrono_literals`。示例键需由项目的音频清单提供。

```cpp
const elysia::audio::SoundRequestResult click = audio->request_sound("ui.button_click", {
    .group = elysia::audio::SoundGroup::Ui
});
if (click.status == elysia::audio::SoundRequestStatus::Rejected)
{
    // 提示音失败不阻止 UI 操作；按项目日志策略记录即可。
    ELYSIA_LOG_WARN("audio", "UI click sound rejected.");
}
```

高频技能音效可选择替换本组最早播放的声音。先设置规则，再发起播放请求；配置失败时保留旧规则。

```cpp
elysia::audio::SoundGroupConfig gameplay_config{};
gameplay_config.max_simultaneous = 8;
gameplay_config.cooldown = 20ms;
gameplay_config.overflow_policy = elysia::audio::SoundOverflowPolicy::ReplaceOldest;
if (!audio->set_sound_group_config(elysia::audio::SoundGroup::Gameplay, gameplay_config))
{
    ELYSIA_LOG_WARN("audio", "Gameplay sound configuration rejected.");
}
```

各组硬上限、按资源键计算的冷却和替换边界见[并发规则](../../reference/audio.md#并发组冷却与溢出策略)。不要用无限重试来绕过播放拒绝。

## 调整环境音与切换音乐

只降低环境音、不影响音乐和技能声音时，使用组音量。以下局部片段沿用上一节的 `audio`：

```cpp
audio->set_sound_group_volume(elysia::audio::SoundGroup::Ambient, 45);
```

组音量只存在于运行时；保存玩家偏好应走[用户配置](../content/configuration.md)。音量范围与最终增益见[音量模型](../../reference/audio.md#音量模型)。

以下片段演示先淡出旧音乐再淡入战斗音乐，并开启循环雨声。它位于宿主的进入逻辑中，`rain_handle` 是宿主保存的 `std::optional<elysia::audio::SoundHandle>` 成员，初始为空；再次进入前必须先清理上次句柄。

```cpp
if (!audio->transition_music("bgm.battle", {
        .fade_out = 800ms,
        .fade_in = 500ms}))
{
    // 校验失败会保留原状态；若同步启动失败，旧音乐可能已经停止。
    ELYSIA_LOG_WARN("audio", "Battle music transition rejected.");
}
const elysia::audio::SoundRequestResult rain = audio->request_sound("ambient.rain", {
    .loops = -1,
    .group = elysia::audio::SoundGroup::Ambient,
    .fade_in = 1000ms
});
rain_handle = rain.handle;
if (rain.status == elysia::audio::SoundRequestStatus::Rejected)
{
    ELYSIA_LOG_WARN("audio", "Rain sound rejected.");
}
```

离开这段流程时执行下面的片段。渐出请求提交后不必继续持有句柄；这也避免下次进入误用旧标识。

```cpp
if (rain_handle)
{
    (void)audio->stop_sound(*rain_handle, 800ms);
    rain_handle.reset();
}
```

音乐切换返回成功只代表请求被接受，未来启动仍可能失败；完整规则见[音乐接口](../../reference/audio.md#音乐-api)。若业务需要两首音乐同时交叉淡化，当前单路音乐接口不提供这项能力。

## 清理与排查失败

音频调度由应用在场景更新后推进，延迟、冷却和渐变受全局时间倍率影响；场景暂停不会自动暂停全局声音。慢动作也不会自动改变已经启动的音轨的音高或播放速度。

先检查服务初始化和资源键，再区分请求被拒绝、预约已接受和实际开始播放。预约到期仍可能因冷却、并发或底层播放失败而被丢弃。不要把播放返回值当作声音已播放完成的通知。

场景退出、技能取消或宿主销毁前，停止自己保存的句柄。需要清空全局音效时，要分别处理活跃声音和待播放队列；两者的接口区别见[停止与取消](../../reference/audio.md#cancel_all_scheduled_sounds)。

## 继续阅读

- [音频接口与播放契约](../../reference/audio.md)：完整参数、结果和能力边界。
- [音频架构](../../architecture/subsystems/audio.md)：调度时序、轨道后端与渐变流程。
- [资源加载](../content/resources.md)、[返回使用指南](../README.md)。
