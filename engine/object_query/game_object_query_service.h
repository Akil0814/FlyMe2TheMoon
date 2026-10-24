#pragma once

#include "game_object_query_types.h"
#include "../core/game_object.h"
#include "../core/geometry/vector2.h"
#include "../tools/singleton.h"

#include <concepts>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#define ELYSIA_OBJECT_QUERY (::elysia::object_query::GameObjectQueryService::instance())

namespace elysia::object_query
{
namespace detail
{
struct AcceptAllGameObjects
{
    template <typename T>
    [[nodiscard]] constexpr bool operator()(const T&) const noexcept
    {
        return true;
    }
};
}

class GameObjectQueryService final
    : public elysia::tools::Singleton<GameObjectQueryService>
{
    friend elysia::tools::Singleton<GameObjectQueryService>;

public:
    // Returned pointers are borrowed from the active scene. Predicates must not
    // add, remove, or destroy scene objects while a query is being evaluated.
    [[nodiscard]] bool is_available() const noexcept;

    template <typename T = elysia::core::GameObject>
    [[nodiscard]] T* find_object() const
    {
        return find_object<T>(
            elysia::core::DepthLayerMask::all(),
            detail::AcceptAllGameObjects{});
    }

    template <typename T = elysia::core::GameObject>
    [[nodiscard]] T* find_object(
        elysia::core::DepthLayerMask layers) const
    {
        return find_object<T>(layers,detail::AcceptAllGameObjects{});
    }

    template <typename T = elysia::core::GameObject, typename Predicate>
        requires std::predicate<Predicate&,const T&>
    [[nodiscard]] T* find_object(Predicate&& predicate) const
    {
        return find_object<T>(
            elysia::core::DepthLayerMask::all(),
            std::forward<Predicate>(predicate));
    }

    template <typename T = elysia::core::GameObject, typename Predicate>
        requires std::predicate<Predicate&,const T&>
    [[nodiscard]] T* find_object(
        elysia::core::DepthLayerMask layers,
        Predicate&& predicate) const
    {
        assert_game_object_type<T>();

        T* result = nullptr;
        visit_game_objects(layers,[&](elysia::core::GameObject& object)
        {
            if (object.is_destroyed())
                return true;

            T* typed_object = dynamic_cast<T*>(&object);
            if (!typed_object)
                return true;

            if (!std::invoke(predicate, std::as_const(*typed_object)))
                return true;

            result = typed_object;
            return false;
        });
        return result;
    }

    template <typename T = elysia::core::GameObject>
    [[nodiscard]] std::vector<T*> find_objects() const
    {
        return find_objects<T>(
            elysia::core::DepthLayerMask::all(),
            detail::AcceptAllGameObjects{});
    }

    template <typename T = elysia::core::GameObject>
    [[nodiscard]] std::vector<T*> find_objects(
        elysia::core::DepthLayerMask layers) const
    {
        return find_objects<T>(layers,detail::AcceptAllGameObjects{});
    }

    template <typename T = elysia::core::GameObject, typename Predicate>
        requires std::predicate<Predicate&,const T&>
    [[nodiscard]] std::vector<T*> find_objects(Predicate&& predicate) const
    {
        return find_objects<T>(
            elysia::core::DepthLayerMask::all(),
            std::forward<Predicate>(predicate));
    }

    template <typename T = elysia::core::GameObject, typename Predicate>
        requires std::predicate<Predicate&,const T&>
    [[nodiscard]] std::vector<T*> find_objects(
        elysia::core::DepthLayerMask layers,
        Predicate&& predicate) const
    {
        assert_game_object_type<T>();

        std::vector<T*> results;
        visit_game_objects(layers,[&](elysia::core::GameObject& object)
        {
            if (object.is_destroyed())
                return true;

            T* typed_object = dynamic_cast<T*>(&object);
            if (typed_object
                && std::invoke(predicate, std::as_const(*typed_object)))
            {
                results.push_back(typed_object);
            }

            return true;
        });
        return results;
    }

    template <typename T = elysia::core::GameObject,
        typename Projection,
        typename Compare>
    [[nodiscard]] T* find_best_object(
        Projection&& projection,
        Compare&& compare) const
    {
        return find_best_object<T>(
            elysia::core::DepthLayerMask::all(),
            detail::AcceptAllGameObjects{},
            std::forward<Projection>(projection),
            std::forward<Compare>(compare)
        );
    }

    template <typename T = elysia::core::GameObject,
        typename Projection,
        typename Compare>
    [[nodiscard]] T* find_best_object(
        elysia::core::DepthLayerMask layers,
        Projection&& projection,
        Compare&& compare) const
    {
        return find_best_object<T>(
            layers,
            detail::AcceptAllGameObjects{},
            std::forward<Projection>(projection),
            std::forward<Compare>(compare)
        );
    }

    template <typename T = elysia::core::GameObject,
        typename Predicate,
        typename Projection,
        typename Compare>
        requires std::predicate<Predicate&,const T&>
    [[nodiscard]] T* find_best_object(
        Predicate&& predicate,
        Projection&& projection,
        Compare&& compare) const
    {
        return find_best_object<T>(
            elysia::core::DepthLayerMask::all(),
            std::forward<Predicate>(predicate),
            std::forward<Projection>(projection),
            std::forward<Compare>(compare));
    }

    template <typename T = elysia::core::GameObject,
        typename Predicate,
        typename Projection,
        typename Compare>
        requires std::predicate<Predicate&,const T&>
    [[nodiscard]] T* find_best_object(
        elysia::core::DepthLayerMask layers,
        Predicate&& predicate,
        Projection&& projection,
        Compare&& compare) const
    {
        assert_game_object_type<T>();

        using Score = std::remove_cvref_t<
            std::invoke_result_t<Projection&, const T&>>;

        T* result = nullptr;
        std::optional<Score> best_score;
        visit_game_objects(layers,[&](elysia::core::GameObject& object)
        {
            if (object.is_destroyed())
                return true;

            T* typed_object = dynamic_cast<T*>(&object);
            if (!typed_object)
                return true;

            const T& candidate = std::as_const(*typed_object);
            if (!std::invoke(predicate, candidate))
                return true;

            Score score = std::invoke(projection, candidate);
            if (!best_score || std::invoke(compare, score, *best_score))
            {
                result = typed_object;
                best_score = std::move(score);
            }

            return true;
        });
        return result;
    }

    template <typename T = elysia::core::GameObject>
    [[nodiscard]] T* find_nearest_object(
        const elysia::core::Vector2& origin) const
    {
        return find_nearest_object<T>(
            origin,
            elysia::core::DepthLayerMask::all(),
            detail::AcceptAllGameObjects{});
    }

    template <typename T = elysia::core::GameObject>
    [[nodiscard]] T* find_nearest_object(
        const elysia::core::Vector2& origin,
        elysia::core::DepthLayerMask layers) const
    {
        return find_nearest_object<T>(
            origin,layers,detail::AcceptAllGameObjects{});
    }

    template <typename T = elysia::core::GameObject, typename Predicate>
        requires std::predicate<Predicate&,const T&>
    [[nodiscard]] T* find_nearest_object(
        const elysia::core::Vector2& origin,
        Predicate&& predicate) const
    {
        return find_nearest_object<T>(
            origin,
            elysia::core::DepthLayerMask::all(),
            std::forward<Predicate>(predicate));
    }

    template <typename T = elysia::core::GameObject, typename Predicate>
        requires std::predicate<Predicate&,const T&>
    [[nodiscard]] T* find_nearest_object(
        const elysia::core::Vector2& origin,
        elysia::core::DepthLayerMask layers,
        Predicate&& predicate) const
    {
        return find_best_object<T>(
            layers,
            std::forward<Predicate>(predicate),
            [&origin](const T& object)
            {
                return origin.distance_squared_to(object.center());
            },
            std::less<float>{}
        );
    }

    template <typename T = elysia::core::GameObject>
    [[nodiscard]] T* find_farthest_object(
        const elysia::core::Vector2& origin) const
    {
        return find_farthest_object<T>(
            origin,
            elysia::core::DepthLayerMask::all(),
            detail::AcceptAllGameObjects{});
    }

    template <typename T = elysia::core::GameObject>
    [[nodiscard]] T* find_farthest_object(
        const elysia::core::Vector2& origin,
        elysia::core::DepthLayerMask layers) const
    {
        return find_farthest_object<T>(
            origin,layers,detail::AcceptAllGameObjects{});
    }

    template <typename T = elysia::core::GameObject, typename Predicate>
        requires std::predicate<Predicate&,const T&>
    [[nodiscard]] T* find_farthest_object(
        const elysia::core::Vector2& origin,
        Predicate&& predicate) const
    {
        return find_farthest_object<T>(
            origin,
            elysia::core::DepthLayerMask::all(),
            std::forward<Predicate>(predicate));
    }

    template <typename T = elysia::core::GameObject, typename Predicate>
        requires std::predicate<Predicate&,const T&>
    [[nodiscard]] T* find_farthest_object(
        const elysia::core::Vector2& origin,
        elysia::core::DepthLayerMask layers,
        Predicate&& predicate) const
    {
        return find_best_object<T>(
            layers,
            std::forward<Predicate>(predicate),
            [&origin](const T& object)
            {
                return origin.distance_squared_to(object.center());
            },
            std::greater<float>{}
        );
    }

    template <typename T = elysia::core::GameObject>
    [[nodiscard]] std::vector<T*> find_objects_in_radius(
        const elysia::core::Vector2& origin,
        float radius) const
    {
        return find_objects_in_radius<T>(
            origin,
            radius,
            elysia::core::DepthLayerMask::all(),
            detail::AcceptAllGameObjects{}
        );
    }

    template <typename T = elysia::core::GameObject>
    [[nodiscard]] std::vector<T*> find_objects_in_radius(
        const elysia::core::Vector2& origin,
        float radius,
        elysia::core::DepthLayerMask layers) const
    {
        return find_objects_in_radius<T>(
            origin,radius,layers,detail::AcceptAllGameObjects{});
    }

    template <typename T = elysia::core::GameObject, typename Predicate>
        requires std::predicate<Predicate&,const T&>
    [[nodiscard]] std::vector<T*> find_objects_in_radius(
        const elysia::core::Vector2& origin,
        float radius,
        Predicate&& predicate) const
    {
        return find_objects_in_radius<T>(
            origin,
            radius,
            elysia::core::DepthLayerMask::all(),
            std::forward<Predicate>(predicate));
    }

    template <typename T = elysia::core::GameObject, typename Predicate>
        requires std::predicate<Predicate&,const T&>
    [[nodiscard]] std::vector<T*> find_objects_in_radius(
        const elysia::core::Vector2& origin,
        float radius,
        elysia::core::DepthLayerMask layers,
        Predicate&& predicate) const
    {
        assert_game_object_type<T>();

        if (radius < 0.0f)
            return {};

        const float radius_squared = radius * radius;
        return find_objects<T>(
            layers,
            [&](const T& object)
            {
                return origin.distance_squared_to(object.center()) <= radius_squared
                    && std::invoke(predicate, object);
            }
        );
    }

private:
    GameObjectQueryService() = default;

    template <typename T>
    static constexpr void assert_game_object_type()
    {
        static_assert(
            std::is_base_of_v<elysia::core::GameObject, T>,
            "T must derive from elysia::core::GameObject."
        );
    }

    void visit_game_objects(
        elysia::core::DepthLayerMask layers,
        const GameObjectVisitor& visitor) const;
};
}
