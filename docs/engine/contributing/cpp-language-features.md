# C++11–23 语言与标准库特性统计

本文面向希望了解项目语言用法或维护扫描器的开发者。它说明哪些特性能够从源码稳定识别、如何复现统计，以及数字不能代表什么。

本页盘点 Elysia Engine 第一方源码中可重复识别的现代 C++ 用法。项目通过[CMake 配置](../../../CMakeLists.txt)要求 C++23。

## 范围与计数口径

- 扫描 `engine/`、`game/`、`tests/` 中的 `.h`、`.hpp`、`.cpp` 文件。
- 排除 `thirdparty/`、构建目录、文档、脚本和生成产物。
- 扫描器先隐藏注释、普通/原始字符串和字符字面量，再匹配注册过的语法或标准库符号。
- `engine`、`game`、`tests` 列表示各目录中的模式命中数；“文件数”是至少命中一次的文件数。
- 同一行可以同时体现多项特性，例如 `constexpr auto` 会分别计入 `constexpr` 和 `auto`。
- 数字表示词法/语法模式规模，不表示运行时调用次数、性能权重或代码质量。

## 当前统计

<!-- BEGIN GENERATED: cpp-language-feature-stats -->

本区块由 `scripts/audit_cpp_language_features.ps1` 根据当前工作树生成；共扫描 763 个第一方 C++ 源文件。
“命中”表示过滤注释及字符串/字符字面量后的检测模式次数，不等同于运行时调用次数。

### C++11

