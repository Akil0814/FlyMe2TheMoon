#pragma once

#include <cstdint>

#include "../../core/ui_element.h"

struct SDL_Texture;

namespace elysia::ui
{
class UiImage : public UiElement
{
public:
    UiImage(SDL_Texture* texture,
        elysia::core::Vector2 pos,elysia::core::Vector2 size,int order = 0);

    UiImage(SDL_Texture* texture,
        elysia::core::Rect rect,int order = 0);

    UiImage(SDL_Texture* texture,
        elysia::core::Vector2 center,elysia::core::Vector2 image_size,UiFromCenterTag,int order = 0);

    UiImage(SDL_Texture* texture,
        elysia::core::Vector2 center,elysia::core::Vector2 source_size,elysia::core::Vector2 render_size,
        UiFromCenterTag,int order = 0);

    // Replaces the caller-owned texture used by this image widget.
    void set_texture(SDL_Texture* texture);
    [[nodiscard]] SDL_Texture* texture() const;

    // Crops rendering to a source sub-rect in texture space.
    void set_source_rect(const elysia::core::Rect& rect);
    // Restores full-texture rendering after a previous source-rect crop.
    void clear_source_rect();

    // Emits one texture draw command for the current image and optional source rect.
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

private:
    SDL_Texture* _texture = nullptr;
    bool _has_source_rect = false;
    elysia::core::Rect _source_rect{};
};
}
