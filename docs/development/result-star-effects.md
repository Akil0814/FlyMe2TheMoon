# 结算星光效果

2026-10-28，静态读取本地原作二进制的 Objective-C 元数据及 ARM 指令，未执行原作。

## 原作证据

`SuccessLayer.showSingleStar:`（0xe676c）先播放 `106-1..3`，在对应星槽中心创建 `createStarParticle`，设为 1 倍；等待 0.2 秒后调用 `showSingleStar2:`（0xe66d0），将 `starUp.png` 换成 `starDown.png`。星星精灵没有旋转、移动或缩放动作。`startAnimation` 按 0.5 秒间隔启动每颗已获得星星的这一流程。

`createStarParticle`（0xe6094）使用 `CCParticleSystemQuad` 和独立贴图 `stars2-grayscale.png`：

| 参数 | 原值 |
|---|---|
| 容量 / 发射时长 / 发射速率 | 100 / 0.2 秒 / 500 每秒 |
| 生命 / 随机范围 | 0.2 ± 0.5 秒，非正生命不显示 |
| 起始尺寸 / 随机范围 / 结束尺寸 | 10 ± 10 / 5 |
| 角度 / 随机范围 | 0 ± 360° |
| 速度 / 随机范围 | 150 ± 15 |
| 重力 | (0, 0) |
| 径向 / 切向加速度 | -400 / -144.74001 |
| 起始颜色 / 结束颜色 | (128,158,182,0) / (0,0,0,0) |
| 初始、结束旋转 | 默认 0 |

基类 `CCParticleSystem.initWithTotalParticles:`（0x2aa2c）保留零初始化的颜色和旋转，默认混合为 `ONE / ONE_MINUS_SRC_ALPHA`。颜色的 alpha 始终为零，因此原效果以 RGB 加亮背景，并随 RGB 降至零消失。SDL 使用独立纹理的 `SDL_BLENDMODE_ADD`、不透明顶点及随生命下降的 RGB，表达同样的加亮意图。

可用 `game/tools/inspect_original.py --class-name SuccessLayer --method createStarParticle` 等方法重新生成反汇编。导入工具 `game/tools/import_result_star_effects.py` 将原始灰度贴图原样复制并注册为 `fly.result_star_particles`，没有改用激光的另一张星光图。

## 游戏接入与边界

`StarRating::present_result` 接收结算的累计时间，按每颗星的 0.2 秒延迟切换贴图；`result_star_effects.cpp` 在 UI 坐标中绘制粒子，径向和切向运动在原作 Y 向上坐标中积分后反射到屏幕。尺寸与速度按当前星槽和原星图半尺寸的比例适配桌面布局。星星自身保持固定位置、尺寸和角度。

随机序列使用固定种子，保证重绘稳定；原作全局随机状态不可恢复。运动使用 60 Hz 小步积分和末尾余量，原版逐帧更新与这里的连续出生时间采样有细微差异，不宣称像素级一致。

暂停菜单、HUD、失败结算仍使用静态星星。跳过结算立即展示最终星数并清理粒子；重试和离开场景随 UI 对象释放状态。零星结算不生成粒子，低星结算不为未获得的槽位发射。评分、存档和计分阶段时间保持原逻辑。

## 验证

Debug 游戏与相关测试构建通过；`fly_idle_visuals`、`fly_audio_scene`、`fly_gamepad_input`、`fly_selection_input`、`fly_game_rules_and_save` 五项 CTest 全部通过。资源完整性与原图清晰度审计 `asset_usage.py --check-hd` 通过，无未注册或闲置资源。

自动化检查覆盖 0.2/0.7/1.2 秒点亮、对应音效提前随粒子启动、原贴图和混合方式、星星不旋转移动缩放、重复绘制稳定、零星/一星与粒子寿命、静态显示和跳过清理。软件渲染九阶段预览：`out/qa/result-star-effects-preview.bmp`，已查看。原生窗口手动观感及实机音频验收另行进行。
