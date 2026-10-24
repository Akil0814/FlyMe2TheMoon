# 参考资料分析

## 调查基线

任务依据为实际存在的 `docs/task.md`；其第 9 节称根目录 task.md，与实际路径不符，按用户指定文件执行，不修改任务书。仓库没有 `.git`，以 `test-evidence/protected-files-baseline.csv` 记录 engine 与 docs/engine 全部文件 SHA-256，最终重新核查。

`game/tools/analyze_reference.py` 对全部参考文件建档。结果见 `reference-inventory.csv` 与 `reference-summary.json`：3337 文件，188528722 字节。07_iOS包与第三方仅归档，不执行其中二进制，不打包其第三方 SDK。原版版权素材仅用于用户提供资料的本地学习验证，不据此取得公开分发授权。

## 已核实（来自本地数据）

- 60 张 XML 地图，60 份 Lua 脚本；26 份脚本因重复被移入 99_重复文件，通过整理清单的 original_path 恢复引用。
- 地图包含 Background/Farground/StaticObject/Texture/DynamicObject/Item/Foreground/Closeground/Area。共 206 个静态轮廓对象、1351 个动态对象、3325 个拾取对象、46 个触发区域；每关恰有一个月亮、三颗星。
- 17 种动态 SeqID：8、24、25、101、105–117；拾取物 3 为星、4/5 为大小水晶。101 为目标月亮。105–107 移动石块，108–110 旋转机关，111 刺，115 传送门（图形与 XML 交叉证据）。其他敌人及风场需继续以动画、Info 参数和文本核实。
- 角色配置 maxF=28、maxP=51、mass=1、damping=0.1、maxXSpeed=5、yGravity=-10；转换到引擎 EU 单位仍需校准。原始 JSON 部分有尾逗号及非标准转义，导入器必须记录并规范化，不能修改原始文件。
- `edgeData.dat` 为小端 float32 XY 点对；meta 的 offset 为字节偏移，count 为点数，支持逐条边界范围校验。地图坐标 Y 向上，引擎 Y 向下，需要统一翻转。
- LevelDependency 提供全部解锁依赖；LevelSelectData 提供四个章节的节点、连线与模式。roleConfig 的开发开关不能作为正式游戏默认解锁全部关卡的依据。
- 中文剧情明确四种试炼：险阵（冒险）、疾阵（竞速、关门）、幻阵（解谜）、困阵（等待机关开启的生存）。模式由关卡机关组合实现，不凭空添加独立倒计时。
- 三种语言剧情与 UI、17 项成就、编号语音、六个 CAF 文件、人物动作与表情、标题/结局素材均可获得。剧情脚本存在条件分支、区域事件、一次性观看标记、镜头移动及第一关操作演示。

## 推断与不确定性

- 碰撞轮廓是否带单向、伤害规则及各类机关默认值未完整提供，须以运行表现和实机资料验证。
- levelScore.svrs 是每关参考数值，不能未经证据将其宣称为原作评分公式。
- 动态对象根级 PNG 有编辑器辅助色块，正式表现应选动画子帧，不直接将编辑器预览当游戏外观。
- HD 与标准分辨率的尺寸、对象锚点、旋转方向、视差倍率需要垂直切片实际校准。
- Game Center 网络排行榜不可作为本地系统等价物；成就规则可本地实现，联网服务状态单独记录。
- 当前仅定位到实机视频链接，尚未逐帧验证，不把搜索摘要当作已观看实机证据。

## 引擎接入结论

已阅读初始化、Scene 生命周期、GameObject 绘制、输入、物理、资源、动画、音频、UI、相机、存档及本地化使用指南。保留现有 Application/StartupLoading 路由。对象由场景持有；物理经 PhysicsParticipant 和固定步推进；资源由 manifest 与 ResourceService 加载；菜单用引擎 UI；存档用 SaveService。引擎没有传统组件容器，采用它文档规定的能力接口组合，不自建 ECS。

公开物理形状仅 AABB 与圆，但刚体支持旋转。优先实验将原始轮廓转换为短静态旋转矩形，或离线栅格化到引擎 TileCollisionWorld，保留误差记录；不访问 Box2D 原生 ID、不修改引擎。
