#pragma once
#include <SDL3/SDL.h>
namespace elysia::core
{
// Engine layout uses integer texture dimensions; SDL3 exposes them as floats.
inline bool texture_pixel_size(SDL_Texture* texture,int* width,int* height)
{
    float w=0,h=0;
    if (!SDL_GetTextureSize(texture,&w,&h)) return false;
    if (width) *width = static_cast<int>(w);
    if (height) *height = static_cast<int>(h);
    return true;
}
}
