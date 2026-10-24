# 通用存档服务

本文面向维护存档模块或设计游戏存档格式的开发者。它解释内存数据、文件替换与失败诊断如何分工；调用示例先处理每一步结果，再继续依赖该结果的操作。

`engine/save/` 提供与具体 Gameplay 数据无关的类型化存档能力。引擎负责名称校验、内存文档、JSON 编码、可靠写入和恢复；Gameplay 负责数据键、业务校验、数据迁移以及保存时机。

## 基本使用

`Application` 会使用 `PathManager::saves()` 初始化 `SaveService`。调用方只提供逻辑存档名，引擎自动在固定目录补充 `.json`：

```cpp
#include "engine/save/save_service.h"
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

// 服务已由 Application 初始化；本函数创建一个新存档。
// 名称已存在等失败由调用方处理，不把创建失败当作覆盖许可。
std::expected<void, elysia::save::SaveFailure> save_progress()
{
    auto* saves = ELYSIA_SAVE;
    auto created = saves->create("slot_01");
    if (!created) return std::unexpected(created.error());
    auto level = saves->set("slot_01", "player.level", std::int64_t{12});
    if (!level) return std::unexpected(level.error());
    auto stages = saves->set("slot_01", "progress.unlocked_stages",
        std::vector<std::string>{"forest_01", "forest_02"});
    if (!stages) return std::unexpected(stages.error());
    return saves->commit("slot_01");
}
```

该函数完整展示结果传播，不是独立程序。调用方检查最终结果并记录诊断；中间失败可能留下已打开的内存存档，需要按业务选择重试或显式丢弃。读取已有存档前必须显式打开：

```cpp
auto opened = ELYSIA_SAVE->open("slot_01");
if (!opened) return;

auto level = ELYSIA_SAVE->get<std::int64_t>(
    "slot_01",
    "player.level");
```

服务可以同时缓存多个打开的存档。修改会更新对应文档的未保存（dirty）状态和 revision；设置相同值不会产生新 revision。`close()` 默认拒绝丢弃脏文档，必须先 `commit()`，或者显式使用 `SaveClosePolicy::DiscardChanges`。

## 数据类型

`SaveData` 只接受以下精确类型，不执行数值或字符串隐式转换：

- `bool`、`std::int64_t`、`double`、`std::string`
- `std::vector<bool>`、`std::vector<std::int64_t>`、`std::vector<double>`、`std::vector<std::string>`

所有 `double` 必须有限。复杂 Gameplay 对象应由 Gameplay codec 展开为稳定数据键；运行时指针、场景或实体实例不能直接写入。

## 文件格式

JSON format version 1 使用独立类型表，因此空数组也能在重启后保持精确类型：

```json
{
  "format_version": 1,
  "types": {
    "player.level": "int64",
    "progress.unlocked_stages": "string_array"
  },
  "values": {
    "player.level": 12,
    "progress.unlocked_stages": []
  }
}
```

根对象只允许 `format_version`、`types`、`values`，两个数据键集合必须完全一致。游戏业务数据版本 应作为普通值保存，例如 `gameplay.schema_version`。

## 名称与恢复

存档名必须由 1–64 个 ASCII 字母、数字、`_` 或 `-` 组成，不能携带扩展名或路径。`slot_01` 对应：

```text
player_data/saves/slot_01.json
player_data/saves/slot_01.json.tmp
player_data/saves/slot_01.json.bak
```

提交时先写入并验证临时文件，再轮换备份并替换主文件。打开损坏主文件时会将其归档为 `.corrupt`，随后依次尝试 `.tmp` 和 `.bak`。没有有效副本时返回错误，不创建空存档。未来 format version 会原样保留并直接返回 `UnsupportedFormatVersion`，不会降级到旧备份。

第一版只提供同步 IO。自动存档触发、异步队列和项目固定槽位枚举属于后续 Gameplay/引擎编排工作。

## 失败诊断与提交边界

`SaveFailure` 保留 `error`、`save_name` 和 `key`；消息、来源及附加条目统一保存在 `diagnostic`，不再提供独立的 `message`。`SaveOpenResult::warning` 是 `optional<SaveFailure>`，成功恢复也可携带损坏、归档和恢复副本的诊断。

磁盘操作失败可携带 `persistence`：操作阶段、主文件/临时文件/备份路径及已知存在状态，以及 `NotRequired`、`Succeeded`、`Failed`、`Unknown` 恢复结果。`Unknown` 不能当作不存在；存在不能当作内容有效。原始失败始终保留，恢复和状态查询失败作为后续诊断条目追加。

写入先准备序列化内容；主文件缺失时先检查并提升已有有效临时副本，再写临时文件并检查写入、flush、close 和内容校验，之后才轮换备份和替换主文件。没有有效临时副本且主文件缺失时保留已有备份；替换失败后恢复旧主文件，失败重试也不能提前删除唯一恢复副本。临时副本提升失败保留其原内容。

访问、权限及读取失败返回错误，不将其当作损坏文件归档。只有确认内容无效时才进入归档和备用副本恢复。损坏主文件归档失败时停止加载，保留原文件和完整上下文。

可预期 I/O 和数据失败返回 `expected`；内存分配及内部契约异常继续传播。异常发生在主文件已移至备份之后时，先受保护地尝试恢复，再传播原异常；恢复失败由清理边界记录。Store 不为普通返回失败重复记录日志，由消费结果的调用边界负责记录和决定重试、提示或退出。

提交失败保留内存未保存（dirty）状态和 revision。上述替换流程不提供断电持久性、跨进程互斥或并发写入保证。

### 保存前的临时副本保护

保存前先检查主文件。主文件缺失且已有 `.tmp` 时，复用加载校验：有效副本先提升为主文件，再开始新写入；确认损坏才允许重写，访问失败或未来版本则停止。提升失败保留副本原内容。有效 `.tmp` 优先于旧 `.bak`，后续正常轮换以恢复后的主文件为备份；提升本身不代表新内容已经保存成功。

恢复操作的原始结果在构造诊断前保存。恢复后的状态查询或诊断分配再次异常时，日志仍保留主失败和此前恢复错误；必要恢复完成后传播原异常，成功恢复不重复执行。持续内存不足时日志可降级，清理边界不得抛出二次异常。