| ID | 分类 | 特性 | engine | game | tests | 总命中 | 文件数 | 代表性位置 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cpp11-array` | 标准库 | `std::array` | 59 | 42 | 81 | 182 | 85 | [源码](../../../game/showcase/ui/hud_demo_state.h) |
| `cpp11-auto` | 语言 | `auto` 类型推导 | 1340 | 461 | 1554 | 3355 | 309 | [源码](../../../game/navigation/main_menu_scene.cpp) |
| `cpp11-chrono` | 标准库 | `std::chrono` | 38 | 1 | 18 | 57 | 26 | [源码](../../../tests/audio/sound_playback_scheduler_tests.cpp) |
| `cpp11-concurrency` | 标准库 | 线程、同步与原子操作 | 55 | 0 | 20 | 75 | 12 | [源码](../../../engine/tools/termination_manager.h) |
| `cpp11-constexpr` | 语言 | `constexpr` | 533 | 43 | 56 | 632 | 128 | [源码](../../../engine/application/application.cpp) |
| `cpp11-decltype` | 语言 | `decltype` | 14 | 2 | 4 | 20 | 15 | [源码](../../../tests/camera/camera_manager_tests.cpp) |
| `cpp11-declval` | 标准库 | `std::declval` | 0 | 0 | 7 | 7 | 2 | [源码](../../../tests/camera/camera_manager_tests.cpp) |
| `cpp11-defaulted-functions` | 语言 | 显式默认函数 | 168 | 6 | 0 | 174 | 135 | [源码](../../../game/navigation/main_menu_scene.h) |
| `cpp11-deleted-functions` | 语言 | 显式删除函数 | 81 | 4 | 0 | 85 | 28 | [源码](../../../engine/camera/camera_manager.h) |
| `cpp11-enum-class` | 语言 | 有作用域枚举 | 163 | 11 | 3 | 177 | 109 | [源码](../../../engine/animation/animation_registration_failure.h) |
| `cpp11-exception-pointers` | 标准库 | 异常保存与重新传播 | 59 | 0 | 2 | 61 | 12 | [源码](../../../tests/application/application_startup_exception_tests.cpp) |
| `cpp11-final` | 语言 | `final` | 65 | 46 | 115 | 226 | 136 | [源码](../../../game/showcase/ui/ui_component_gallery_scene.h) |
| `cpp11-fixed-width-integers` | 标准库 | 定宽整数类型 | 342 | 13 | 71 | 426 | 132 | [源码](../../../engine/audio/sound_playback_types.h) |
| `cpp11-function` | 标准库 | `std::function` | 64 | 57 | 31 | 152 | 79 | [源码](../../../engine/animation/animation.h) |
| `cpp11-initializer-list` | 标准库 | `std::initializer_list` | 9 | 1 | 1 | 11 | 10 | [源码](../../../engine/resources/atlas/atlas.h) |
| `cpp11-lambda` | 语言 | Lambda 表达式 | 502 | 187 | 445 | 1134 | 181 | [源码](../../../game/showcase/effects/effects_showcase_scene.cpp) |
| `cpp11-math-functions` | 标准库 | C++11 数学函数 | 205 | 14 | 5 | 224 | 51 | [源码](../../../engine/application/application.cpp) |
| `cpp11-move-forward` | 标准库 | 移动与完美转发工具 | 854 | 222 | 183 | 1259 | 223 | [源码](../../../game/navigation/main_menu_scene.cpp) |
| `cpp11-new-algorithms` | 标准库 | C++11 非修改序列算法 | 16 | 0 | 3 | 19 | 9 | [源码](../../../engine/ui/window/ui_window.cpp) |
| `cpp11-noexcept` | 语言 | `noexcept` | 3123 | 131 | 74 | 3328 | 394 | [源码](../../../engine/scene/scene_manager.h) |
| `cpp11-nullptr` | 语言 | `nullptr` | 641 | 123 | 480 | 1244 | 259 | [源码](../../../engine/scene/scene_manager.h) |
| `cpp11-override` | 语言 | `override` | 383 | 129 | 377 | 889 | 140 | [源码](../../../game/showcase/ui/ui_component_gallery_scene.h) |
| `cpp11-quick-exit` | 标准库 | `std::_Exit` | 0 | 0 | 3 | 3 | 3 | [源码](../../../tests/application/application_startup_exception_tests.cpp) |
| `cpp11-random` | 标准库 | 随机数工具 | 2 | 0 | 0 | 2 | 2 | [源码](../../../engine/tools/random_generator.cpp) |
| `cpp11-range-for` | 语言 | 基于范围的 `for` | 415 | 69 | 179 | 663 | 193 | [源码](../../../game/showcase/ui/ui_component_gallery_content.cpp) |
| `cpp11-reference-wrappers` | 标准库 | 引用包装工具 | 0 | 0 | 17 | 17 | 6 | [源码](../../../tests/scene/scene_recovery_tests.cpp) |
| `cpp11-smart-pointers` | 标准库 | 智能指针与 `std::make_shared` | 194 | 46 | 25 | 265 | 113 | [源码](../../../engine/ui/core/ui_child_host.h) |
| `cpp11-static-assert` | 语言 | `static_assert` | 13 | 0 | 49 | 62 | 20 | [源码](../../../engine/scene/scene_manager.h) |
| `cpp11-string-conversion` | 标准库 | 字符串与数值转换 | 32 | 19 | 35 | 86 | 43 | [源码](../../../game/showcase/gameplay/runtime/demo_combat.cpp) |
| `cpp11-system-error` | 标准库 | 系统错误码工具 | 56 | 0 | 28 | 84 | 21 | [源码](../../../engine/builtin/resources/builtin_asset_catalog.cpp) |
| `cpp11-time-formatting` | 标准库 | `std::put_time` | 2 | 0 | 0 | 2 | 1 | [源码](../../../engine/tools/logger.cpp) |
| `cpp11-tuple` | 标准库 | 元组工具 | 6 | 0 | 0 | 6 | 4 | [源码](../../../engine/ui/core/ui_child_host.h) |
| `cpp11-type-alias` | 语言 | `using` 类型别名 | 123 | 10 | 12 | 145 | 92 | [源码](../../../engine/animation/animation.h) |
| `cpp11-type-index` | 标准库 | `std::type_index` | 3 | 0 | 0 | 3 | 2 | [源码](../../../engine/ui/style/ui_theme_style_resolver.h) |
| `cpp11-unordered-containers` | 标准库 | 无序容器与哈希 | 65 | 3 | 2 | 70 | 39 | [源码](../../../engine/builtin/resources/builtin_asset_cache.h) |
| `cpp11-variadic-templates` | 语言 | 可变参数模板 | 10 | 2 | 0 | 12 | 6 | [源码](../../../engine/ui/core/ui_child_host.h) |

### C++14

| ID | 分类 | 特性 | engine | game | tests | 总命中 | 文件数 | 代表性位置 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cpp14-chrono-literals` | 标准库 | 时间字面量 | 0 | 1 | 3 | 4 | 4 | [源码](../../../tests/audio/sound_playback_scheduler_tests.cpp) |
| `cpp14-exchange` | 标准库 | `std::exchange` | 6 | 1 | 11 | 18 | 11 | [源码](../../../engine/gameplay/control/controller_manager.cpp) |
| `cpp14-generic-lambda` | 语言 | 泛型 Lambda | 79 | 9 | 61 | 149 | 53 | [源码](../../../game/showcase/gameplay/gameplay_demo_scene_base.cpp) |
| `cpp14-init-capture` | 语言 | Lambda 初始化捕获 | 2 | 4 | 0 | 6 | 5 | [源码](../../../engine/ui/widgets/ui_button.cpp) |
| `cpp14-make-unique` | 标准库 | `std::make_unique` | 75 | 156 | 185 | 416 | 73 | [源码](../../../engine/animation/animation_service.cpp) |
| `cpp14-type-trait-aliases` | 标准库 | 类型特征 `_t` 别名 | 15 | 2 | 2 | 19 | 12 | [源码](../../../engine/ui/core/ui_child_host.h) |

