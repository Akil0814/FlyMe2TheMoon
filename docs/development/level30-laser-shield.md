# 第 30 关出生平台与激光遮挡

第 30 关中间岩石平台的贴图位置正确，但旧导入器没有处理静态碰撞对象的 `IsXFlipped`。碰撞轮廓落在平台右侧，向下的激光因此穿过贴图，命中出生角色。

## 原作对照

- `reference/Fly2theMoon_1.1_ios_3.2/01_关卡/Level_030/Level_030` 的 `StaticObject` 层包含 `SeqID="3" X="717" Y="489" IsXFlipped="true"`。
- 原程序 `StaticObjectLayer.load:` 在 `0xabf2c`、`0xabf68` 分别读取水平、垂直翻转，并在 `0xac090` 调用 `edgeTransformatedByRotation:xFlip:yFlip:`。
- `Edge.edgeTransformatedByRotation:xFlip:yFlip:` 的 `0x97e34..0x97ea4` 先翻转局部坐标，再应用对象旋转。这里只读分析参考程序，没有运行 iOS 原作。
- 旧碰撞轮廓的 X 范围为 `719.70..855.95`；正确镜像后的范围为 `578.05..714.30`，与中间平台相符。出生下射激光约位于 `x=640.25`，修正后在 `y=2340.95` 首次碰到平台，早于玩家身体所在位置。

出生点、激光角度、载体速度均未修改，也没有添加出生无敌。原作 `RoleModel.load:` 会立即创建身体并把免疫时间清零；出生特效隐藏精灵，不构成免疫依据。

## 修正范围与验证

导入器恢复静态碰撞对象的水平、垂直翻转。核对全部 60 关后，现有数据中只有第 28、30、41 关包含翻转的静态对象，因此只更新这三关对应的碰撞轮廓。其余关卡和这三关的非碰撞数据保持一致。

`fly_idle_visuals` 增加第 30 关实际激光、出生角色和全部地形轮廓的物理查询回归：平台存在时光束应止于中间岛屿；移除地形后，同一束激光必须能够命中角色。`game/testing/level30-spawn-shield.json` 用无输入原生回放检查开场结束后仍存活。

本次结果：

- 直接执行修正后导入器的静态地形分支，全部 60 关的输出与保存数据完全一致；对比修正前，只有第 28、30、41 关的碰撞轮廓变化。
- Release 主程序和 `fly_visual_tests` 构建成功；`fly_idle_visuals` 通过（1/1）。在独立素材副本中换回旧第 30 关数据，同一测试准确失败于 `Level 30 island failed to shield the player at spawn`。
- `level30-spawn-shield` 原生回放退出码 0、全部断言通过；墙钟运行 5.5 秒，开场结束后的游戏时间约 3.06 秒，7 束激光仍开启，角色碰撞体启用、死亡次数 0。已查看回放截图。

构建、导入核验和新旧数据对照日志位于 `out/qa/level30-shield/`；回放报告及截图位于 `docs/development/test-evidence/level30-spawn-shield.{json,bmp}`。这些结果覆盖出生遮挡，没有验证第 30 关完整通关。
