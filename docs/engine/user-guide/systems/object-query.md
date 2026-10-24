# 游戏对象查询

本文介绍按类型、绘制层级和位置查找活动场景中的对象。这里的层级是 `DepthLayer`，不是父子对象树。对象查询用于选择业务目标；需要几何碰撞或遮挡判定时，应使用物理查询。

`ELYSIA_OBJECT_QUERY` 查询当前绑定的活动场景中的 `GameObject`，不是全局存档数据库，也不查询 UI 树或其他缓存场景。无需给对象添加物理碰撞体即可进行类型、层级和位置查询。

## 查找附近的活跃对象

下面的辅助函数放在游戏代码中，在正常输入或更新阶段使用。`origin` 与半径采用世界坐标单位。

```cpp
#include "engine/object_query/game_object_query_service.h"

std::vector<elysia::core::GameObject*> nearby_items(
    elysia::core::Vector2 origin, float radius)
{
    elysia::object_query::GameObjectQueryService* query = ELYSIA_OBJECT_QUERY;
    // 全局入口只借用当前场景；场景未绑定时不发起查询。
    if (!query->is_available())
        return {};
    // 返回的对象指针仍属于场景，调用方不能据此延长对象寿命。
    return query->find_objects_in_radius<elysia::core::GameObject>(
        origin, radius, elysia::core::DepthLayerMask{elysia::core::DepthLayer::Item},
        [](const elysia::core::GameObject& object)
        {
            return object.is_active();
        });
}
```

已标记销毁的对象被排除；可见性和活跃状态按需要在谓词中筛选，不能假定“查到”就等于“屏幕可见、可交互”。模板类型可以换成具体游戏对象派生类。

## 选择查询

| 需求 | 接口 |
| --- | --- |
| 一个符合条件的对象 | `find_object<T>` |
| 全部符合条件的对象 | `find_objects<T>` |
| 距离某世界位置最近或最远 | `find_nearest_object<T>`、`find_farthest_object<T>` |
| 中心位于指定半径内 | `find_objects_in_radius<T>` |

接口提供层级掩码和谓词重载；组合层级可使用 `DepthLayer::Item | DepthLayer::Character`。单对象查询无结果返回空指针，多对象查询返回空容器；活动运行时不可用也不能得到有效场景结果。负半径返回空结果。

最近、最远与半径查询按 `object.center()` 的距离判断，半径边界包含在内；并非按物体轮廓与圆相交，也不处理物理遮挡。需要射线、碰撞形状重叠、扫掠或接触过滤时使用[物理查询](physics.md)。

## 借用结果与安全边界

结果中的指针由场景拥有，保存 vector 不会延长对象生命。查询结束后发生场景切换、重建或对象清理，都可能使其失效。单个结果是遍历中符合条件的对象，不应把它解释为稳定的角色 ID 或业务优先级。

谓词接收只读对象引用。求值期间不要添加、移除或销毁场景对象，也不要通过其他捕获引用绕过这一限制。需要批量修改时先完成查询，再在对象仍有效的同一业务阶段处理结果；不要把查询中的场景容器当成可安全修改的列表。

对象查询 visitor / 谓词内的同步 `add_object` 或 `clear_scene_objects` 会被场景拒绝。物理查询的 `noexcept` 回调同样必须只读，不能把异常当成退出查询的方式。先返回查询结果，再在允许修改场景的阶段执行变更。

## 参考

- [公开接口与重载](../../../../engine/object_query/game_object_query_service.h)
- [对象所有权](../core_concepts.md)、[返回使用指南](../README.md)