### C++17

| ID | 分类 | 特性 | engine | game | tests | 总命中 | 文件数 | 代表性位置 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cpp17-any` | 标准库 | `std::any` | 2 | 0 | 1 | 3 | 2 | [源码](../../../tests/scene/scene_core_tests.cpp) |
| `cpp17-apply` | 标准库 | `std::apply` | 1 | 0 | 0 | 1 | 1 | [源码](../../../engine/scene/scene_manager.h) |
| `cpp17-as-const` | 标准库 | `std::as_const` | 3 | 0 | 0 | 3 | 1 | [源码](../../../engine/object_query/game_object_query_service.h) |
| `cpp17-clamp` | 标准库 | `std::clamp` | 70 | 3 | 0 | 73 | 37 | [源码](../../../engine/camera/camera_controller.cpp) |
| `cpp17-ctad` | 语言 | 类模板实参推导 | 5 | 5 | 44 | 54 | 26 | [源码](../../../tests/application/application_presentation_settings_tests.cpp) |
| `cpp17-filesystem` | 标准库 | `std::filesystem` | 372 | 0 | 371 | 743 | 134 | [源码](../../../engine/io/path/path_manager.h) |
| `cpp17-if-constexpr` | 语言 | `if constexpr` | 43 | 2 | 0 | 45 | 14 | [源码](../../../engine/camera/camera_manager.cpp) |
| `cpp17-if-initializer` | 语言 | `if` 初始化语句 | 147 | 3 | 13 | 163 | 70 | [源码](../../../engine/animation/runtime/animation_manager.cpp) |
| `cpp17-inline-variables` | 语言 | 内联变量 | 234 | 24 | 0 | 258 | 21 | [源码](../../../engine/core/render/colors.h) |
| `cpp17-invoke` | 标准库 | `std::invoke` 与结果类型特征 | 9 | 0 | 0 | 9 | 2 | [源码](../../../engine/object_query/game_object_query_service.h) |
| `cpp17-maybe-unused` | 语言 | `[[maybe_unused]]` | 19 | 0 | 0 | 19 | 3 | [源码](../../../engine/scene/scene.h) |
| `cpp17-nested-namespace` | 语言 | 嵌套命名空间定义 | 586 | 155 | 98 | 839 | 682 | [源码](../../../engine/animation/animation.h) |
| `cpp17-nodiscard` | 语言 | `[[nodiscard]]` | 1617 | 78 | 45 | 1740 | 305 | [源码](../../../engine/animation/animation.h) |
| `cpp17-optional` | 标准库 | `std::optional` / `std::nullopt` | 556 | 37 | 41 | 634 | 190 | [源码](../../../engine/animation/animation.h) |
| `cpp17-scoped-lock` | 标准库 | `std::scoped_lock` | 7 | 0 | 0 | 7 | 1 | [源码](../../../engine/config/config_service.cpp) |
| `cpp17-size` | 标准库 | `std::size` | 0 | 0 | 1 | 1 | 1 | [源码](../../../tests/loading/resource_load_plan_validator_tests.cpp) |
| `cpp17-string-view` | 标准库 | `std::string_view` | 471 | 17 | 118 | 606 | 159 | [源码](../../../engine/animation/animation_service.h) |
| `cpp17-structured-bindings` | 语言 | 结构化绑定 | 68 | 3 | 6 | 77 | 31 | [源码](../../../game/showcase/gameplay/runtime/demo_combat.cpp) |
| `cpp17-type-trait-variables` | 标准库 | 类型特征 `_v` 变量模板 | 42 | 2 | 21 | 65 | 25 | [源码](../../../engine/camera/camera_manager.cpp) |
| `cpp17-variant` | 标准库 | 变体类型工具 | 63 | 2 | 11 | 76 | 33 | [源码](../../../engine/ui/widgets/ui_button.cpp) |

### C++20

| ID | 分类 | 特性 | engine | game | tests | 总命中 | 文件数 | 代表性位置 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cpp20-bit-operations` | 标准库 | 位操作工具 | 1 | 0 | 0 | 1 | 1 | [源码](../../../engine/tools/debug_draw.cpp) |
| `cpp20-concepts` | 语言 | Concepts 与 `requires` 子句 | 15 | 0 | 3 | 18 | 5 | [源码](../../../engine/save/save_data.h) |
| `cpp20-designated-initializers` | 语言 | 指定初始化器 | 91 | 84 | 458 | 633 | 119 | [源码](../../../game/navigation/main_menu_scene.cpp) |
| `cpp20-erase` | 标准库 | `std::erase` / `std::erase_if` | 45 | 2 | 0 | 47 | 18 | [源码](../../../engine/physics/physics_world.cpp) |
| `cpp20-format` | 标准库 | `std::format` | 0 | 4 | 0 | 4 | 1 | [源码](../../../game/showcase/camera/camera_showcase_scene.cpp) |
| `cpp20-jthread` | 标准库 | `std::jthread` / 停止令牌 | 0 | 0 | 4 | 4 | 2 | [源码](../../../tests/application/application_lifecycle_tests.cpp) |
| `cpp20-lerp` | 标准库 | `std::lerp` | 1 | 0 | 0 | 1 | 1 | [源码](../../../engine/gameplay/ui/world_bar.cpp) |
| `cpp20-numbers` | 标准库 | 数学常量 | 1 | 0 | 0 | 1 | 1 | [源码](../../../engine/physics/physics_debug_draw.cpp) |
| `cpp20-ordering-types` | 标准库 | 比较类别类型 | 4 | 0 | 0 | 4 | 2 | [源码](../../../engine/physics/collision/collision_target.h) |
| `cpp20-ranges` | 标准库 | Ranges 库 | 42 | 4 | 5 | 51 | 23 | [源码](../../../engine/physics/physics_world.cpp) |
| `cpp20-remove-cvref` | 标准库 | `std::remove_cvref_t` | 19 | 0 | 0 | 19 | 6 | [源码](../../../engine/save/save_service.h) |
| `cpp20-source-location` | 标准库 | `std::source_location` | 197 | 0 | 19 | 216 | 69 | [源码](../../../engine/application/lifecycle/application_termination_logging.h) |
| `cpp20-span` | 标准库 | `std::span` | 60 | 7 | 13 | 80 | 52 | [源码](../../../engine/builtin/object/engine_character.cpp) |
| `cpp20-standard-concepts` | 标准库 | 标准库 Concepts | 37 | 0 | 0 | 37 | 5 | [源码](../../../engine/object_query/game_object_query_service.h) |
| `cpp20-string-prefix-suffix` | 标准库 | `starts_with` / `ends_with` | 10 | 0 | 15 | 25 | 11 | [源码](../../../tests/save/save_service_tests.cpp) |
| `cpp20-three-way-comparison` | 语言 | 三路比较 | 16 | 0 | 0 | 16 | 7 | [源码](../../../engine/physics/collision/collision_target.h) |

