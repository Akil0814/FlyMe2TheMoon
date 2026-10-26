#pragma once
#include "game/particles/particle_emitter.h"
#include "engine/resources/resource_service.h"
#include <stdexcept>

namespace game::play {
class ThrustParticles {
public:
    ThrustParticles(){
        _texture=ELYSIA_RESOURCES->find_texture("fly.role_fire");
        // Original RoleView uses premultiplied RGB with zero particle alpha.
        // SDL additive blending plus a fading alpha expresses the same light sum.
        if(!_texture||!SDL_SetTextureBlendMode(_texture,SDL_BLENDMODE_ADD))throw std::runtime_error("Missing or unsupported role particle texture");
    }
    void clear(){_emitter.clear();}
    void update(double delta,elysia::core::Rect role,elysia::core::Vector2 thrust,elysia::core::Vector2 attachment,bool active){
        if(!active){clear();return;}
        // RoleView.createParticle / updateParticleByPushForce:fireAngleDegrees:.
        const float force=std::clamp(thrust.length(),0.f,1.f)*28;
        _emitter.update(delta,{{role.x()+role.width()*attachment.x,role.y()+role.height()*attachment.y},-thrust,
            std::clamp(force,2.f,32.f),std::clamp(force*.5f,1.f,16.f)});
    }
    void submit(std::vector<elysia::core::RenderCommand>& out)const{_emitter.submit(out,_texture);}
private:
    particles::ParticleEmitter _emitter;
    SDL_Texture* _texture=nullptr;
};
}
