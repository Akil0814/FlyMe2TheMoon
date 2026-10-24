# 音频接口与播放契约

本页集中维护 `AudioService` 的参数、结果、句柄、并发规则和音量契约。接入步骤与示例见[音频指南](../user-guide/systems/audio.md)，服务与调度器的内部协作见[音频架构](../architecture/subsystems/audio.md)。

实际定义见 [audio_service.h](../../../engine/audio/audio_service.h)、[sound_playback_types.h](../../../engine/audio/sound_playback_types.h)、[audio_settings.h](../../../engine/audio/audio_settings.h) 和 [music_playback_controller.h](../../../engine/audio/music_playback_controller.h)。

## 音效播放与 SoundHandle

以下签名和结构只摘录与调用有关的成员，省略命名空间、部分默认初始化和其他声明；用于查阅参数，不要复制为项目中的同名类型。实际定义见本页开头的头文件链接。

### `play_sound`

```cpp
bool play_sound(const std::string_view& key, int loops = 0);
```

这是简化入口：立即请求播放，默认归入 `SoundGroup::Extra`，只返回是否已经实际开始播放。

- `loops = 0`：播放一次。
- `loops > 0`：额外循环次数，实际播放次数为 `loops + 1`。
- `loops = -1`：SDL_mixer 的无限循环语义。

它不返回句柄，因此不适合需要稍后精确停止的持续音效。

### `request_sound`

```cpp
SoundRequestResult request_sound(
    const std::string_view& key,
    const SoundPlayOptions& options = {});
```

这是推荐的音效播放入口。它先验证资源键，再按 `options` 决定立即播放或创建延迟请求。

```cpp
struct SoundPlayOptions
{
    std::optional<int> loops;                 // nullopt 等同一次播放
    SoundGroup group = SoundGroup::Extra;
    std::chrono::milliseconds start_delay{0};
    std::chrono::milliseconds fade_in{0};
};
```

返回值：

```cpp
enum class SoundRequestStatus
{
    Started,
    Scheduled,
    Rejected
};

struct SoundRequestResult
{
    SoundRequestStatus status;
    std::optional<SoundHandle> handle;
};
```

- `Started`：逻辑通道已经开始播放，`handle` 指向活跃实例。
- `Scheduled`：请求已进入延迟队列，`handle` 指向待播放实例；到期时仍会检查冷却和并发限制。
- `Rejected`：服务未初始化、资源不存在、即时请求触发冷却/并发限制，或底层播放失败；没有句柄。

延迟请求到期时若并发已满、正在冷却或底层播放失败，会被丢弃，不排队、不重试。

### `stop_sound`

```cpp
bool stop_sound(SoundHandle handle, std::chrono::milliseconds fade_out = {});
```

统一停止或取消一个请求：

- 句柄仍处于延迟状态：取消待播放请求，返回 `true`。
- 句柄已处于播放状态：接受停止请求，返回 `true`。正时长从当前增益渐出，到零后停止逻辑通道并移除活跃记录；期间句柄有效且仍占并发名额。
- 渐入中停止会从当前增益渐出；重复渐出请求不重启计时，非正时长立即停止。自然结束会提前清理。
- 句柄已自然结束、已被 `ReplaceOldest` 替换、无效，或服务未初始化：返回 `false`。

### `cancel_all_scheduled_sounds`

```cpp
void cancel_all_scheduled_sounds();
```

仅清空尚未开始的延迟请求，不停止已经在播放的音效。与之对应：

```cpp
void stop_all_sounds(std::chrono::milliseconds fade_out = {});
```

仅停止或渐出已经开始的全部音效，不取消延迟请求。时间参数使用毫秒，默认零；非正时长立即完成。

## 并发组、冷却与溢出策略

每个音效请求属于一个固定 `SoundGroup`：

| 组 | 固定硬上限 | 典型用途 |
| --- | ---: | --- |
| `Ui` | 4 | 焦点、点击、选择、菜单反馈 |
| `Gameplay` | 12 | 角色、技能、受击、脚步等玩法音效 |
| `Ambient` | 4 | 环境与场景持续音 |
| `Extra` | 4 | 暂未分类或通用音效 |

所有组共享 24 个音效播放槽，音乐不占用这些槽位。四组硬上限合计为 24，因此一个组即使看到其他组空闲，也不能超过自身硬上限。

组规则通过以下类型配置：

```cpp
struct SoundGroupConfig
{
    std::optional<std::size_t> max_simultaneous;
    std::chrono::milliseconds cooldown{0};
    SoundOverflowPolicy overflow_policy = SoundOverflowPolicy::IgnoreNew;
};
```

```cpp
enum class SoundOverflowPolicy
{
    IgnoreNew,
    ReplaceOldest,
};
```

### 规则含义

- `max_simultaneous = std::nullopt`：使用上表固定硬上限。
- 设置具体上限时只能下调，不能超过该组硬上限；允许设置为 `0`。
- `cooldown` 按**音效资源键**记录最近播放时间，并使用当前请求组的冷却时长判断。不同资源键互不影响；同一资源键即使换组也共享最近播放时间。只有成功开始播放才会刷新记录。
- `IgnoreNew`：组已满时拒绝新请求，默认策略，适用于 UI 与环境音。
- `ReplaceOldest`：组已满时停止本组最早开始的活跃声音，再启动新声音；被替换声音的 `SoundHandle` 会失效。它不会停止其他组的声音。
- 若目标组未满但全局 24 逻辑通道已满，仍直接拒绝；当前没有跨组优先级或抢占。

