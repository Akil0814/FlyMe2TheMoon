# 场景生命周期与安全边界

本文面向修改场景框架的开发者，解释状态转换、对象所有权和异常清理。阅读重点是每个阶段由谁负责、何时可以修改对象集合，以及失败后哪些步骤仍须完成。

`SceneManager` 协调状态转换、缓存和异常恢复；`Scene` 持有对象并实现不可重写的输入、更新、渲染及清理流程。游戏只实现 `on_enter`、`on_exit`、`on_reset` 和专用扩展钩子。使用示例和允许增删的完整阶段表见[场景系统](../../user-guide/scene/scene.md)。

## 状态与所有权

场景的状态为 Inactive → Entering → Active → Exiting → Inactive；Reset 只从 Inactive 经 Resetting 返回 Inactive。销毁准备从 Inactive 经 PreparingDestruction 到 PreparedForDestruction，完成后不能重新进入。`SceneManager` 的 Running 只对当前场景分发输入、更新和渲染；缓存中的 Inactive 场景不再更新。

`SceneFactory` 按 SceneKey 持有 `unique_ptr<Scene>`，不同键可以使用同一个具体类型。场景独占世界对象和 UI 根节点；UI 容器独占自己的子节点。对象借用、控制目标、物理注册、监听和服务绑定都不延长所有者的寿命。

默认场景不创建固定步、物理或相机；配置 `PhysicsWorld` 必须同时配置 `FixedStepRuntime`。`GameplayScene` 默认有固定步与控制上下文，玩法碰撞需显式启用并依赖物理。运行时运行时上下文借用 `Application` 的渲染器、内容注册表、字体和开发面板；它们必须活到全部缓存场景销毁之后。

## 路由和回调顺序

| 操作 | 顺序 |
| --- | --- |
| 新建 / 进入 | 构造候选 → 绑定运行时上下文、玩家与 UI 设备访问 → 绑定活动特效、查询和场景运行时 → 绑定请求观察者 → `on_enter` → 缓存候选并提交当前场景 |
| 离开 | 解除请求观察者 → 重置输入交互 → 解除场景运行时 → 解除查询与特效 → `on_exit` → 标记对象回收 → 取消相机活动 → Inactive |
| Reuse | 离开当前场景 → 进入已有目标；保留未销毁业务对象，控制目标需要重新绑定 |
| Reset | 离开当前场景 → 输入 / 暂停 / 固定步 / 相机重置 → `on_runtime_reset` → `on_reset` → 标记对象回收 → 进入目标 |
| Recreate | 先构造候选 → 离开当前场景 → 销毁目标旧缓存 → 绑定并进入候选 → 缓存并提交 |
| 销毁缓存 | 确保 Inactive 且服务已解除 → 取消相机活动 → 全部对象逻辑回收 → 输入重置 → 清空运行时上下文 → 析构 |

目标相机槽会在跨实例进入以及 Reset / Recreate 时重置；Reuse 当前实例只取消活动轨迹和混合，保留静态姿态。未声明拥有的相机槽不随此次进入重置。相机配置和外部服务接入应放在 `on_enter`，不能在候选构造阶段修改当前场景仍使用的共享服务。

`on_enter` 失败不补调 `on_exit`：候选只进入过 Entering，管理器撤销已建立的引擎绑定并销毁它。游戏的外部订阅和回调必须由 RAII 或局部回滚撤销。正常退出时运行时服务已经解绑，清理应使用本场景持有的接口，不能假设全局活动服务仍指向本场景。

## 更新与对象变更

一次更新为 `on_before_update` → 普通对象更新 → UI 呈现动画 → 固定步 → 物理 `finalize_frame` → 相机 → `on_after_update` → 标记对象回收。`GameplayScene` 的每个实际固定步先交付控制命令，再执行 `on_game_fixed_update`，最后进入可选物理模拟步。

`add_object` 同步完成所有权接收、接口和物理注册，再通知 `on_scene_object_registered`。注册失败会回滚该对象；注册钩子允许嵌套添加独立对象，但整个注册过程禁止清空场景，防止释放正在注册的对象。对象更新、UI 呈现、渲染、对象查询与物理模拟步中禁止同步增删；场景前后更新钩子、模拟步前的场景固定步钩子及输入快照回调可以修改对象集合。物理查询的 `noexcept` 回调必须只读。

`destroy()` 是标记。更新末尾、退出后及重置后都执行回收；不存在“退出场景必须等下一帧才回收”的要求。回收顺序为：对象存活时通知移除 → 注销物理并解除参与者绑定 → 清除输入 / 更新 / 物理登记 → 释放 `GameObject` 或 UI 根节点所有权。`GameplayScene` 在游戏移除扩展前完成控制目标解绑和玩法碰撞解绑。一个对象的清理回调抛异常不会阻止本批其他对象回收。

## 异常恢复与关闭

场景逻辑异常先记录原始边界，再解除并退出当前场景，销毁故障场景及其对象。只有全部必要清理成功，才调用失败路由工厂、构造并激活恢复场景，避免恢复构造与故障对象同时占用外部资源。清理抛异常时继续所有可执行清理，汇总诊断并进入 Faulted，不调用路由工厂。恢复场景失败直接进入 Faulted，不递归恢复。

普通业务异常与后续清理失败都保留诊断；底层 `RenderBackendError` 优先传播到应用故障退出，不再依赖同一个渲染器展示恢复界面。关闭流程隔离各个阶段，返回的失败结果具有粘性，重复关闭不会把失败覆盖成成功。

`Application` 先停止输入并关闭 `SceneManager`。后者退出当前场景、清除调试和屏幕特效、重置相机、销毁全部缓存场景，最后关闭控制器并清空应用级玩家。随后 `Application` 关闭开发面板、释放场景运行时上下文、存档、字体、项目和内置资源，最后销毁 Renderer / 窗口并关闭 SDL。析构兜底只释放资源和断开借用，不调用已经析构的游戏派生类回调。

## 回归与示例验证

核心回归位于 `tests/scene/scene_core_tests.cpp`、`scene_lifecycle_regression_tests.cpp`、`scene_factory_failure_tests.cpp`，集成回归覆盖控制器、相机与 `Application` 关闭流程。

`tests/scene/compile_scene_documentation.cmake` 从实际 Markdown 提取完整场景、对象和服务调用示例生成编译单元，以构建目标 `scene_documentation_examples` 检查公开 API；诊断通过 `#line` 返回原文位置。Markdown 修改触发重新提取，不维护独立复制的示例。
