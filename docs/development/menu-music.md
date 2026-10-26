# 菜单音乐连续播放

2026-10-26：按需求让项目现有的 `fly.MoonTrip` 在主菜单、设置、成就、选关、制作人员界面间连续播放，返回主菜单保留播放位置。进入关卡按原作套组选择音乐；从关卡返回选关时重新启动菜单音乐。

## 原作核对

依据仓库内 iOS 1.1 可执行文件的静态反汇编，未运行原程序。完整 Mach-O 文件 SHA256：`c7047ffe5e21be1373498787131a488ff7e65d6900352c2fa04e132a05265c83`。通过 `python game/tools/inspect_original.py --class-name <类名> --method <方法名>` 可复查以下地址。

| 方法 | 地址 | 行为 |
| --- | --- | --- |
| `GameAudioManager switchToMusic:` | `0x9319c` | 比较 `currentMusicName_` 与请求曲名；`0x931cc` 调用 `isEqualToString:`，相同时在 `0x931d8` 直接跳到返回，绕过播放调用。 |
| `MainMenuLayer initMusic` | `0xdeb64` | 请求 `SkyTunnel`。 |
| `OptionsLayer initMusic` | `0xe1668` | 请求 `SkyTunnel`。 |
| `LevelSelectLayerV2 initMusic` | `0xda7f8` | 请求 `SkyTunnel`。 |
| `CreditsScene initMusic` | `0xd2fd0` | 请求 `MoonTrip`，与其他菜单曲目不同。 |
| `MainMenuLayer onAchievements:` | `0xde9fc` | 调用 `GameCenterManager showAchievements`。 |
| `GameCenterManager showAchievements` | `0xf2b88` | 展示成就模态界面，没有音乐切换调用。 |

上述四个菜单类的 `dealloc` 中均未发现停止/暂停音乐调用。因此原作支持同曲连续播放，但制作人员界面会换曲，不能将五个界面共用一曲描述为完全复刻原作。本项目保留用户要求的五界面共用 `MoonTrip`。

## 实现和验证

各菜单场景进入时调用引擎已有的 `transition_music`：相同曲目继续播放，无音乐或曲目不同时启动菜单曲目。主菜单和制作人员场景退出时只清理界面，不停止全局音乐。关卡退出仍停止关卡音乐，重试仍重新开始配乐。

`game/tests/visual_tests.cpp` 使用 SDL dummy 音频后端加载真实音乐，将播放位置设为 240000 帧，逐个进入/退出各菜单并返回主菜单，检查曲目、播放状态和位置不回退；重复选关/成就进入以覆盖分页重建，检查静音/恢复音量，以及每个界面在无音乐或关卡音乐之后恢复菜单曲目。存档使用 `out/qa/menu-music/` 下的独立目录。

验证命令：`ctest --test-dir out/build/x64-Debug -R "^(fly_idle_visuals|fly_selection_input)$" --output-on-failure`。

结果：Debug 游戏及相关测试目标编译成功；两项测试均通过（`fly_idle_visuals` 77.72 秒，`fly_selection_input` 0.45 秒）。音乐检查使用 dummy 音频设备验证真实播放状态和位置，未进行人工听音验收。

## 补充：还原关卡配乐分配

原作 `-[Game nameMusic]`（`0xa3ebc`）先判断 `levelID_ < 4`，前三关固定返回 `SymphonyofRush`。其他关卡依据 `suiteID_`：1 → `LifeforSpeed`，2 → `Hectic`，3 → `SpiralofAdventure`，其他值 → `SymphonyofRush`。`SkyTunnel` 不在关卡选曲分支中。

曲名常量分别位于 `0x1f4d64`、`0x1f4d68`、`0x1f4d6c`、`0x1f4d70` 指向的 CFString。套组来自地图 `<Header><Suite ID="…"/></Header>`；以 `Level_XXX` 文件名对应游玩的关卡编号，部分地图根节点的 `Map ID` 是不同编号，不能据此列出关卡。

| 曲目 | 关卡数 | 关卡编号 |
| --- | --- | --- |
| `SymphonyofRush` | 23 | 1、2、3、4、5、6、10、12、13、16、21、23、24、29、31、32、34、37、44、49、54、55、57 |
| `Hectic` | 13 | 7、8、9、17、25、30、39、42、45、46、48、51、60 |
| `LifeforSpeed` | 16 | 11、14、15、18、19、26、28、33、35、38、41、47、50、53、56、58 |
| `SpiralofAdventure` | 8 | 20、22、27、36、40、43、52、59 |

`game/content/level_music.h` 实现原作选曲规则，`FlightScene` 传入当前关卡编号和已导入的 `suite`，替换每 15 关换一首的旧逻辑。已逐一核对 60 份导入数据的 `suite` 与原版地图一致。菜单仍按用户要求统一使用 `MoonTrip`，所以 `SkyTunnel` 保留在资源中，当前没有场景播放它；本次共使用六首资源中的五首。

新增 `fly_original_music` 测试，用独立的 60 关配乐清单核对真实导入数据和选曲函数，验证前三关覆盖各套组的特殊规则、未知套组回退，以及曲目注册和文件存在。现有 `fly_idle_visuals` 检查四首关卡曲目均能加载并播放，且从 `Hectic` 返回菜单恢复 `MoonTrip`。

验证命令：`ctest --test-dir out/build/x64-Debug -R "^(fly_original_music|fly_idle_visuals|fly_selection_input)$" --output-on-failure`。

补充更新验证结果：Debug 游戏及相关测试目标编译成功，三项测试全部通过：`fly_original_music` 1.49 秒，`fly_idle_visuals` 82.12 秒，`fly_selection_input` 0.46 秒。实际音频检查使用 SDL dummy 设备，未进行人工听音验收。
