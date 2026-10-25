# 开场与剧情镜头移动

关卡载入后先显示终点月亮，停留 1 秒，再移动到角色起点。镜头抵达起点后才播放角色出场动画，展开完成后恢复操作和物理，启动原关卡 Lua 剧情；开场不计入飞行时间。剧情中的 `camera.gotoPosition`、`camera.gotoRole` 也会移动镜头，并在到达后继续执行后面的对白或操作。

## 原作依据

仅读取本地原程序，不执行它；方法可用 `game/tools/inspect_original.py` 复查。

- `ScriptController.onGameLoaded`（0xa3c6c）调用 `cameraSetToMoon` 并关闭角色跟随。
- `onGameStart`（0xa3ca8）关闭输入、暂停逻辑，等待 1 秒；`onGameStart2`（0xa3d84）调用 `cameraGotoRoleTodo:`。
- `onGameStart3`（0xa3dac）恢复跟随、输入和逻辑，再唤醒起始剧情脚本。
- `GameController.cameraGotoPosition:todo:`（0x9f8c4）按 `(两点距离 / 1000)^0.6` 计算移动秒数。
- `moveGameCameraAndContinue:time:callBack:`（0x9eec0）将 MoveTo 包在 `CCEaseSineOut` 内，再串联回调。原缓动为 `sin(t*pi/2)`，移动完成后才继续脚本。

## 游戏层职责

`game/gameplay/camera_move.h` 保存起点、终点、停留时间和移动进度，计算该帧焦点。`FlightScene::on_before_update` 推进它，场景相机运行时随后通过公开焦点接口取值。剧情暂停时相机仍推进，世界边界裁剪继续由引擎处理。

`FlightScene` 按 Pan → Spawn → Ready 三阶段衔接开场：运镜期间隐藏角色，镜头返回后重置并播放入场特效，等待全部 16 帧结束后进入 Ready。这一出场顺序按用户要求接入，避免动画提前在镜头外播完。

`FlightScene` 负责开场暂停、恢复与剧情启动，并将 Lua 的原 Y-up 坐标转换为当前关卡的 Y-down 坐标。剧情镜头命令向协程返回移动时长，`story::Runtime` 等待该时长后才继续；零距离调用立即结束。关闭跟随时会保留当前视野，移动结束且仍关闭跟随时停留在目标位置。角色复位也通过移动回到角色，避免直接跳回出生点。

日常角色跟随继续使用现有跟随策略。当前桌面视野、1.5 倍缩放和世界边界规则保持项目设置，未宣称与 iOS 画面逐像素一致。此次修改仅在游戏层，没有修改引擎或原关卡脚本。

## 验证

- 规则检查覆盖原距离公式、SineOut 中间位置、开场停留边界、跨停留边界的帧时间、零距离和重新指定目标时的连续性。
- 剧情检查覆盖镜头未到达时不显示下一句对白、返回角色后才恢复跟随、等待期间跳过操作不会越过移动，以及 60 关原 Lua 分支。
- `game/testing/camera-intro-story.json` 通过普通 SDL 输入验证第 3 关开场和局内展示；原生结果记录在 `original-flight-research.md`。
