#include "ui_element.h"
#include "ui_child_host.h"

namespace elysia::ui
{
UiElement::~UiElement()
{
    if (_cancellation_lifetime)
        _cancellation_lifetime->element = nullptr;
}

elysia::core::Vector2 UiElement::accumulated_presentation_translation() const noexcept
{
    elysia::core::Vector2 translation = _presentation_translation;
    for (const UiChildHost* ancestor = _layout_parent; ancestor; ancestor = ancestor->layout_parent())
        translation += ancestor->presentation_translation();
    return translation;
}

elysia::core::Rect UiElement::presentation_screen_rect() const noexcept
{
    return _screen_rect.translated(accumulated_presentation_translation());
}

void UiElement::set_order(int order) noexcept
{
    if (_order == order)
        return;

    _order = order;
    if (_layout_parent)
        _layout_parent->on_child_order_changed(*this);
}

void UiElement::bind_translation_animation(std::string name,UiTranslationAnimation animation)
{
    _translation_animation_player.bind(std::move(name),animation);
}

bool UiElement::remove_translation_animation(std::string_view name)
{
    return _translation_animation_player.remove(name);
}

void UiElement::clear_translation_animations() noexcept
{
    _translation_animation_player.clear();
    _presentation_translation = {};
}

bool UiElement::play_translation_animation(std::string_view name) noexcept
{
    if (!_translation_animation_player.play(name))
        return false;
    _presentation_translation = _translation_animation_player.translation();
    return true;
}

void UiElement::stop_translation_animation() noexcept
{
    _translation_animation_player.stop();
}

bool UiElement::is_translation_animation_playing() const noexcept
{
    return _translation_animation_player.is_playing();
}

std::optional<std::string> UiElement::active_translation_animation() const
{
    return _translation_animation_player.active_name();
}

void UiElement::update_presentation_animations(double delta)
{
    if (_translation_animation_player.update(delta))
        _presentation_translation = _translation_animation_player.translation();
}

void UiElement::notify_base_style_invalidated() noexcept
{
    if (_layout_parent)
        _layout_parent->on_child_base_style_invalidated(*this);
}
}
