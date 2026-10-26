# 前两关教学还原

2026-10-26，根据本地原 Lua、原 iOS 程序的只读静态分析与原 PNG 素材修正。原关卡脚本和引擎代码均未修改。

## 行为

- 镜头返回起点后立即恢复逻辑、启动 Lua。第一关先念咒，`pauseLogic` 冻结角色与光球，继续后角色才展开；第二关入场与原脚本 1 秒等待重叠，移除额外 16/30 秒等待。
- 第一关教学图以 10 Hz 交替抬起/按下手指、站立/上升角色与箭头，周期 2 秒。第二关使用原 80 帧位置、方向、遮片和角色状态表，周期 8 秒。
- 教学图保留原组合图的相对布局与贴图比例，在横屏对白上方整体缩放和定位。位置适配桌面，未宣称 iOS 逐像素一致。
- Demo 手指使用原 `fingerUp`/`fingerDown`，按用户后续要求固定在逻辑画布 (640,420)，位于对话框上方；两张图只随录制按压状态切换，不跟随相机或录制坐标移动。继续保留此前修复：玩家输入不得清除 Demo 推力，否则动画在物理更新之前读到零值。
- 更正先前对资源名的判断：`inGame/blackEdge.png` 是本地化的“演示中”文字，不是电影黑边。恢复原文字贴图以及 127→255→127、每段 1 秒的循环透明度。
- 对白按 60 字符/秒逐字显示，完整 Unicode 字符一起出现。普通确认先补全文字，再确认才进入下一句；鼠标在抬起时确认。`#cRRGGBB` 与 `#n` 恢复文字颜色，排版根据完整句子预先计算，逐字显示不改变已显示字的位置。
- 保留 Lua 的 `showMinTime` 和 `showFixedTime`：首次教学仍需等满 4/6.5 秒；固定时长 Demo 不被普通确认提前结束；已看过且允许跳过时，Tab 与手柄跳过键可越过最低展示时间。按用户后续要求移除鼠标“跳过”按钮，鼠标仍可补全和继续对白。
- 原角色动画表恢复飞行/站立等 25 fps、待机/左右观察等 10 fps；脚本指定的左顾右盼为 35 帧、3.5 秒，与原 Lua 的 3 秒等待独立，保留末尾约 0.5 秒与下一句重叠。动作切换重置相应动画。
- 普通角色随逻辑暂停，死亡呈现保留独立推进，避免场景等待死亡回调时锁死；暂停菜单仍冻结全部呈现、教学图和对白揭示。

## 原版依据

- `ScriptController.onGameStart3`（0xa3dac）恢复跟随、输入、逻辑后唤醒 Lua。
- `Game.doUpdate:realDt:`（0xa6308）在暂停时跳过角色所在的 `doUpdateGame:`；`Role.update:`（0xcbaa0）更新模型与视图，`RoleView.updateBallEffect:`（0xd0f94）推进光球。
- `TutorialDiagram.update:`（0xebbbc）、`updateAnimate1ByFrame:`（0xeb570）、`updateAnimate2ByFrame:`（0xeb900）。第二关查表：手指/遮片 0x1f3550/0x1f37d0/0x1f3a50，箭头 0x1ac9e8，角色 0x1f3500；原 switch 映射为 stand、left、right、fall、up。
- `GameController.gameSetReplayMode`（0x9f400）、`TutorialLayer.updateFinger`（0xe940c）、`HUDLayer.setBlackEdgeVisible:`（0xe35d8）。
- `InGameDialog.update:`（0xeeff8）、`touch:`（0xef1f4）：逐字、补全与已允许的跳过优先于最短展示时间。
- `RoleView.playLookLeftRight`（0xd129c）；角色帧数/帧率表 0x1ac97c/0x1ac9a8。
- 本轮进一步确认 `AppDelegate.applicationDidFinishLaunching:` 在 double 地址 0x7938c 设置导演间隔为 1/60 秒；原录制每次游戏更新推进一条。因此当前逐 60 Hz 物理帧读取教程录制有原程序依据。录制文件头 0.166667 仍保留，其用途不作额外解释。

`game/tools/import_tutorial_visual.py` 可重新提取表格和本地化 Demo 图片，`import_content.py` 也会调用它。参考文件只读。

## 验证入口

- `fly_story_tests`：60 关、145 个对白键、27 个持久标记，以及最短时长、跳过返回值、固定镜头等待。
- `fly_gamepad_tests`：真实前两关开场与教学流程、角色暂停/展开、鼠标抬起确认、3.5 秒动作、首次教学最低时间、已读教学 Tab 跳过、Demo 到月亮后复位交接，以及 240 Hz 呈现/60 Hz 物理下的 Demo 动作与粒子。
- `fly_visual_tests`：UTF-8 逐字、颜色与稳定位置，两个教学图的组合和完整 80 帧状态，Demo 手指与文字提示的显示/清除；生成 `out/qa/tutorial-restoration-preview.bmp`。
- 原生输入：`game/testing/tutorial-original-opening.json`、`tutorial-original-up.json`、`tutorial-original-direction.json`、`tutorial-original-demo.json`。对应截图与结构化结果保存在 `out/qa/`；回放采用 dummy 音频设备，音效听感不在此次截图验收范围。

最终 Debug 主程序与相关测试目标构建通过。合入同期音频改动并解决三处冲突后，7 项游戏回归全部通过（54.02 秒）：音频、场景/输入、视觉、选关、评分存档、原 Lua 和机关，日志为 `out/qa/tutorial-ctest-merged.log`。四条原生回放的画面断言均通过且进程退出码为 0；开场、上下教学、方向教学和 Demo 截图已检查。没有据此宣称所有引擎测试或 60 关人工验收完成。

首次还原验收时曾将鼠标跳过按钮移至底部提示栏右侧，场景输入与原 Lua 两项复测通过（14.21 秒）。后续按用户要求直接取消该按钮，跳过保留键盘和手柄绑定；Demo 手指也改为固定屏幕位置。

固定手指与删除按钮后，Debug 构建通过；场景输入和视觉两项回归通过（50.63 秒），覆盖已读教学 Tab 跳过及两张手指图的相同固定位置。第一关 Demo 原生回放断言通过、进程退出码 0，最新截图 `out/qa/tutorial-original-demo.bmp` 已检查。

第二关原 Lua 没有调用自动飞行 Demo，本次没有添加。桌面字体排版、横屏视野与角色跟随策略仍为当前项目适配，未运行原 iOS 应用做逐帧视觉等价比较。