运行中下调上限不会主动停止已有声音。`IgnoreNew` 会在达到上限后拒绝新请求；`ReplaceOldest` 每次只替换一个活跃声音，因此已有数量高于新上限时不会立即收敛。特别是把上限设为 `0` 也不能替代禁用逻辑：空组会拒绝请求，但已有活跃声音时，替换策略仍可能启动新声音。

公开配置 API：

```cpp
bool set_sound_group_config(SoundGroup group, const SoundGroupConfig& config);
const SoundGroupConfig& sound_group_config(SoundGroup group) const;
```

当 `max_simultaneous` 超过该组硬上限，或 `cooldown` 为负数时，`set_sound_group_config()` 返回 `false` 且不更新旧配置。

## 音量模型

### 全局与音乐音量

`AudioSettings` 包含：

```cpp
struct AudioSettings
{
    int master_volume = 100;
    int music_volume = 100;
    int sound_volume = 100;
};
```

可在运行时修改：

```cpp
void set_master_volume(int volume);
void set_music_volume(int volume);
void set_sound_volume(int volume);
const AudioSettings& settings() const;
```

所有写入值都会钳制到 `0..100`。

音乐有效音量为：

```text
music_gain = master_volume / 100 × music_volume / 100 × music_fade_gain
```

### 组音量

四个并发组各自拥有不持久化的运行时音量：

```cpp
void set_sound_group_volume(SoundGroup group, int volume);
int sound_group_volume(SoundGroup group) const;
```

- 输入值同样钳制到 `0..100`。
- 新启动的逻辑通道会立即使用该组有效音量。
- 修改 master、sound 或任一组音量时，该组所有仍活跃的逻辑通道会立即更新。
- 组音量不进入 `AudioSettings`、用户配置或资源清单；下一次初始化会恢复四组为 `100`。

音效逻辑通道的有效音量为：

```text
sound_gain = master_volume / 100 × sound_volume / 100 × group_volume / 100 × instance_fade_gain
```

以上除法按浮点数计算，最终通过 `MIX_SetTrackGain` 设置轨道增益，没有乘以整数音量上限或向下取整的步骤。渐变增益是独立的 `0..1` 线性系数；渐变过程中修改音量立即生效，渐变本身不修改音量设置。播放前先设置初始增益，避免渐入首帧满音量。

`music_volume` 不影响音效；四组 `group_volume` 也不影响音乐。

## 音乐 API

```cpp
bool play_music(const std::string_view& key, int loops = -1,
    std::chrono::milliseconds fade_in = {});
void stop_music(std::chrono::milliseconds fade_out = {});

struct MusicTransitionOptions
{
    int loops = -1;
    std::chrono::milliseconds fade_out{0};
    std::chrono::milliseconds fade_in{0};
};
bool transition_music(const std::string_view& key,
    const MusicTransitionOptions& options = {});
```

- `play_music()` 校验资源后立即替换当前音乐、清除待切换目标，并让新音乐按指定时长渐入；同一资源键也从头播放。
- `transition_music()` 执行“旧音乐渐出 → 启动新音乐 → 新音乐渐入”。无音乐时直接启动新音乐；旧音乐自然结束时，在下一次更新启动目标。
- 仅保存一个待播放目标。渐出期间收到新目标，更新资源键、loops 和渐入时长，保留旧音乐渐出进度与结束时间。
- 请求当前仍在播放的同一资源键时，取消待切换目标、保持播放位置与 loops，并按本次渐入时长恢复增益到 1；满音量时继续播放。
- `stop_music()` 清除待切换目标，再从当前增益渐出。重复渐出不重启计时；非正时长立即停止。
- 音乐资源键必须已加载。服务未初始化或资源无效时返回 `false`，不改变当前播放或已接受的切换。默认 `loops = -1` 为持续循环。
- `transition_music()` 返回 `true` 表示请求已接受，不保证未来启动成功。待播放目标持有资源键，启动时再次查询资源；资源消失或 SDL 启动失败时记录日志、清空目标并进入空闲，不自动重试。同步启动失败返回 `false`。
- 音乐不参与音效组、冷却、延迟请求、句柄或 24 个音效逻辑通道的调度。本功能是单路顺序切换，不是两首音乐同时播放的交叉淡化。

## 生命周期与借用结果

`shutdown()` 立即停止所有播放并清空延迟、切换与渐变状态，不等待渐出；重复初始化先清理上一轮播放状态。

`initialize`、`update` 和 `shutdown` 由应用驱动，游戏代码在初始化与内容加载成功后调用公开播放接口。`is_initialized()` 可查询服务状态。`SoundHandle` 是 `std::uint64_t` 标识，不持有声音、资源或场景对象；服务关闭或重新初始化后应丢弃旧句柄。

`settings()` 与 `sound_group_config()` 返回服务内部配置的只读借用引用；如需保存配置快照，应复制值。音量 setter 修改当前运行状态，持久化由[用户配置](../user-guide/content/configuration.md)处理。

## 当前能力边界

当前模块**不提供**：

- 满槽后等待、自动重试或顺序播放队列。
- 跨组优先级与跨组逻辑通道抢占。
- 自定义动态音效组。
- 单实例暂停/恢复、声像、音高或随机变体。
- 多路音乐交叉淡化、音乐排队、组音量渐变，以及内置启动画面播放器的渐变扩展。
- 组音量的用户设置持久化。
- 自动按场景、实体或技能批量取消延迟请求。

这些能力应在出现明确玩法或产品需求时单独设计，避免把资源加载、场景生命周期和运行时播放调度重新耦合。
