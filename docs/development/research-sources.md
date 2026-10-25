# 外部资料

查询日期：2026-10-23（用户时区）。本地原版资源优先。

| 来源 | 发现 | 可信度/验证范围 |
|---|---|---|
| https://apps.apple.com/us/app/flyme2themoon/id467894515 | 官方商品说明列出 60 关，Adventure/Puzzle/Racing/Survival 四模式、滑动单指控制、Game Center | 高；只用于内容范围，不用来猜测物理细节 |
| https://www.youtube.com/watch?v=JTKopXT7r3E | 原 iPhone 第一章流程录像候选 | 一手实机候选；已定位，尚未实际观看 |
| https://www.bilibili.com/video/BV1cx411w7pe/ | 宣传片转载候选 | 尚未实际观看，不计为实机验证 |

不下载、不执行网上的 IPA、模拟器或不可信补丁。

## 实现依赖与格式调查

- Lua 官方源码：https://www.lua.org/ftp/ 。使用 Lua 5.4.9，下载包 SHA-256 `2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6`；仅编译库源代码。MIT 授权保留于 `game/vendor/lua/lua.h`。运行原脚本需要协程等待，不重新实现脚本语言；不开放文件、进程和包加载库。
- FFmpeg CAF 解析器：https://ffmpeg.org/doxygen/8.1/cafdec_8c_source.html 。原版六首 CAF 的 `pakt` 包计数全部为零；本地逐块检查确定 `data` 长度均整除 34 字节，计算得到的包数 × 64 帧恰等于有效帧数与尾部余量之和。在 `out/import-audio` 副本中填入包数后转换 PCM，六个输出的有效采样数均与原记录一致。原文件未修改。转换日志中仍有一个 EOF demux 提示，后续需保留定位并验证全部音频。


## 2026-10-24：原程序静态结构

- Apple Mach-O 格式定义：https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/EXTERNAL_HEADERS/mach-o/loader.h 。用于读取本地 FAT/ARM 分片、段和符号，不加载或执行原程序。
- Apple Objective-C 元数据定义：https://raw.githubusercontent.com/apple-oss-distributions/objc4/main/runtime/objc-runtime-new.h 。结合本地 32 位布局识别类、方法与 ivar。不同运行时版本布局须以本地数据边界检验。
- Capstone 官方 Python 文档：https://www.capstone-engine.org/lang_python.html ；发行记录：https://pypi.org/project/capstone/ 。分析依赖 5.0.9 放在 out/analysis/python，未引入最终游戏运行时。
- 具体推力、出生坐标和碰撞形状结论见 original-flight-research.md，可信依据为本地原程序指令与 JSON 的交叉核对。
- 原 XML 的 60 个 Header 全部含 NoMana；原程序对应 disableMana。因此这批原关卡不需要显示或消耗魔力，不能仅凭存在 RoleManaComponent 就新增耗能规则。
- Background_001 曾未找到图片，现已由本地 BackgroundLayer.load: 确认是被跳过的渐变背景标记，并非缺失图。语音 44-1 的外部搜索仍未得到可核实补充资源。社区模拟器帖不构成行为证据，也未下载或执行其中程序。
