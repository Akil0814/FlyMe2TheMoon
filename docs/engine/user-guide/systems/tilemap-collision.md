# 地图碰撞接入

地图通过 `ITileCollisionWorld` 提供碰撞描述；绘制、关卡数据和编辑方式由游戏决定。先按[物理指南](physics.md)启用场景物理，再把长期存活的地图实例绑定到该世界。

## 绑定与释放

地图实现提供行列数、世界原点、单元格尺寸、越界策略和单元格内容。支持矩形单元格、负世界原点及非正方形尺寸；目前没有斜坡、半格、分块流式加载或合并轮廓。

以下完整辅助函数在物理世界未推进的场景接入阶段调用。它只展示绑定，调用方必须保证 `tiles` 在绑定期间存活，并在销毁地图前调用 `clear_tile_world(tiles)`、检查返回结果；若在模拟回调中请求解绑，要等延迟操作实际提交后才能释放地图。

```cpp
#include "engine/physics/physics_world.h"
#include "engine/tools/logger.h"

bool attach_collision_map(elysia::physics::PhysicsWorld& world,
                          const elysia::physics::ITileCollisionWorld& tiles)
{
    // 世界借用地图；返回 true 不转移所有权。
    if (!world.set_tile_world(tiles))
    {
        ELYSIA_LOG_ERROR("physics", "Invalid collision map dimensions or origin.");
        return false;
    }
    return true;
}

bool refresh_collision_cell(elysia::physics::PhysicsWorld& world,
                            elysia::physics::TileCoordinate coordinate)
{
    // 先修改游戏自己的地图数据，再通知物理世界。两端均包含在脏区内。
    if (!world.update_tiles(coordinate, coordinate))
    {
        ELYSIA_LOG_ERROR("physics", "No bound map or invalid dirty coordinates.");
        return false;
    }
    return true;
}
```

绑定会校验有限的原点、有限正数的尺寸、非负行列数及最多一百万个单元格。越界策略 `Block` 建立四面边界墙，`Empty` 不建立。不要把绑定成功理解为每个单元格的自定义碰撞参数都有效；无效形状描述不会生成碰撞体。

## 修改地图

修改数据后调用 `update_tiles(begin, end)`；坐标必须在地图内且按从小到大排列，范围包含两端。它仅刷新指定区域；尺寸、行列数、原点或越界策略改变时重新绑定地图。模拟期间的修改可能延迟提交，应在安全阶段完成地图修改与刷新，不能修改后不通知世界。

## 主动穿过单向平台

从当前接触结果取得角色的 `ColliderId` 和支撑的 `CollisionTarget`，调用 `request_pass_through(actor, support)` 并检查 bool。失效角色、缺少目标、没有单向规则或禁止穿过的支撑会返回 false。

成功请求会唤醒角色，并暂时抑制指定碰撞对，直到形状充分分离。跨多个单元格站立时，需要为每个支撑碰撞对提交请求；它不会自动关闭整个平台，也不会自动给角色向下速度。游戏决定下落输入与运动规则。

完整接口见[物理参考](../../reference/physics/README.md)，邻接面和单向判定机制见[地图碰撞架构](../../architecture/subsystems/physics/06-tile-map-collision.md)。
