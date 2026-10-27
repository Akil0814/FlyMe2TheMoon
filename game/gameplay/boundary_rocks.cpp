#include "boundary_rocks.h"
#include "engine/resources/resource_service.h"
#include <algorithm>
#include <stdexcept>

namespace game::play {
BoundaryRocks::BoundaryRocks(const content::Json& level):GameObject(elysia::core::DepthLayer::Terrain){
    using namespace elysia::core;
    static const auto config=content::read("landscape_boundaries.json");
    const auto found=config.at("levels").find(std::to_string(level.at("id").get<int>()));
    if(found==config.at("levels").end())return;
    const auto& region=*found;
    const float width=level.at("width"),padding=config.at("padding"),overlap=config.at("overlap");
    const float top=region.at("top"),bottom=region.at("bottom");
    const auto key="fly.terrain"+std::to_string(level.at("suite").get<int>())+".Texture_040";
    auto* texture=ELYSIA_RESOURCES->find_texture(key);
    float texture_width=0,texture_height=0;
    if(!texture||!SDL_GetTextureSize(texture,&texture_width,&texture_height))
        throw std::runtime_error("Missing boundary rock texture: "+key);
    // Original HD terrain is drawn at half its pixel dimensions.
    const float tile_width=texture_width*.5f,tile_height=texture_height*.5f;
    if(overlap<0||overlap>=std::min(tile_width,tile_height))
        throw std::runtime_error("Invalid boundary rock overlap");
    const auto fill=[&](float left,float right){
        for(float y=top;y<bottom;y+=tile_height-overlap)
            for(float x=left;x<right;x+=tile_width-overlap){
                const float w=std::min(tile_width,right-x),h=std::min(tile_height,bottom-y);
                RenderCommand command;command.texture=texture;
                command.command_rect={x,y,w,h};
                // Crop the last tile rather than squeezing its rock pattern.
                command.use_src_rect=true;command.src_rect={0,0,w*2,h*2};
                _commands.push_back(command);
            }
    };
    // These explicit cutoffs stay behind the original closed corridor walls.
    fill(-padding,region.at("left"));
    fill(region.at("right"),width+padding);
}
void BoundaryRocks::submit_render_commands(std::vector<elysia::core::RenderCommand>& out)const{
    out.insert(out.end(),_commands.begin(),_commands.end());
}
}