### C++23

| ID | 分类 | 特性 | engine | game | tests | 总命中 | 文件数 | 代表性位置 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `cpp23-expected` | 标准库 | `std::expected` / `std::unexpected` | 1107 | 0 | 35 | 1142 | 141 | [源码](../../../engine/bootstrap/bootstrapper.h) |

### 常见但当前未检出

| 标准 | ID | 分类 | 特性 |
| --- | --- | --- | --- |
| C++14 | `cpp14-binary-literals` | 语言 | 二进制字面量 |
| C++14 | `cpp14-digit-separators` | 语言 | 数字分隔符 |
| C++17 | `cpp17-fallthrough` | 语言 | `[[fallthrough]]` |
| C++20 | `cpp20-bit-cast` | 标准库 | `std::bit_cast` |
| C++20 | `cpp20-consteval` | 语言 | `consteval` |
| C++20 | `cpp20-constinit` | 语言 | `constinit` |
| C++20 | `cpp20-coroutines` | 语言 | 协程 |
| C++23 | `cpp23-byteswap` | 标准库 | `std::byteswap` |
| C++23 | `cpp23-if-consteval` | 语言 | `if consteval` |
| C++23 | `cpp23-mdspan` | 标准库 | `std::mdspan` |
| C++23 | `cpp23-print` | 标准库 | `std::print` / `std::println` |
| C++23 | `cpp23-to-underlying` | 标准库 | `std::to_underlying` |

