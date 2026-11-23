# 第 48 关传送门连接修复

## 原因与原作依据

第 48 关入口对象 14 的 `Info.TargetPortalID="5"` 应指向出口对象 5。出口对象 22 同时带有 `Info.PortalID="5"`，此前运行时和路线规划器把这个字段当作优先标识，造成 14→22 的错误连接。

本地原程序静态核对：`Portal.tryTeleportRole`（0xbc5ec）读取 `targetPortalID_`，调用 `DynamicObjectLayer.getObjectByID:`；后者（0xa7d6c）通过整数键从对象字典查找。`Portal.initWithXML:`（0xbd2d4）读取 `Info.TargetPortalID`，没有将 `Info.PortalID` 作为另一套对象 ID 的逻辑。因此保留原始 XML/JSON，修正查找规则即可。

| 第 48 关入口对象 | 原作出口对象 | 修复前 | 修复后 |
|---|---|---|---|
| 1、3、4、20、21 | 16 | 16 | 16 |
| 10 | 22 | 22 | 22 |
| 11 | 19 | 19 | 19 |
| 13 | 2 | 2 | 2 |
| 14 | 5 | 22 | 5 |

游戏侧私有方法 `FlightScene::find_portal_destination` 和 QA 路线规划器现在只匹配对象 ID。目标缺失仍报告 `Portal destination absent`，不回退到 `PortalID` 或其他对象。

## 验证范围

- 修复前的实际场景测试退出 1，报告 `Level 48 portal 14 expected object 5, got 22`；规划器测试也在同一连接失败，见 `out/qa/portal-scene-before.log`、`portal-reference-before.log`。
- 全 60 关原 XML 与导入数据核对：71 个传送门、41 个入口，目标均存在且为出口。修复前后只有第 48 关的 14→5 改变。见 `out/qa/portal-reference-regression.json`。
- 全 60 个实际 `FlightScene` 调用运行时目标查找；第 48 关额外验证真实重叠处理、吸入/移动/展开、速度恢复、暂停冻结与恢复、出口锁释放、缺失目标报错及重试清理。见 `out/qa/portal-scene-regression.json`。
- Debug 构建通过。`fly_portal_reference`、`fly_portal_links`、`fly_mechanisms`、`fly_idle_visuals`、`fly_audio_scene`、`fly_gamepad_input`、`fly_original_story`、`fly_game_rules_and_save` 共 8 项 CTest 通过，见 `out/qa/portal-ctest.log`。
- 第 13 关原有 `role-transition-portal` 输入回放通过，实际发生 0→1 传送，见 `out/qa/level13-portal-regression.json` 和对应 `-run.json`。
- 第 48 关旧输入路线在正确连接下反复触发 14→5，75 秒内未通关；失败结果完整保留于 `out/qa/level48-portal-old-route.json`。重新生成路线的初次回放被后半段激光击中；静态路线不会处理跷跷板遮挡激光，输入校准证据单独保留，不能据此宣称整关通过。
- 校准路线补入推动上层跷跷板、遮挡激光的正常鼠标操作，再从 10→22 前往终点；`level48-portal-calibration05` 已完成原剧情和成功结算，程序退出 0。通过的路线写回 `game/testing/level48-route01.json`，保留原有通关断言，并增加成功结算、零死亡、恰好两次传送及完整传送阶段检查。正式路线再次通过，程序退出 0，计时 21.586 秒、零死亡、两次传送；Direct3D12 回放结果、结算截图及程序/场景文件 SHA-256 记录于 `out/qa/level48-route01.json`、`level48-route01.bmp`、`level48-route01-run.json`。两次成功回放均只注入正常 SDL 输入，没有定位或修改模拟状态。

软件场景截图位于 `out/qa/level48-portal-before.bmp`、`level48-portal-travel.bmp`、`level48-portal-emerge.bmp`、`level48-portal-after.bmp` 和 `level48-portal-paused.bmp`。场景测试用夹具定位到入口，再执行实际重叠和传送流程；这与只注入正常 SDL 输入的整关回放分开记录。

本次不调整引擎、图片、关卡配置、物理参数、存档格式或版本号，不包含打包发布；全关目标查找通过不等于全 60 关通关或手机触控验收。
