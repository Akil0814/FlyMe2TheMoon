# Fly2theMoon PNG 解析说明

本目录包含原始图片、完整图集以及由 Cocos2d/TexturePacker plist 恢复的独立 sprite。原 PVR、CCZ、plist 和 PNG 均未修改。

## 统计

- 独立图片：137
- 完整图集：47（PVR/CCZ 36，PNG 11）
- 拆分帧：2683
- 标准化 CgBI PNG：140
- 旋转帧：153
- 恢复透明画布的裁剪帧：602
- 资源 PNG 合计：2867

## 处理规则

- PVR/CCZ 使用 Imagination PVRTexToolCLI 5.7 解码。
- CCZ 在临时目录中解压，临时 PVR 已清理。
- rotated 帧已反旋转，并按 sourceColorRect 放回 sourceSize 透明画布。
- PNG解析清单.csv 记录每个输出的来源、尺寸和 SHA-256。
