#include "result_star_effects.h"
#include "engine/resources/resource_service.h"
#include <numbers>
#include <stdexcept>

namespace game::frontend {
using namespace elysia;
namespace {
struct Spark {double life;float size;core::Vector2 velocity;};
// Original random() state is unavailable; keep each burst stable across redraws.
const auto sparks=[] {
    std::array<std::array<Spark,100>,3> bursts{};
    std::uint32_t random=0x106735e;
    const auto signed_unit=[&]{random=random*1664525u+1013904223u;return (random>>8)*(2.f/16777216)-1;};
    for(auto& burst:bursts)for(auto& spark:burst){
        spark.life=std::max(0.,.2+.5*signed_unit());
        spark.size=std::max(0.f,10+10*signed_unit());
        const float angle=360*signed_unit()*std::numbers::pi_v<float>/180;
        const float speed=150+15*signed_unit();
        spark.velocity={std::cos(angle)*speed,std::sin(angle)*speed};
    }
    return bursts;
}();
}
void submit_result_star_particles(std::vector<core::UiRenderCommand>& out,
    const core::Rect& slot,double elapsed,int index){
    // 0.2 s emission + at most 0.7 s life. No transforms on the star sprite.
    if(elapsed<=0||elapsed>=.9)return;
    auto* texture=ELYSIA_RESOURCES->find_texture("fly.result_star_particles");
    if(!texture||!SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_ADD))
        throw std::runtime_error("Missing or unsupported original result star particle texture");
    auto* star=ELYSIA_RESOURCES->find_texture("fly.InGameUI.starDown");
    float width=0,height=0;
    if(!star||!SDL_GetTextureSize(star,&width,&height))return;
    const float scale=slot.width()/(std::max(width,height)*.5f);
    const auto& burst=sparks.at(index);
    for(std::size_t i=0;i<burst.size();++i){
        // Original capacity/life = 100/0.2 = 500 particles per second.
        const double age=elapsed-(i+1)/500.;
        const auto& spark=burst[i];
        if(age<0||age>=spark.life)continue;
        core::Vector2 position{},velocity=spark.velocity;
        // Cocos gravity mode: inward radial and clockwise tangential acceleration.
        // Integrate in original Y-up coordinates, then reflect into screen space.
        for(double remaining=age;remaining>1e-9;){
            const float dt=static_cast<float>(std::min(remaining,1./60.));
            const float length=position.length();
            const auto radial=length>0?position*(1/length):core::Vector2{};
            const core::Vector2 tangent{-radial.y,radial.x};
            velocity+=(radial*-400+tangent*-144.74f)*dt;
            position+=velocity*dt;remaining-=dt;
        }
        const float progress=static_cast<float>(age/spark.life);
        const float size=(spark.size+(5-spark.size)*progress)*scale;
        auto command=core::make_ui_texture_command(texture,core::Rect::from_center(
            slot.center()+core::Vector2{position.x,-position.y}*scale,{size,size}));
        // Original start RGB=(128,158,182), end RGB=0; both alpha values are 0.
        // ONE / ONE_MINUS_SRC_ALPHA therefore adds RGB. SDL ADD supplies the
        // equivalent contribution using straight-alpha PNGs and opaque vertices.
        command.texture_color_modulation=core::TextureColorModulation{
            static_cast<Uint8>(128*(1-progress)),static_cast<Uint8>(158*(1-progress)),static_cast<Uint8>(182*(1-progress))};
        out.push_back(command);
    }
}
}
