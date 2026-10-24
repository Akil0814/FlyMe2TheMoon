#pragma once

#include "screen_effect_types.h"
#include "../../core/render/render_command.h"
#include "../../core/render/render_failure.h"
#include "../../animation/animation.h"
#include "../../animation/animation_types.h"
#include <memory>
#include <optional>
#include <vector>

namespace elysia::effects
{
// Owns playback state only; textures and scene identities are borrowed.
class ScreenEffectRuntime
{
public:
    static bool valid(const ScreenEffectPlayback& playback) noexcept;
    std::optional<ScreenEffectHandle> create(const ScreenColorEffectRequest& request, const void* scene,
        std::optional<std::size_t> frame = {});
    std::optional<ScreenEffectHandle> create(const ScreenImageEffectRequest& request, SDL_Texture* texture, const void* scene,
        std::optional<std::size_t> frame = {});
    std::optional<ScreenEffectHandle> create(const ScreenAnimationEffectRequest& request,
        std::unique_ptr<elysia::animation::Animation> animation,
        const elysia::animation::AnimationDefinition& definition,
        elysia::core::Vector2 natural_size, double angle, const void* scene,
        std::optional<std::size_t> frame = {});
    bool stop(ScreenEffectHandle handle) noexcept;
    bool cancel(ScreenEffectHandle handle) noexcept;
    bool active(ScreenEffectHandle handle) const noexcept;
    void update(double raw_delta, double scene_delta, bool paused, std::optional<std::size_t> frame = {}) noexcept;
    void unbind(const void* scene) noexcept;
    void clear() noexcept;
    void append_commands(ScreenEffectLayer layer, const elysia::core::Rect& viewport,
        std::vector<elysia::core::UiRenderCommand>& out) const;
    [[nodiscard]] elysia::core::RenderResult render(SDL_Renderer* renderer, ScreenEffectLayer layer,
        const elysia::core::Rect& viewport) const;

private:
    struct Effect
    {
        ScreenEffectPlayback playback;
        const void* scene = nullptr;
        SDL_Texture* texture = nullptr;
        elysia::core::Color color;
        ScreenEffectFit fit = ScreenEffectFit::Stretch;
        ScreenEffectPlacement placement;
        std::unique_ptr<elysia::animation::Animation> animation;
        double animation_duration = 0;
        double angle = 0;
        elysia::core::SpriteFlip flip = elysia::core::SpriteFlip::None;
        float width = 0, height = 0;
        float natural_width = 0, natural_height = 0;
        double elapsed = 0, opacity = 0, stop_opacity = 0;
        bool stopping = false;
        std::optional<std::size_t> created_frame;
        std::uint64_t order = 0;
    };
    struct Slot
    {
        std::uint64_t generation = 1;
        std::optional<Effect> effect;
    };
    std::vector<Slot> _slots;
    std::uint64_t _order = 0;
    Effect* find(ScreenEffectHandle handle) noexcept;
    ScreenEffectHandle insert(Effect effect);
    static void retire(Slot& slot) noexcept;
};
}
