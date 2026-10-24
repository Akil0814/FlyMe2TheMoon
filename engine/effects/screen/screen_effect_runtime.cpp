#include "screen_effect_runtime.h"
#include "../../core/render/sdl_render_command_executor.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace elysia::effects
{
namespace
{
bool valid_layout(ScreenEffectFit fit, const ScreenEffectPlacement& p)
{
    return (fit == ScreenEffectFit::Stretch || fit == ScreenEffectFit::Cover
        || fit == ScreenEffectFit::Contain || fit == ScreenEffectFit::Natural)
        && p.anchor >= EffectAnchor::TopLeft && p.anchor <= EffectAnchor::BottomRight
        && std::isfinite(p.offset.x) && std::isfinite(p.offset.y)
        && std::isfinite(p.scale.x) && std::isfinite(p.scale.y)
        && p.scale.x > 0 && p.scale.y > 0;
}

elysia::core::Rect layout_rect(const elysia::core::Rect& viewport, ScreenEffectFit fit,
    const ScreenEffectPlacement& p, float width, float height, float natural_width, float natural_height)
{
    float w = viewport.width(), h = viewport.height();
    if (fit == ScreenEffectFit::Natural) { w = natural_width; h = natural_height; }
    else if (fit != ScreenEffectFit::Stretch)
    {
        const float sx = w / width, sy = h / height;
        const float factor = fit == ScreenEffectFit::Cover ? std::max(sx, sy) : std::min(sx, sy);
        w = width * factor; h = height * factor;
    }
    w *= p.scale.x; h *= p.scale.y;
    const int anchor = static_cast<int>(p.anchor);
    const float ax = (anchor % 3) * 0.5f, ay = (anchor / 3) * 0.5f;
    return {viewport.x() + (viewport.width() - w) * ax + p.offset.x,
        viewport.y() + (viewport.height() - h) * ay + p.offset.y, w, h};
}

// Screen overlays always alpha-blend, including opaque RGB/JPEG textures.
// Restore the borrowed texture's blend mode even if command execution fails.
class ScreenTextureBlendState
{
public:
    explicit ScreenTextureBlendState(SDL_Texture* texture) : _texture(texture)
    {
        elysia::core::detail::checked_render_operation("SDL_GetTextureBlendMode", [&] {
            return SDL_GetTextureBlendMode(texture, &_blend);
        });
    }
    void apply()
    {
        _changed = true;
        elysia::core::detail::checked_render_operation("SDL_SetTextureBlendMode", [&] {
            return SDL_SetTextureBlendMode(_texture, SDL_BLENDMODE_BLEND);
        });
    }
    template<typename Restore>
    void restore(Restore&& restore)
    {
        if (_changed) restore([&] {
            elysia::core::detail::checked_render_operation("restore.SDL_SetTextureBlendMode", [&] {
                return SDL_SetTextureBlendMode(_texture, _blend);
            });
        });
    }
private:
    SDL_Texture* _texture;
    SDL_BlendMode _blend{};
    bool _changed = false;
};
}
bool ScreenEffectRuntime::valid(const ScreenEffectPlayback& p) noexcept
{
    const auto time = [](double v) { return std::isfinite(v) && v >= 0; };
    return time(p.fade_in_seconds) && time(p.hold_seconds) && time(p.fade_out_seconds)
        && std::isfinite(p.target_opacity) && p.target_opacity >= 0 && p.target_opacity <= 1
        && (p.end == ScreenEffectEnd::Timed || p.end == ScreenEffectEnd::Manual)
        && (p.layer == ScreenEffectLayer::BeforeUi || p.layer == ScreenEffectLayer::AfterUi)
        && (p.clock == ScreenEffectClock::Unscaled || p.clock == ScreenEffectClock::Scene)
        && (p.end == ScreenEffectEnd::Manual || p.fade_in_seconds > 0 || p.hold_seconds > 0 || p.fade_out_seconds > 0);
}

ScreenEffectHandle ScreenEffectRuntime::insert(Effect effect)
{
    effect.order = ++_order;
    effect.opacity = effect.playback.fade_in_seconds == 0 ? effect.playback.target_opacity : 0;
    for (std::size_t i = 0; i < _slots.size(); ++i)
        if (!_slots[i].effect && _slots[i].generation != 0)
        {
            _slots[i].effect = std::move(effect);
            return {static_cast<std::uint32_t>(i), _slots[i].generation};
        }
    _slots.push_back({1, std::move(effect)});
    return {static_cast<std::uint32_t>(_slots.size() - 1), 1};
}

std::optional<ScreenEffectHandle> ScreenEffectRuntime::create(const ScreenColorEffectRequest& r, const void* scene,
    std::optional<std::size_t> frame)
{
    if (!scene || !valid(r.playback)) return {};
    Effect e;
    e.playback = r.playback; e.scene = scene; e.color = r.color;
    e.created_frame = frame;
    return insert(std::move(e));
}

std::optional<ScreenEffectHandle> ScreenEffectRuntime::create(const ScreenImageEffectRequest& r, SDL_Texture* texture, const void* scene,
    std::optional<std::size_t> frame)
{
    if (!scene || !texture || !valid(r.playback) || !valid_layout(r.fit, r.placement)) return {};
    Effect e;
    if (!SDL_GetTextureSize(texture, &e.width, &e.height) || e.width <= 0 || e.height <= 0) return {};
    e.playback = r.playback; e.scene = scene; e.texture = texture; e.fit = r.fit;
    e.placement = r.placement; e.natural_width = e.width; e.natural_height = e.height;
    e.created_frame = frame;
    return insert(std::move(e));
}

std::optional<ScreenEffectHandle> ScreenEffectRuntime::create(const ScreenAnimationEffectRequest& r,
    std::unique_ptr<elysia::animation::Animation> animation,
    const elysia::animation::AnimationDefinition& definition,
    elysia::core::Vector2 natural_size, double angle, const void* scene, std::optional<std::size_t> frame)
{
    auto playback = r.playback;
    if (playback.end == ScreenEffectEnd::AnimationFinished) playback.end = ScreenEffectEnd::Manual;
    if (!scene || !animation || !definition.atlas || definition.atlas->empty()
        || !valid(playback) || !valid_layout(r.fit, r.placement) || !std::isfinite(angle)
        || !std::isfinite(definition.fps) || definition.fps <= 0
        || (r.loop && r.playback.end == ScreenEffectEnd::AnimationFinished)
        || (r.flip != elysia::core::SpriteFlip::None && r.flip != elysia::core::SpriteFlip::Horizontal
            && r.flip != elysia::core::SpriteFlip::Vertical && r.flip != elysia::core::SpriteFlip::Both)) return {};
    for (std::size_t i = 0; i < definition.atlas->size(); ++i)
    {
        const auto* f = definition.atlas->frame_at(i);
        if (!f || !f->_texture || f->_width <= 0 || f->_height <= 0) return {};
    }
    Effect e;
    const auto* first = definition.atlas->frame_at(0);
    e.width = static_cast<float>(first->_width); e.height = static_cast<float>(first->_height);
    if (natural_size.is_zero()) natural_size = {e.width, e.height};
    if (!std::isfinite(natural_size.x) || !std::isfinite(natural_size.y)
        || natural_size.x <= 0 || natural_size.y <= 0) return {};
    e.natural_width = natural_size.x; e.natural_height = natural_size.y;
    e.animation_duration = definition.atlas->size() / definition.fps;
    if (!std::isfinite(e.animation_duration) || e.animation_duration <= 0) return {};
    animation->set_loop(r.loop);
    e.animation = std::move(animation);
    e.playback = r.playback; e.scene = scene; e.fit = r.fit; e.placement = r.placement;
    e.angle = angle; e.flip = r.flip; e.created_frame = frame;
    return insert(std::move(e));
}

ScreenEffectRuntime::Effect* ScreenEffectRuntime::find(ScreenEffectHandle h) noexcept
{
    if (!active(h)) return nullptr;
    return &*_slots[h.slot].effect;
}
bool ScreenEffectRuntime::active(ScreenEffectHandle h) const noexcept
{
    return h.generation != 0 && h.slot < _slots.size() && _slots[h.slot].generation == h.generation && _slots[h.slot].effect.has_value();
}
void ScreenEffectRuntime::retire(Slot& slot) noexcept
{
    slot.effect.reset();
    // A wrapped generation permanently retires the slot.
    ++slot.generation;
}
bool ScreenEffectRuntime::cancel(ScreenEffectHandle h) noexcept
{
    if (!active(h)) return false;
    retire(_slots[h.slot]); return true;
}
bool ScreenEffectRuntime::stop(ScreenEffectHandle h) noexcept
{
    auto* e = find(h);
    if (!e || e->stopping) return false;
    if (e->playback.end == ScreenEffectEnd::Timed
        && e->elapsed >= e->playback.fade_in_seconds
        && e->elapsed - e->playback.fade_in_seconds >= e->playback.hold_seconds) return false;
    if (e->playback.end == ScreenEffectEnd::AnimationFinished
        && e->elapsed >= std::max(e->playback.fade_in_seconds, e->animation_duration)) return false;
    if (e->playback.fade_out_seconds == 0) return cancel(h);
    e->stopping = true; e->stop_opacity = e->opacity; e->elapsed = 0;
    return true;
}
void ScreenEffectRuntime::update(double raw, double scaled, bool paused, std::optional<std::size_t> frame) noexcept
{
    for (auto& slot : _slots)
    {
        if (!slot.effect) continue;
        auto& e = *slot.effect;
        if (frame && e.created_frame == frame) continue;
        const auto& p = e.playback;
        double delta = p.clock == ScreenEffectClock::Unscaled ? raw : (paused ? 0 : scaled);
        if (!std::isfinite(delta) || delta < 0) delta = 0;
        e.elapsed += delta;
        if (e.animation) e.animation->update(delta);
        if (e.stopping)
        {
            if (e.elapsed >= p.fade_out_seconds) { retire(slot); continue; }
            e.opacity = e.stop_opacity * (1 - e.elapsed / p.fade_out_seconds);
        }
        else if (e.elapsed < p.fade_in_seconds)
            e.opacity = p.target_opacity * e.elapsed / p.fade_in_seconds;
        else
        {
            const double fade_out_start = p.end == ScreenEffectEnd::AnimationFinished
                ? std::max(p.fade_in_seconds, e.animation_duration) : p.fade_in_seconds + p.hold_seconds;
            if (p.end == ScreenEffectEnd::Manual || e.elapsed < fade_out_start)
                e.opacity = p.target_opacity;
            else
            {
                const double after_hold = e.elapsed - fade_out_start;
                if (after_hold >= p.fade_out_seconds) { retire(slot); continue; }
                e.opacity = p.target_opacity * (1 - after_hold / p.fade_out_seconds);
            }
        }
    }
}
void ScreenEffectRuntime::unbind(const void* scene) noexcept
{
    for (auto& slot : _slots)
        if (slot.effect && slot.effect->scene == scene)
        {
            retire(slot);
        }
}
void ScreenEffectRuntime::clear() noexcept
{
    for (auto& slot : _slots) if (slot.effect) retire(slot);
}
void ScreenEffectRuntime::append_commands(ScreenEffectLayer layer, const elysia::core::Rect& viewport,
    std::vector<elysia::core::UiRenderCommand>& out) const
{
    if (viewport.width() <= 0 || viewport.height() <= 0) return;
    std::vector<const Effect*> ordered;
    for (const auto& slot : _slots)
        if (slot.effect && slot.effect->playback.layer == layer) ordered.push_back(&*slot.effect);
    std::ranges::sort(ordered, {}, &Effect::order);
    for (const auto* e : ordered)
    {
        const auto alpha = static_cast<std::uint8_t>(std::lround(std::clamp(e->opacity, 0.0, 1.0) * 255));
        if (!e->texture && !e->animation)
        {
            auto color = e->color;
            color.a = static_cast<std::uint8_t>(std::lround(color.a * std::clamp(e->opacity, 0.0, 1.0)));
            out.push_back(elysia::core::make_ui_fill_rect_command(viewport, color));
        }
        else
        {
            const auto rect = layout_rect(viewport, e->fit, e->placement,
                e->width, e->height, e->natural_width, e->natural_height);
            auto command = elysia::core::make_ui_texture_command(e->texture, rect, viewport, alpha);
            if (e->animation)
            {
                elysia::core::RenderCommand frame;
                if (!e->animation->build_render_command(rect, e->angle, e->flip, frame)) continue;
                command.texture = frame.texture; command.use_src_rect = frame.use_src_rect;
                command.src_rect = frame.src_rect; command.rotation_degrees = frame.rotation_degrees;
                command.rotation_origin = frame.rotation_origin; command.flip = frame.flip;
            }
            out.push_back(command);
        }
    }
}
elysia::core::RenderResult ScreenEffectRuntime::render(SDL_Renderer* renderer, ScreenEffectLayer layer,
    const elysia::core::Rect& viewport) const
{
    return elysia::core::detail::render_boundary(renderer, [&] {
        std::vector<elysia::core::UiRenderCommand> commands;
        append_commands(layer, viewport, commands);
        for (const auto& command : commands)
        {
            if (command.texture)
            {
                ScreenTextureBlendState state(command.texture);
                elysia::core::detail::with_render_state(state, [&] {
                    state.apply();
                    elysia::core::require_render_success(elysia::core::execute_render_command(renderer, command));
                });
            }
            else elysia::core::require_render_success(elysia::core::execute_render_command(renderer, command));
        }
    });
}
}

