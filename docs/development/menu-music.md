# 菜单音乐连续播放

2026-10-26：主菜单、设置、成就和关卡选择统一使用 `fly.SkyTunnel`，相互切换、分页或返回主菜单时保留播放位置。制作人员界面保留 `fly.MoonTrip`，返回主菜单时切回 `SkyTunnel`。进入关卡按原作套组选择音乐；从关卡返回选关时启动 `SkyTunnel`。F2 开发面板及其 ImGui 配置保持启用。

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

上述四个菜单类的 `dealloc` 中均未发现停止/暂停音乐调用。原作支持同曲连续播放，制作人员界面会换曲。本项目的主菜单、设置和选关使用同样的曲目分配；独立的成就界面按用户确认也跟随 `SkyTunnel`，制作人员界面使用 `MoonTrip`。

## 实现和验证

各菜单场景进入时调用引擎已有的 `transition_music`：相同曲目继续播放，无音乐或曲目不同时启动该场景的曲目。主菜单和制作人员场景退出时只清理界面，不停止全局音乐。关卡退出仍停止关卡音乐，重试仍重新开始配乐。没有新增公开接口或更换音频资源。

`game/tests/visual_tests.cpp` 使用 SDL dummy 音频后端加载真实音乐，将 `SkyTunnel` 播放位置设为 240000 帧，进入/退出设置、四个选关分页和三个成就分页并返回主菜单，检查曲目、播放状态和位置不回退；另检查静音/恢复音量，以及无音乐或关卡音乐之后各菜单恢复 `SkyTunnel`。制作人员界面单独检查 `MoonTrip`、同曲重进不重播，以及返回主菜单恢复 `SkyTunnel`。检查已接入 `fly_audio_scene` 音频专项测试，存档使用 `out/qa/menu-music/` 下的独立目录。

验证命令：`ctest --test-dir out/build/x64-Debug -R "^(fly_audio_scene|fly_original_music|fly_level_music_playback|fly_development_overlay)$" --output-on-failure`。开发面板测试通过 F2 事件验证显示/隐藏及面板功能；音乐检查不包含人工听音验收。

本次四项专项测试全部通过：`fly_original_music` 1.51 秒、`fly_development_overlay` 0.14 秒、`fly_audio_scene` 2.98 秒、`fly_level_music_playback` 9.50 秒。开发面板测试已适配当前的面板注册接口，验证相机缩放、F2 开关、暂停时的 UI 帧、隐藏时不捕获输入以及渲染状态恢复；引擎、面板实现和 ImGui 配置未修改。测试日志见 `test-evidence/menu-skytunnel-ctest.log`。

启用 ImGui 的 Debug 游戏已构建为标准路径 `out/build/x64-Debug/FlyMe2TheMoon.exe`。首次链接时旧游戏占用该文件，曾先生成独立的 `FlyMe2TheMoon-SkyTunnel.exe`；用户授权关闭旧游戏后，标准可执行文件也已更新完成。

## 补充：还原关卡配乐分配

原作 `-[Game nameMusic]`（`0xa3ebc`）先判断 `levelID_ < 4`，前三关固定返回 `SymphonyofRush`。其他关卡依据 `suiteID_`：1 → `LifeforSpeed`，2 → `Hectic`，3 → `SpiralofAdventure`，其他值 → `SymphonyofRush`。`SkyTunnel` 不在关卡选曲分支中。

曲名常量分别位于 `0x1f4d64`、`0x1f4d68`、`0x1f4d6c`、`0x1f4d70` 指向的 CFString。套组来自地图 `<Header><Suite ID="…"/></Header>`；以 `Level_XXX` 文件名对应游玩的关卡编号，部分地图根节点的 `Map ID` 是不同编号，不能据此列出关卡。

| 曲目 | 关卡数 | 关卡编号 |
| --- | --- | --- |
| `SymphonyofRush` | 23 | 1、2、3、4、5、6、10、12、13、16、21、23、24、29、31、32、34、37、44、49、54、55、57 |
| `Hectic` | 13 | 7、8、9、17、25、30、39、42、45、46、48、51、60 |
| `LifeforSpeed` | 16 | 11、14、15、18、19、26、28、33、35、38、41、47、50、53、56、58 |
| `SpiralofAdventure` | 8 | 20、22、27、36、40、43、52、59 |

`game/content/level_music.h` 实现原作选曲规则，`FlightScene` 传入当前关卡编号和已导入的 `suite`，替换每 15 关换一首的旧逻辑。已逐一核对 60 份导入数据的 `suite` 与原版地图一致。当前菜单使用 `SkyTunnel`，制作人员使用 `MoonTrip`，关卡使用另外四首音乐，六首资源全部有对应场景。

`fly_original_music` 测试用独立的 60 关配乐清单核对真实导入数据和选曲函数，验证前三关覆盖各套组的特殊规则、未知套组回退，以及曲目注册和文件存在。`fly_level_music_playback` 逐一进入 60 个真实关卡场景，检查实际曲目、播放帧推进和退出停止；`fly_audio_scene` 检查从 `Hectic` 返回菜单恢复 `SkyTunnel`。

关卡逐关核验及原始资源比对记录见 `test-evidence/level-music60-check.md`；本次菜单改曲保留既有的关卡选曲规则。
