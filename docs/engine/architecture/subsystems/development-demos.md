# 模块展示应用

本文面向维护演示应用的开发者，说明各模块的源码归属、返回路由和状态清理。演示应用用于组合与观察引擎能力，其业务类型不会进入引擎库。

`game_lib` 是项目拥有的引擎功能展示应用。主菜单保留模块展示、设置和退出；模块总目录按 UI、输入与控制、相机、动画、特效、音频、物理、Gameplay 排列。Elysia Realm 和需要确认的故障注入位于辅助功能区。

## 文件与职责

- `game/application` 显式注册场景并配置启动路线。
- `game/navigation` 保存主菜单和场景标识；已有数值保持稳定，音频、Gameplay 目录和 Gameplay 验证分别使用 15、16、17。
- `game/showcase/<module>` 就近保存场景、展示组件、状态和运行逻辑。场景管理输入、场景路线与生命周期；View 构建 UI、显示数据并调用操作回调。
- `game/showcase/shared` 保存多个模块实际使用的外壳、几何布局和 Inspector。
- `game/showcase/scenarios` 保存共同的案例执行器、验证显示和目录组织。战斗 actor、combat session、地图和障碍物位于 `game/showcase/gameplay/runtime`。

场景继续拥有窗口及 `SceneObject`，View 的控件指针均为借用引用。重建前清理主题注册和引用；退出前撤销 Inspector、恢复设备与调试状态。`destroy()` 只标记对象，最终释放由场景安全点完成。

## 路由与展示

普通模块和自由游玩使用 `ShowcaseEnterPayload{return_route}`。验证场景使用 `ScenarioEnterPayload{return_route, scenario_id, pressure_tier}`；保留完整返回路线中的目标、payload 与 reload mode。

目录、UI、相机、动画和特效允许缓存复用；本地多人、音频、自由游玩及验证场景从目录进入时重新创建。验证重启重新创建案例；暂停与单步不创建另一套世界。

`ShowcaseFrame` 提供统一标题、说明、返回与状态区域；列表页面使用滚动内容区，世界展示使用轻量外壳和独立操作面板。UI Gallery 保留分页。技能示例的冷却与道具状态由 `HudDemoState` 保存，控件由 `HudDemoView` 显示；隐藏页不处理快捷键。

## 相机展示

`CameraShowcaseScene` 使用场景标识 13，拥有 Main 和 Cinematic。跟随、镜头运动、演出三页共享两个角色和世界展示，场景保存 `CameraDemoState`、句柄与阶段；ControlsView 接收数据和回调，OverlayView 接收当前呈现相机的投影数据。页签使用 `UiTabBar`，操作行各自使用滚动容器与焦点区。

演出阶段由混合和运动完成值事件推进，并校验句柄；Hold 停留计时跳过路径完成帧，防止将整帧 delta 重复计入停留。演出通过 actor 控制门控冻结角色，保持绑定并保存自动运动设置。退出及进入失败清理活动和借用引用，控件按场景安全点释放。

`SceneCameraRuntime` 保留终点 Holding 的句柄所有权；暂停该姿态时也冻结该槽的震动呈现。公共接口保持不变。

执行结果和待完成的体验验收见 [相机展示验收记录](camera-showcase-validation.md)。

## 物理与 Gameplay

物理组提供十个基础案例，以及 bodies、contacts、tiles 三个压力案例的低、中、高档。Gameplay 组提供碰撞战斗、平台战斗、俯视战斗三个玩法场景，以及对应的三个战斗验证案例。

`ShowcaseScenario` 是展示与 CTest 共用的案例执行器。保留原案例 ID、步数和阈值。物理验证只拥有案例世界；自由游玩只拥有 `GameplayScene` 的世界。Gameplay 验证使用场景标识 17，不再通过同一场景中的 Verify/FreePlay 分支切换运行方式。

Gameplay 操作区提供暂停、单步、重启、世界 UI 显示开关、气泡触发与返回。`WorldBar`、`WorldText` 和 `SpeechBubble` 复用 actor 的实现，锚定角色呈现位置；伤害、死亡和清理仍遵循原战斗逻辑。

## 音频

运行 `python scripts/generate_showcase_audio.py` 可重建低音量 PCM WAV 演示素材。素材不依赖外部音频，项目清单登记一个音效和两段循环音乐。

音频展示覆盖即时/延迟音效、循环、分组限制及冷却、音乐淡入淡出与切换、音量控制。场景保存自己取得的 SoundHandle，退出时取消这些活动或待播放请求；其他场景的音效请求不被全局取消。音量和分组设置在退出及进入失败时恢复，不写入用户配置。音乐使用引擎的单一音乐播放通道，展示中的音乐播放会使用该通道。

## 验证

测试位于 `tests/game`，展示代码不放入测试目录，也不编入 `engine_lib`。现有案例 CTest 名称保持不变，新增 `showcase_tests` 检查模块顺序、案例归属、音频素材和退出恢复；`showcase_render_tests` 加载项目资源并遍历模块、玩法、验证场景及五种语言。

自动测试包括完整返回路线、重复进入、HUD 键鼠交互、相机重入、多人设备恢复、Inspector 注销、Gameplay 世界 UI 以及基础和战斗案例。软件渲染截图用于布局核对，真实键鼠、手柄体验和音频试听需要另行记录。

本次重构的执行结果及待完成的手动验收见 [展示重构验收记录](showcase-validation.md)。
