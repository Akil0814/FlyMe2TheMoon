#include "ui_animation.h"

#include "../../../builtin/resources/builtin_resources.h"
#include "../../../animation/animation_service.h"
#include "../../../core/render/render_command.h"

namespace elysia::ui
{
UiAnimation::UiAnimation(const elysia::core::Rect& rect, int order)
    : UiElement(rect, order)
{
}

UiAnimation::UiAnimation(
    std::string_view animation_key,
    const elysia::core::Vector2& position,
    const elysia::core::Vector2& size,
    int order
)
    : UiElement(position, size, order)
{
    set_animation_key(animation_key);
}

UiAnimation::UiAnimation(std::string_view animation_key, const elysia::core::Rect& rect, int order)
    : UiElement(rect, order)
{
    set_animation_key(animation_key);
}

UiAnimation::UiAnimation(
    std::string_view animation_key,
    const elysia::core::Vector2& center,
    const elysia::core::Vector2& size,
    UiFromCenterTag,
    int order
)
    : UiElement(center, size, from_center, order)
{
    set_animation_key(animation_key);
}

bool UiAnimation::set_animation_key(std::string_view animation_key)
{
    std::unique_ptr<elysia::animation::Animation> animation =
        ELYSIA_ANIMATIONS->create_animation(animation_key);
    if (!animation)
    {
        _animation_key.clear();
        _animation.reset();
        _default_loop.reset();
        return false;
    }

    if (_loop_override)
        animation->set_loop(*_loop_override);

    _animation_key = animation_key;
    _animation = std::move(animation);
    const auto* definition = ELYSIA_ANIMATIONS->find_definition(animation_key);
    _default_loop = definition ? std::optional<bool>(definition->loop) : std::nullopt;
    _animation->reset();
    return true;
}

bool UiAnimation::set_engine_animation(
    elysia::builtin::BuiltinAnimationId animation_id)
{
    std::unique_ptr<elysia::animation::Animation> animation =
        elysia::builtin::BuiltinResources::instance()->create_animation(animation_id);
    const auto* definition = elysia::builtin::BuiltinResources::instance()->find_animation(animation_id);
    if (!animation || !definition)
    {
        _animation_key.clear();
        _animation.reset();
        _default_loop.reset();
        return false;
    }

    if (_loop_override)
        animation->set_loop(*_loop_override);

    _animation_key = elysia::builtin::builtin_resource_name(animation_id);
    _animation = std::move(animation);
    _default_loop = definition->loop;
    _animation->reset();
    return true;
}

const std::string& UiAnimation::animation_key() const noexcept
{
    return _animation_key;
}

void UiAnimation::set_loop(bool loop)
{
    _loop_override = loop;
    if (_animation)
        _animation->set_loop(loop);
}

bool UiAnimation::is_looping() const noexcept
{
    if (_loop_override)
        return *_loop_override;

    return _default_loop.value_or(false);
}

void UiAnimation::set_color_overlay(
    std::optional<elysia::core::Color> color_overlay) noexcept
{
    _color_overlay = color_overlay;
}

const std::optional<elysia::core::Color>&
UiAnimation::color_overlay() const noexcept
{
    return _color_overlay;
}

void UiAnimation::play()
{
    if (_animation)
        _animation->reset();
}

void UiAnimation::pause()
{
    if (_animation)
        _animation->pause();
}

void UiAnimation::resume()
{
    if (_animation)
        _animation->resume();
}

void UiAnimation::reset() noexcept
{
    UiElement::reset();
    if (_animation)
        _animation->reset();
}

bool UiAnimation::is_finished() const noexcept
{
    return _animation && _animation->is_finished();
}

bool UiAnimation::is_paused() const noexcept
{
    return !_animation || _animation->is_paused();
}

void UiAnimation::update(double delta)
{
    if (_animation)
        _animation->update(delta);
}

void UiAnimation::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible() || !_animation)
        return;

    const elysia::resources::FrameInfo* frame = _animation->current_frame();
    if (!frame || !frame->_texture)
        return;

	elysia::core::UiRenderCommand command =
		elysia::core::make_ui_texture_command(frame->_texture, screen_rect());
	if (frame->_source_rect.has_value())
	{
		command.use_src_rect = true;
		command.src_rect = *frame->_source_rect;
	}
	apply_opacity(command);
    out_commands.push_back(command);

    if (!_color_overlay || _color_overlay->a == 0 || !frame->_coverage_mask)
        return;

    elysia::core::UiRenderCommand overlay_command =
        elysia::core::make_ui_texture_command(
            frame->_coverage_mask,
            screen_rect(),
            _color_overlay->a);
    if (frame->_source_rect.has_value())
    {
        overlay_command.use_src_rect = true;
        overlay_command.src_rect = *frame->_source_rect;
    }
    overlay_command.texture_color_modulation =
        elysia::core::TextureColorModulation{
            .r = _color_overlay->r,
            .g = _color_overlay->g,
            .b = _color_overlay->b
        };
    apply_opacity(overlay_command);
    out_commands.push_back(overlay_command);
}
}
