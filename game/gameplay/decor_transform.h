#pragma once
#include "game/content/content.h"
#include "engine/core/render/render_command.h"

namespace game::play {
// The original layers use anchor (0,0) and negative scale for mirrors.
// SDL's texture flip alone mirrors inside the old rectangle, so move the
// rectangle and its rotation pivot to keep the original lower-left anchor.
inline elysia::core::RenderCommand decor_transform(const content::Json& data,
    elysia::core::Vector2 offset={}) {
    using namespace elysia::core;
    const bool x=data.value("flip",false),y=data.value("flip_y",false);
    const float w=data.at("w"),h=data.at("h");
    RenderCommand command;
    command.command_rect={data.at("x").get<float>()+offset.x-(x?w:0),
        data.at("y").get<float>()+offset.y+(y?h:0),w,h};
    command.rotation_degrees=data.at("rotation");command.rotation_origin={x?1.f:0.f,y?0.f:1.f};
    command.flip=x?(y?SpriteFlip::Both:SpriteFlip::Horizontal):(y?SpriteFlip::Vertical:SpriteFlip::None);
    command.alpha=static_cast<Uint8>(std::clamp(data.value("alpha",255),0,255));
    return command;
}
}
