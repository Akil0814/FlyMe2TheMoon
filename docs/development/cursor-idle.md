# 局内光标空闲隐藏

`FlightScene` 持有 `game::platform::IdleCursor`，进入关卡时显示光标，在 `on_before_update` 的暂停分支之前采样，退出或销毁时恢复。鼠标静止满 2 秒隐藏，实际位移后显示并重新计时。开场、剧情、暂停、结算和 F2 沿用同一策略；持续按住鼠标、键盘或手柄不重置计时。

控制器使用 `SDL_GetTicksNS` 和物理鼠标状态，不依赖模拟 delta、时间倍率或路由输入。窗口失去键盘/鼠标焦点、最小化或指针移出客户区时显示并重置计时；客户区边界也会显式检查，避免拖拽捕获鼠标后在窗口外隐藏桌面光标。显隐操作仅在状态切换时提交。新的关卡取得所有权后，旧场景的退出不会覆盖新场景的光标状态。

游戏层 ImGui 适配器在光标因空闲隐藏时，围绕 SDL 后端的 `begin_frame` 临时设置 `ImGuiConfigFlags_NoMouseCursorChange`，随后恢复该位原值，防止后端每帧重新显示光标。移动唤醒或退出关卡后恢复普通 ImGui 光标形状管理。没有修改引擎公共接口或增加用户设置。

## 验证

- `fly_cursor_idle` 使用注入采样覆盖精确的 2 秒边界、微小移动、暂停时间、持续输入、焦点恢复、重试、销毁和跨场景所有权。
- `fly_gamepad_input` 使用真实 `FlightScene` 验证开场、剧情、暂停、正常飞行、结算以及退出/重试的显隐行为；采样时钟和位置可注入，模拟 delta 为零。
- `fly_development_overlay` 验证暂停的 F2 界面中反复隐藏/恢复、ImGui 不覆盖隐藏状态，以及原有 ConfigFlags 得到保留。

以上自动化检查和 Debug 构建通过。原生窗口检查尝试中，沙箱内定位未移动实际系统指针；Windows 自动化工具返回 `foreground window did not report a process id`，因此未完成真实桌面输入验收。

可在交互桌面运行 `out/build/x64-Debug/game/fly_cursor_tests.exe --window-smoke`，点击测试窗口开始，静止等待隐藏，标题提示移动后在窗口内移动鼠标。该检查查询 Windows 实际光标句柄，并继续验证移出客户区、返回和最小化恢复；不会操作存档。
