#include "ui_image.h"

#include "../../../core/render/render_command.h"

#include <SDL3/SDL.h>

namespace elysia::ui
{
UiImage::UiImage(SDL_Texture* texture,elysia::core::Vector2 pos,elysia::core::Vector2 size,int order)
    : UiElement(pos,size,order) { set_texture(texture); }

UiImage::UiImage(SDL_Texture* texture,elysia::core::Rect rect,int order)
    : UiElement(rect,order) { set_texture(texture); }

UiImage::UiImage(SDL_Texture* texture,elysia::core::Vector2 center,elysia::core::Vector2 image_size,UiFromCenterTag,int order)
    : UiElement(center,image_size,from_center,order) { set_texture(texture); }

UiImage::UiImage(
    SDL_Texture* texture,
    elysia::core::Vector2 center,elysia::core::Vector2 source_size,elysia::core::Vector2 render_size,
    UiFromCenterTag,int order) : UiElement(center,render_size,from_center,order)
{
    set_texture(texture);
    set_source_rect(elysia::core::Rect(0.0f,0.0f,source_size.x,source_size.y));
}

void UiImage::set_texture(SDL_Texture* texture)
{
    _texture = texture;
    if (_texture)
        SDL_SetTextureBlendMode(_texture,SDL_BLENDMODE_BLEND);
}

SDL_Texture* UiImage::texture() const
{
    return _texture;
}

void UiImage::set_source_rect(const elysia::core::Rect& rect)
{
    _has_source_rect = true;
    _source_rect = rect;
}

void UiImage::clear_source_rect()
{
    _has_source_rect = false;
    _source_rect = elysia::core::Rect::zero();
}

void UiImage::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!_texture || !is_visible())
        return;

    elysia::core::UiRenderCommand command = elysia::core::make_ui_texture_command(_texture,screen_rect());
    if (_has_source_rect)
    {
        command.use_src_rect = true;
        command.src_rect = _source_rect;
    }

    apply_opacity(command);
    out_commands.push_back(command);
}
}