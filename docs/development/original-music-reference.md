# 原作 iOS 1.1 音乐对应表

本表只描述仓库内原作，不描述复刻项目的音乐配置。依据原始可执行文件的静态反汇编与 60 个原始关卡文件，未运行原始 iOS 程序。假定音乐选项开启。

## 场景及播放行为

| 场景/状态 | 原作背景音乐及行为 |
| --- | --- |
| 启动 Logo | 未发现背景音乐播放调用；miHoYo Logo 的加载过程预加载 SkyTunnel。 |
| 标题/点击开始画面 | SkyTunnel；标题属于 MainMenuLayer。 |
| 主菜单 | SkyTunnel。 |
| 设置 | SkyTunnel。 |
| 关卡选择 | SkyTunnel。 |
| 成就、排行榜 | Game Center 模态界面，不请求新曲目，沿用进入前音乐，通常为 SkyTunnel。 |
| 重置进度确认 | 不请求新曲目，通常沿用设置的 SkyTunnel。 |
| 制作人员 | MoonTrip；返回设置切为 SkyTunnel。 |
| 关卡加载、重新加载 | LoadingScene loadStep0 停止背景音乐。 |
| 关卡实际开始 | Game doStartGame 根据 nameMusic 的返回值切换曲目，见逐关表。 |
| 关内剧情/教程/演示 | 不请求新背景曲目，沿用该关配乐。 |
| 暂停菜单 | HUDLayer doPause 暂停背景音乐；doResume 从当前位置恢复。 |
| 通关、失败结算 | HUDLayer doEndGame 暂停场景音效并展示结算，不切换/暂停背景曲目，继续该关配乐。 |

同曲请求不会重新播放：GameAudioManager switchToMusic: 先比较当前曲名，名字相同时直接返回。因此主菜单、设置、选关之间连续播放 SkyTunnel；进入制作人员会换 MoonTrip，返回设置再换回 SkyTunnel。

## 60 关逐关配乐

关卡编号取 Level_XXX 文件名；部分文件的 Map ID 是不同的内部编号，不能用于展示关卡序号。Game nameMusic 的规则为：关卡编号小于 4 → SymphonyofRush；其他关卡按 Header/Suite：1 → LifeforSpeed，2 → Hectic，3 → SpiralofAdventure，其他 → SymphonyofRush。

| 关卡 | 配乐 | 关卡 | 配乐 |
| --- | --- | --- | --- |
| 1 | `SymphonyofRush` | 31 | `SymphonyofRush` |
| 2 | `SymphonyofRush` | 32 | `SymphonyofRush` |
| 3 | `SymphonyofRush` | 33 | `LifeforSpeed` |
| 4 | `SymphonyofRush` | 34 | `SymphonyofRush` |
| 5 | `SymphonyofRush` | 35 | `LifeforSpeed` |
| 6 | `SymphonyofRush` | 36 | `SpiralofAdventure` |
| 7 | `Hectic` | 37 | `SymphonyofRush` |
| 8 | `Hectic` | 38 | `LifeforSpeed` |
| 9 | `Hectic` | 39 | `Hectic` |
| 10 | `SymphonyofRush` | 40 | `SpiralofAdventure` |
| 11 | `LifeforSpeed` | 41 | `LifeforSpeed` |
| 12 | `SymphonyofRush` | 42 | `Hectic` |
| 13 | `SymphonyofRush` | 43 | `SpiralofAdventure` |
| 14 | `LifeforSpeed` | 44 | `SymphonyofRush` |
| 15 | `LifeforSpeed` | 45 | `Hectic` |
| 16 | `SymphonyofRush` | 46 | `Hectic` |
| 17 | `Hectic` | 47 | `LifeforSpeed` |
| 18 | `LifeforSpeed` | 48 | `Hectic` |
| 19 | `LifeforSpeed` | 49 | `SymphonyofRush` |
| 20 | `SpiralofAdventure` | 50 | `LifeforSpeed` |
| 21 | `SymphonyofRush` | 51 | `Hectic` |
| 22 | `SpiralofAdventure` | 52 | `SpiralofAdventure` |
| 23 | `SymphonyofRush` | 53 | `LifeforSpeed` |
| 24 | `SymphonyofRush` | 54 | `SymphonyofRush` |
| 25 | `Hectic` | 55 | `SymphonyofRush` |
| 26 | `LifeforSpeed` | 56 | `LifeforSpeed` |
| 27 | `SpiralofAdventure` | 57 | `SymphonyofRush` |
| 28 | `LifeforSpeed` | 58 | `LifeforSpeed` |
| 29 | `SymphonyofRush` | 59 | `SpiralofAdventure` |
| 30 | `Hectic` | 60 | `Hectic` |

合计：SymphonyofRush 23 关、LifeforSpeed 16 关、Hectic 13 关、SpiralofAdventure 8 关。SkyTunnel 和 MoonTrip 不在原作关卡选曲分支中。

## 静态证据

- 原始可执行文件 SHA256：`c7047ffe5e21be1373498787131a488ff7e65d6900352c2fa04e132a05265c83`。
- `Game nameMusic`：`0xa3ebc`；`Game doStartGame`：`0xa44c0`。
- 曲名全局常量：`0x1f4d60` SkyTunnel，`0x1f4d64` LifeforSpeed，`0x1f4d68` Hectic，`0x1f4d6c` SpiralofAdventure，`0x1f4d70` SymphonyofRush，`0x1f4d74` MoonTrip。
- `MainMenuLayer initMusic`：`0xdeb64`；`OptionsLayer initMusic`：`0xe1668`；`LevelSelectLayerV2 initMusic`：`0xda7f8`；`CreditsScene initMusic`：`0xd2fd0`。
- `GameAudioManager switchToMusic:`：`0x9319c`；`LoadingScene loadStep0`：`0xddbf8`；`HUDLayer doPause`：`0xe3e6c`；`HUDLayer doResume`：`0xe3f48`；`HUDLayer doEndGame`：`0xe4004`。
- 复查命令示例：`python game/tools/inspect_original.py --class-name Game --method nameMusic`；其他类和方法同理。
- 本次复查扫描了 2105 个游戏及应用方法的音乐相关调用。结果缓存于 `out/analysis/original-music-calls.json`，反汇编缓存于同目录；源数据始终为 `reference/Fly2theMoon_1.1_ios_3.2/`。
