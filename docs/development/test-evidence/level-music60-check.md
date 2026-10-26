# 60 关实际音乐核验

2026-10-26，Debug 构建。结果：**60/60 关匹配原作，错误 0 关**。

独立执行原作 `Game::nameMusic` 的 ARM 指令取得预期曲目，逐关比对原始 XML 和项目套组数据；随后逐个进入真实 `FlightScene`，检查 SDL mixer 当前资源、播放帧推进及退出停止。四首关卡曲目解码后的 PCM 与原始 CAF 一致。使用 SDL dummy 音频设备，未进行人工听音。

两项专项测试通过：`fly_original_music`（3.59 秒）、`fly_level_music_playback`（16.21 秒）。复查命令：`ctest --test-dir out/build/x64-Debug -R "^(fly_original_music|fly_level_music_playback)$" --output-on-failure`。

| 曲目 | 关卡数 |
| --- | --- |
| `SymphonyofRush` | 23 |
| `Hectic` | 13 |
| `LifeforSpeed` | 16 |
| `SpiralofAdventure` | 8 |

| 关卡 | 原作曲目 | 实际播放 | 结果 |
| --- | --- | --- | --- |
| 1 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 2 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 3 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 4 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 5 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 6 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 7 | `Hectic` | `Hectic` | 通过 |
| 8 | `Hectic` | `Hectic` | 通过 |
| 9 | `Hectic` | `Hectic` | 通过 |
| 10 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 11 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 12 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 13 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 14 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 15 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 16 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 17 | `Hectic` | `Hectic` | 通过 |
| 18 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 19 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 20 | `SpiralofAdventure` | `SpiralofAdventure` | 通过 |
| 21 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 22 | `SpiralofAdventure` | `SpiralofAdventure` | 通过 |
| 23 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 24 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 25 | `Hectic` | `Hectic` | 通过 |
| 26 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 27 | `SpiralofAdventure` | `SpiralofAdventure` | 通过 |
| 28 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 29 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 30 | `Hectic` | `Hectic` | 通过 |
| 31 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 32 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 33 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 34 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 35 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 36 | `SpiralofAdventure` | `SpiralofAdventure` | 通过 |
| 37 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 38 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 39 | `Hectic` | `Hectic` | 通过 |
| 40 | `SpiralofAdventure` | `SpiralofAdventure` | 通过 |
| 41 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 42 | `Hectic` | `Hectic` | 通过 |
| 43 | `SpiralofAdventure` | `SpiralofAdventure` | 通过 |
| 44 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 45 | `Hectic` | `Hectic` | 通过 |
| 46 | `Hectic` | `Hectic` | 通过 |
| 47 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 48 | `Hectic` | `Hectic` | 通过 |
| 49 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 50 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 51 | `Hectic` | `Hectic` | 通过 |
| 52 | `SpiralofAdventure` | `SpiralofAdventure` | 通过 |
| 53 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 54 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 55 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 56 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 57 | `SymphonyofRush` | `SymphonyofRush` | 通过 |
| 58 | `LifeforSpeed` | `LifeforSpeed` | 通过 |
| 59 | `SpiralofAdventure` | `SpiralofAdventure` | 通过 |
| 60 | `Hectic` | `Hectic` | 通过 |

完整数据及哈希见 `level-music60-check.json`，专项测试日志见 `level-music60-ctest.log`。

另：首次完整视觉测试中，音乐核验全部通过，随后主菜单背景检查因新增退出确认层导致旧的两个子节点断言失败；原始日志保留在 `level-music60-initial-ctest.log`。本次未修改该断言，音乐检查已独立注册。
