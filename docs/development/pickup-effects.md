# 拾取动画

2026-10-26 核对本地参考程序的 Objective-C 方法和 ARM 指令，只读取二进制，没有执行参考程序。

| 原方法 | 动作 |
|---|---|
| `CrystalPiece.beEaten`（0xb1430）和 `ItemLayer.crystalPieceDisappear:`（0xa9bec） | 立即移除物理碰撞、累计宝石；原地 `ScaleTo(1.2, 0.3s)`，再 `ScaleTo(0.1, 0.3s)`，最后移除精灵。大小宝石共用动作。 |
| `Star.beEaten`（0xbf960）和 `HUDLayer.showStarEatenEffectAtScreenPosInView:`（0xe3c14） | 累计星星，在拾取屏幕位置创建 `animateStar.png`；0.05 秒放大至 1.4，然后用 0.6 秒同时移动到 HUD 目标槽位、缩小至 0.6。 |
| `HUDLayer.onStarFinished:star:` | 删除飞行精灵并调用 `updateStarDisplay`。 |

实现位于 `Entity` 的宝石消失状态和 `frontend::StarPickupEffects`。物理拾取、评分和存档计数立即生效，局内星槽按抵达动画的数量点亮。使用已有 HD 原图的半尺寸；星星目的地从当前 `StarRating::slot_rect` 获取，适配桌面 HUD，不使用旧手机界面的固定坐标。飞行精灵使用屏幕坐标，飞出后不再跟随世界相机。

暂停菜单冻结两类动画，恢复后继续；脚本恢复角色清空飞行并还原物品，重试和离开场景释放状态。结算清空剩余动画，使用真实收集数量显示成绩。没有加入无参考依据的粒子、淡出或旋转。

验证覆盖两种宝石的阶段尺寸/时长/原地中心、同一物品重复触碰、星星原图和线性移动缩放、连续拾取独立抵达、HUD 延迟点亮、真实场景暂停/恢复和重试。软件渲染时间线为 `out/qa/pickup-effects-preview.bmp`；原生窗口手动手感验收另行进行。
