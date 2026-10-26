# 局外界面打磨记录

日期：2026-10-25。本次范围为主菜单、四章选关、成就图册和设置界面。完整游戏的关卡验收与独立交付仍在进行中。

## 风格与素材

- 参考 `reference/in_game/` 的原作截图，采用蓝色月面、星光节点、玻璃按钮和原版人物插画。横向布局为本项目重新设计，不声称与原手机版界面完全一致。
- `game/tools/import_frontend.py` 复制已解码的原版按钮、星星、节点、17 枚 `achv*@2x.png`，以及用户提供的横屏背景。原始 `reference/` 文件未修改。
- 选关显示 15 个关卡节点及实际星数，保留解锁判定、四章入口、键盘导航与鼠标点击。锁定节点变暗并显示原版锁标记。
- 成就分为三页（6 / 6 / 5），使用原版名称、达成说明和徽章。未获得徽章覆盖深蓝色半透明遮罩，已获得项保持明亮。只添加排版空格，不改达成条件或原始文本数据。
- 设置页使用引擎公开的 `SettingsPanel` 和 `UserConfigService`，保留显示模式、窗口大小、帧率、垂直同步、音量、语言、保存与返回；背景、半透明分组和圆角由 Game Layer 配置。

## mainBg2 与 mainTitle2 的对齐

用户明确要求保留原版 `mainTitle2`，放回人物脚下并与 `mainBg2` 重合。两张原图分别为 **640×960** 与 **640×276**。对标题图没有文字覆盖的背景区域作像素比较，最佳对应位置为背景图 **Y=539**，样本的通道差中位数为 0。

`MenuArtwork` 将背景的 0—539、标题的 0—276、背景的 815—960 三段按同一比例绘制；标题目标 Y 为 `539 × scale`，不再作为独立按钮列的横幅。当前中央画幅为 544×816，逻辑视口为 1280×720，底部渐暗以承接横屏两侧。人物与标题的中央区域直接使用原版像素。

初次将原图叠到用户提供的横屏候选上时，发现候选图中的中央构图有变化，产生接缝和重复发丝，因此重新生成两侧扩展。后续测试发现窄纹理切片在非整数缩放时出现细竖线，已改为完整宽度的纹理四边形，仅用整数宽度的裁切区域处理两侧渐隐。该问题的失败截图保存在 `test-evidence/history/frontend-menu03-*`。

章节和成就分页按钮的选中样式最初替换了整份覆盖配置，丢失圆角。现通过 `mark_selected` 保留已有的几何和交互颜色，只替换选中常态颜色；交互测试检查每个分页按钮的圆角仍然大于零。

## 背景扩展生成记录

使用 **内置 image_gen 工具**，模式为编辑 / 扩图；未使用 CLI 或外部 API 密钥。输入为 `assets/textures/fly/ui/mainBg2.png`。输出已复制至：

`assets/textures/fly/frontend/background_menu_outpaint.png`

SHA256：`162440824D272C187B17D10125DC8AABC159DBEA0E49D0ED818E2F291548AE2D`。

模型生成的中央区域仍存在细节差异，因此游戏中用原版中央画幅覆盖它，扩展图提供两侧背景。扩展部分属于 AI 补充内容，不作为原作素材事实。

实际提示词：

```text
Use case: precise-object-edit. Asset type: widescreen background for a 2D game main menu. EDIT TARGET: the attached original portrait game illustration, 640 x 960. Expand the canvas ONLY to the left and right into a 16:9 landscape, keeping the original 2:3 portrait as an EXACT, UNCHANGED central rectangle at full canvas height. This means the preserved source occupies the central 37.5 percent of the final canvas width. Do not zoom in, zoom out, redraw, resize relative objects, shift, or recompose the preserved central artwork. Preserve the exact character pose, face, hair, outfit, boots, moon size and position, star positions, cloud horizon height, drawing style, colors and every central detail. Outpaint ONLY the new left and right areas: continue the same black/navy starry sky, sparse cyan star dust, the cropped moon edge where naturally required, and the same hand drawn turquoise/green cloud horizon at the bottom, matching the source boundaries seamlessly. Keep the side areas calm and dark enough for UI text. Do not add a second character or extra moon or extra hair, no large nebula clouds, no text, no logos, no interface, no watermark. The original image must remain visually intact centered at its original aspect ratio; all new content belongs exclusively outside that central rectangle. Final landscape aspect ratio exactly 16:9.
```

## 清理与验证方法

按照用户授权清理模板展示资源。工作树已删除的 Ryougi 示例和演示动画不再由 `assets/content_registry.json`、特效清单引用；另外删除三段 showcase 音频、对应音频注册和五种语言中的 showcase 文本。引擎启动素材与引擎目录保持只读。

最初资源测试确实失败，分别报告示例目录缺失、遗留资源引用和特效引用缺失。修复的是游戏资源注册，没有跳过资源验证。日志 `frontend-ctest04.log`、`frontend-ctest05.log`、`frontend-ctest06.log` 保留失败和修复过程。

`run-replay.ps1` 新增可见窗口模式 `-Interactive`，用于菜单鼠标 / 键盘验证；默认加载时间预算从 25 秒增至 60 秒，原因是此前两次后台启动在加载阶段超过 28 秒总时限。场景断言、成功判定均保留。每次运行仍记录 EXE / 场景 SHA256，旧证据归档到 history。

新增鼠标回放只向 SDL 事件队列发送逻辑坐标转换后的移动和按键事件，不直接调用按钮回调或修改界面状态。保存测试点击真实保存按钮，要求引擎配置事务返回成功；该测试保存当前设置，不改用户偏好。

最终验证结果见本文件后续记录与 `test-evidence/incremental-validation.md`。成就界面的完整展示不等于全部 17 项成就已经在实际游戏中触发；本次不扩大该验收结论。
