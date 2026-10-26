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

## 已执行的界面验证

- `build-frontend08.log` 编译成功，`frontend-ctest09.log` 5/5 通过，包含菜单键盘路由、章节标签圆角、17 项成就三页路由及当前页焦点、原徽章资源与遮罩、标题与底图对齐。
- `frontend-menu03`、`menu-artwork-en`：中英文主菜单实际运行通过，原版人物脚部与标题接合，竖线问题已消除。
- `frontend-selection03`、`frontend-chapter2`：实际点击选关、切至第 2 章通过，截图确认当前章节的圆角与焦点。
- `frontend-achievements03`、`frontend-achievements-page2`、`frontend-achievements-page3`：三页实际点击通过，第 3 页末次复测确认数字没有拆行、当前页持有焦点。
- `menu-selection`、`achievement-page01`：键盘入口回放通过。后者仍要求存档中成就 101 已获得，没有削弱旧断言。
- `frontend-settings-save03`：真实保存按钮点击后，配置服务返回成功，记录 `settings_saved=true`；`frontend-settings-back03` 实际回到 scene=1。设置测试保存当前值，不更改用户偏好。
- `frontend-protected-check.json`：644 项只读基线检查，changed=0。当前引擎、公共 API、原引擎文档相对 HEAD 无差异。

各次增量构建的 EXE / 场景 SHA256 以对应 `*-run.json` 为准。最后一次焦点修复后重跑完整 CTest、中文菜单、第 2 章、第 3 页成就；其余界面保留此前成功记录。

用户随后要求去除“60 个关卡 · 一段月光下的旅程”“让旅程更合心意”等额外文案。主菜单副标题、页脚、成就页引导语和设置页引导语已从界面及五种语言表中移除；导入脚本也不再重新生成这些文案。`build-frontend09.log` 记录合并后视觉测试漏引头文件的编译失败；补回引用后，`build-frontend10.log` 成功，`frontend-ctest10.log` 5/5 通过。中文菜单、成就第 3 页和设置页均重新实际运行通过，截图逐页确认文案已消失。旧截图保留在同名场景的 history 中。

## 选关成绩星星的实心/空心绑定修正（2026-10-25）

用户指出已获得的星星仍为空心。核实当前 `starUp*` 图片中心透明，且与参考 HD 图片完全一致，排除导入损坏。原 `LevelSelectButtonV2.initSprites`（0xd5bbc）明确将 `starDown*` 赋给 `starFullSpriteFrame_`、`starUp*` 赋给 `starEmptySpriteFrame_`、`starGrey*` 赋给 `starGreySpriteFrame_`；`refreshSprites`（0xd702c）在 0xd7358..0xd7380 根据 nStars 选择实心或空心。此前复刻把 starUp 当作已获得星星，同时漏导入 starDown，属于游戏贴图状态映射错误。

现已补入四章的原版 `starDownO/G/Y/P`，文件均与参考 HD 源逐字节一致；前端导入器也保留此项。已通关关卡按星数绘制实心 starDown 与空心 starUp，未通关使用 starGrey；锁定节点保留当前桌面布局的灰星和低透明度。没有改动存档成绩、关卡解锁、按钮布局或星星尺寸。原作锁定/新关隐藏星星及选中边框等完整视觉策略仍未逐项复原。

Debug 主程序及游戏测试目标构建通过。针对本次修改执行 `fly_idle_visuals` 和 `fly_selection_input`，两项均通过（36.23 秒）；新增视觉检查覆盖四章、未通关及 0..3 星、锁定/解锁状态的纹理绑定、三颗星数量与透明度。测试预览 `out/qa/selection-stars-audit/preview.bmp` 已查看：四行对应四章，五列依次为未通关、0、1、2、3 星。它是实际 SDL 软件绘制，不包含完整场景交互或原生窗口截图；本轮未另跑窗口回放。日志为同目录 `ctest01.log`，原方法摘录为 `initSprites.asm`、`refreshSprites.asm`。

首轮构建发现新预览代码误调用 UiButton 不存在的 update 方法；移除该调用后重新构建、运行上述测试通过，没有跳过相关检查。

## 选关节点按关卡类型取色（2026-10-26）

圆点、星星和独立锁图标统一读取 `catalog.selection.levels[id][2]`，使用原 `LevelSelectData.json` 的关卡类型：1 紫色（P）、2 黄色（Y）、3 橙色（O）、4 绿色（G）。颜色在节点创建时确定，不再按章节整页取色；卡片布局、星数状态、解锁逻辑和锁定透明度保持现有行为。

原版依据为 `LevelsDataV2.typeOfLevel:`（0xea41c）读取配置第三项，`LevelSelectPageV2.initWithPageID:` 将其传给 `LevelSelectButtonV2.initWithLevelID:levelType:`，后者与 `initSprites` 完成上述类型到贴图后缀的映射。首章序列为 1–3 橙、4–6 绿、7–9 黄、10–13 绿、14–15 紫。

`fly_idle_visuals` 检查全部 60 关、未通关及 0–3 星、锁定/解锁状态的实际绘制命令，要求圆点、锁和三颗星均使用该关颜色，并保留原星级贴图与透明度。`out/qa/selection-stars-audit/preview.bmp` 现在展示首章 15 关，混合星级与锁定状态。
