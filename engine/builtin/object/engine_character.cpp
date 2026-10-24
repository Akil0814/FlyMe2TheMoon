#include "engine_character.h"

#include "../resources/builtin_resources.h"
#include "../../core/render/colors.h"
#include "../../tools/debug_draw.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace elysia::builtin
{
namespace
{
constexpr elysia::core::Rect kDefaultWorldRect{0.0f, 0.0f, 96.0f, 96.0f};
constexpr elysia::core::Rect kLocalColliderRect{24.0f, 16.0f, 48.0f, 72.0f};


}

EngineCharacter::EngineCharacter(elysia::input::InputActionId movement_action) : GameObject(elysia::core::DepthLayer::Character), _movement_action(std::move(movement_action))
{
    set_world_rect(kDefaultWorldRect);

    _collider.shape = elysia::physics::AabbShape{kLocalColliderRect};
    _collider.filter.category = 0;
    _collider.filter.mask = 0;
    _collider.response = elysia::physics::CollisionResponse::Ignore;

    if (!set_animations(
            BuiltinAnimationId::EngineCharacterIdle,
            BuiltinAnimationId::EngineCharacterMove))
    {
        throw std::logic_error(
            "EngineCharacter requires the built-in idle and move animations.");
    }
}

void EngineCharacter::update(double delta_seconds)
{
    const double delta = scaled_delta(delta_seconds);
    if (auto *animation = _is_moving ? _move.get() : _idle.get())
        animation->update(std::isfinite(delta) && delta > 0 ? delta : 0);
}
void EngineCharacter::on_control_command(const elysia::gameplay::ControlCommand &command, double delta)
{
    auto direction = command.state.axis2d(_movement_action);
    if (direction.length_squared() > 1)
        direction.normalize_in_place();
    set_moving(!direction.is_zero());
    if (direction.x < 0)
        _flip = elysia::core::SpriteFlip::None;
    else if (direction.x > 0)
        _flip = elysia::core::SpriteFlip::Horizontal;
    const auto scaled = scaled_delta(delta);
    if (std::isfinite(scaled) && scaled > 0)
        set_position(position() + direction * (kMovementSpeed * static_cast<float>(scaled)));
    clamp_to_movement_bounds();
}

void EngineCharacter::submit_render_commands(
    std::vector<elysia::core::RenderCommand>& out_commands) const
{
    const elysia::animation::Animation* active_animation =
        _is_moving ? _move.get() : _idle.get();
    if (active_animation)
    {
        (void)active_animation->append_render_commands(
            render_rect(),
            0.0,
            _flip,
            std::nullopt,
            out_commands);
    }
}

std::span<const elysia::physics::Collider>
EngineCharacter::colliders() const noexcept
{
    return std::span<const elysia::physics::Collider>(&_collider, 1);
}


bool EngineCharacter::set_animations(
    BuiltinAnimationId idle_id,
    BuiltinAnimationId move_id)
{
    std::unique_ptr<elysia::animation::Animation> idle =
        BuiltinResources::instance()->create_animation(idle_id);
    std::unique_ptr<elysia::animation::Animation> move =
        BuiltinResources::instance()->create_animation(move_id);
    if (!idle || !move)
        return false;

    idle->resume();
    move->pause();
    _idle = std::move(idle);
    _move = std::move(move);
    _is_moving = false;
    return true;
}

void EngineCharacter::set_movement_bounds(
    std::optional<elysia::core::Rect> bounds) noexcept
{
    if (bounds && bounds->is_empty())
        bounds.reset();
    _movement_bounds = std::move(bounds);
    clamp_to_movement_bounds();
}

const std::optional<elysia::core::Rect>&
EngineCharacter::movement_bounds() const noexcept
{
    return _movement_bounds;
}

void EngineCharacter::clear_movement_input() noexcept
{
    set_moving(false);
}

void EngineCharacter::submit_debug_draw() const
{
    if (!_collider.enabled)
        return;

    const auto* aabb = std::get_if<elysia::physics::AabbShape>(
        &_collider.shape);
    if (!aabb)
        return;

    elysia::tools::DebugDraw::instance()->draw_rect(
        elysia::tools::DebugDrawCategory::PhysicsCollider,
        aabb->local_rect.translated(position()),
        elysia::core::colors::green_500,
        2.0f);
}

void EngineCharacter::set_moving(bool moving) noexcept
{
    if (_is_moving == moving)
        return;

    elysia::animation::Animation* previous =
        _is_moving ? _move.get() : _idle.get();
    elysia::animation::Animation* next = moving ? _move.get() : _idle.get();
    if (previous)
        previous->pause();
    if (next)
        next->reset();
    _is_moving = moving;
}

void EngineCharacter::clamp_to_movement_bounds() noexcept
{
    if (!_movement_bounds)
        return;

    const elysia::core::Rect& bounds = *_movement_bounds;
    const elysia::core::Rect& character_rect = world_rect();
    elysia::core::Vector2 clamped_position = character_rect.position();

    if (character_rect.width() <= bounds.width())
    {
        clamped_position.x = std::clamp(
            clamped_position.x,
            bounds.left(),
            bounds.right() - character_rect.width());
    }
    else
    {
        clamped_position.x = bounds.center().x - character_rect.width() * 0.5f;
    }

    if (character_rect.height() <= bounds.height())
    {
        clamped_position.y = std::clamp(
            clamped_position.y,
            bounds.top(),
            bounds.bottom() - character_rect.height());
    }
    else
    {
        clamped_position.y = bounds.center().y - character_rect.height() * 0.5f;
    }

    set_position(clamped_position);
}
}
