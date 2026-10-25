# 角色出生、到达月亮与传送特效

本次补充局内三类视觉过渡；使用已导入的原作素材和引擎公开绘制、物理接口，未修改引擎。

## 原作证据

仅读取本地参考 Mach-O，不加载或执行原程序。方法地址可用 `game/tools/inspect_original.py` 复查。

| 方法 | 已核实行为 |
|---|---|
| `RoleViewBallEffect.init`（0xd14b4） | 一张 ball、四张 ballDisapear、六张 ballExplode；Disapear 为原资源拼写 |
| `update:`（0xd1f10）、`shouldRoleBodyVisible`（0xd1cac） | 特效索引为时间乘 30 后截断；入场第 10 帧显示角色，离场第 6 帧隐藏角色 |
| `refreshAnimationEnterScene`（0xd17fc） | 第 0 帧隐藏；1～4 帧展开星光；5～9 帧光球从 0.5 放大至 0.9；10～15 帧散开粒子，保留 0.9 比例；第 16 帧回普通角色 |
| `refreshAnimationLeaveScene`（0xd19d0） | 前六帧倒放散开粒子，随后光球从 1 缩至约 0.5，最后倒放四张星光；角色持续隐藏 |
| `playLeaveSceneEffect`（0xd1e68）、`playInPortalEffect`（0xd1e9c） | 光球收缩分别为 26、10 帧；完整离场分别为 36、20 帧。注意立即数按 IEEE float 解码，而非把十六进制尾数当十进制 |
| `RoleLayer.roleGotoMoon:`（0xabac0） | 化球的同时用 1.2 秒移到月亮中心，`CCEaseOut` rate=2；再等待 1.2 秒，才调用 `roleReachedMoon:moon:`，进入剧情或结算 |
| `Portal.playDrawRoleEffect:`（0xbc194） | 2/3 秒吸入门中心，同时缩至 0.5，`CCEaseIn` rate=2，随后启动镜头移动 |
| `Portal.getTeleportTimeToTargetPortal`（0xbbe74）、`teleportRole:targetPortal:`（0xbc534） | 镜头时长为 `(两门距离 / 1000)^0.6`；入口销毁角色物理体；出口回调额外包含 0.138 秒余量 |
| `Portal.playCameraMoveEffect:role:`（0xbc00c）、`playRoleOutEffect:`（0xbbf40） | 镜头使用 `CCEaseSineOut`；出口播放入场光球，并用 0.3 秒恢复节点比例，`CCEaseOut` rate=2 |
| `CCEaseOut.update:`（0x54e0）、`CCEaseSineOut.update:`（0x5974） | rate=2 的 EaseOut 是 `sqrt(t)`；SineOut 是 `sin(t*pi/2)` |

## 游戏层接入

`role_effect.h` 计算贴图、帧号、比例、角色可见性和过渡阶段；`Player` 提交普通角色与光球绘制命令；`FlightScene` 管理输入、物理与剧情事件。首次出生等待开场镜头返回起点后播放，展开结束后恢复操作；`role.loadState` 会重播入场，剧情暂停期间视觉继续推进；出生不增加无敌或关闭碰撞。

到达月亮后停止角色物理与输入，让显示中心移到月亮；2.4 秒结束后再派发原 Lua 回调。事件只派发一次，原脚本仍可进入对白、成功结算或教程复位。月亮过渡不计入已完成的飞行时间。

传送期间关闭角色物理体，吸入后隐藏角色，镜头从当前视野移向出口，再展开角色并恢复输入和碰撞。其他机关与弹体继续更新；传送时间继续计入飞行时间。出口仍使用已有的区域锁，防止立即重复触发。角色显示中心对准门中心，并转换回低 13 单位的物理锚点。

物理体关闭会清掉 Box2D 速度，因此游戏显式保存入口速度，并在出口重新启用后写回。**保留速度、出口锁和等待完整展开后恢复碰撞是当前游戏的衔接策略，不宣称已核实原作的出门动量与控制恢复帧。** 原作帧号、素材与上述动作曲线有静态证据；世界边界裁剪、桌面镜头尺寸和现有角色动画框架仍使用本项目实现。本次没有接入角色音效。

补充核对月亮本体的到达高亮：`Moon.initLightEffect`（0xbba5c）读取本组第二帧，作为与常态月亮对齐的独立子图层；`startAnimate`（0xbb544）在到达时显示它，`updateAnimation:`（0xbb5b0）读取 0x1a139c 的 220 字节透明度表。表在索引 99 达到 255，保持至索引 218，索引 219 为 0。游戏以原 60 Hz 节奏换算为约 1.65 秒完全点亮、3.65 秒关闭，避免桌面帧率影响时长；此图层与角色化光球同时启动。新增 `import_moon_visual.py` 可从参考文件重建 `moon_visual.h`，全程不执行原程序。

## 验证

- `fly_idle_visuals`：三种光球时序的全部帧，真实图集绑定、尺寸、显隐、复位；实际物理世界中的关闭、传送、启用与速度恢复。
- 月亮高亮逐帧检查第二张贴图、透明度关键值、底图保留、图层对齐、到达重播和最终关闭；预览位于 `out/qa/moon-highlight-preview.bmp`。
- `fly_mechanisms`：月亮 1.2/2.4 秒边界、原 EaseOut 曲线、零距离及不同距离传送、吸入和出口边界。
- `fly_original_story`：原 Lua 分支回归；其他游戏、存档与菜单测试同步运行。
- 软件渲染预览：`out/qa/role-transition-effects.bmp`，从上到下是入场、月亮离场、入门离场，各行从左到右按时间推进。

原生回放及最终验证结果记录在 `original-flight-research.md` 的本次增量记录中。自动化与局部实跑不等同于全 60 关人工验收。