<!-- END GENERATED: cpp-language-feature-stats -->

## 使用扫描器

在仓库根目录使用 PowerShell 7 或更高版本运行：

```powershell
# 输出完整 Markdown 统计区块，不修改文件
.\scripts\audit_cpp_language_features.ps1

# 重新生成本页的统计区块
.\scripts\audit_cpp_language_features.ps1 -Update

# 检查本页是否与当前源码一致
.\scripts\audit_cpp_language_features.ps1 -Check

# 列出某项特性的全部命中位置
.\scripts\audit_cpp_language_features.ps1 -Feature cpp20-span

# 验证过滤、检测与符号分类逻辑
.\scripts\audit_cpp_language_features.ps1 -SelfTest
```

`-Check` 还会验证每个已用特性都有仍然有效的代表性源码，并检查所有出现的`std::` 标识符是否已归入现代特性注册表或明确的旧标准/通用库基线。出现新符号时，维护者必须先判断版本归属，统计才会通过。

## 自动统计的边界

扫描器刻意不为下列语义型能力制造伪精确数字：

- 保证复制消除、隐式移动和返回值优化等由语义与编译器行为共同决定的特性。
- 聚合初始化、统一初始化和类模板实参推导中无法仅凭局部文本可靠归因的形式；
  当前只统计能够明确识别的 `std::array` CTAD。
- 编译器扩展、预处理后才出现的代码，以及第三方头文件内部使用的特性。
- 仅凭重载解析、模板实例化结果或类型系统推导才能确认的间接用法。

因此，“全面”表示完整覆盖扫描器注册并能从当前第一方源码稳定识别的 C++11–23特性族，同时列出常见但当前未检出的候选；它不是 ISO C++ 标准条款逐项合规矩阵。

## 维护约定

新增检测项时，应在脚本注册表中给出稳定 ID、标准版本、分类、检测模式和代表性源码，并扩充 `-SelfTest`。修改源码后若 `-Check` 报告统计过期，运行 `-Update`，审阅数字变化及代表性链接，再提交文档。编码取舍仍以 [C++ 代码规范](coding-standard.md)为准。
