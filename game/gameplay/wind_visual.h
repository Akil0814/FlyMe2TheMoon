#pragma once
#include "wind_field.h"
#include "engine/core/render/render_command.h"
#include "engine/resources/atlas/atlas.h"
#include <random>
#include <array>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace game::play {
// Wind.initFlowEffectSprites / initFadeSprites: crop a scrolling window from
// the second texture. The third texture supplies independent WindLeaf sprites.
class WindVisual {
public:
    using Vec=elysia::core::Vector2;
    WindVisual(int kind,const content::Json& info,const elysia::resources::FrameInfo& flow,
               const elysia::resources::FrameInfo& leaf,int seed):
        _flow(flow),_leaf(leaf),_random(static_cast<unsigned>(seed)) {
        _blocks=std::max(1,static_cast<int>(parameter(info,"NumBlocks",1)));
        _height=kind==8?64.f:kind==24?128.f:256.f;
        _row_height=std::min(_height,128.f);
        const float strength=static_cast<float>(parameter(info,"Strength",20))/20;
        _rate=120*std::clamp(strength,.25f,2.f);
        _leaf_speed=320*std::clamp(strength,.25f,2.f);
        _opacity=static_cast<Uint8>(std::clamp((.236f+.764f*std::clamp(strength,.5f,1.f))*255,0.f,255.f));
        const auto fade=info.value("Fade",std::string("00"));
        _fade_in=!fade.empty()&&fade[0]=='1';_fade_out=fade.size()>1&&fade[1]=='1';
        _leaves.resize((_blocks/2)*(kind==8?1:kind==24?2:3));
        for(auto& item:_leaves){item.wait=unit()*width()/_leaf_speed;restart(item);}
    }
    void update(double delta) {
        _dirty=true;
        // Native flow animation advances a 60-step texture window; the rate
        // depends on Strength, independently of the physics update clock.
        _time=std::fmod(_time+delta,60./_rate);
        const float dt=static_cast<float>(delta);
        for(auto& leaf:_leaves) {
            if(leaf.wait>0){leaf.wait-=dt;continue;}
            leaf.velocity.y+=leaf.acceleration*dt;leaf.position+=leaf.velocity*dt;
            leaf.rotation+=leaf.spin*dt;
            if(leaf.position.x<left()){restart(leaf);continue;}
            leaf.presented=leaf.position+leaf.noise;
            leaf.alpha=static_cast<Uint8>(255*std::clamp(std::min(leaf.position.x-left(),64-leaf.position.x)/48,0.f,1.f));
            leaf.acceleration=-160*(leaf.position.y-_height/2)/(_height/2);
            const float angle=-leaf.rotation*std::numbers::pi_v<float>/180;
            leaf.noise={16*std::cos(angle),16*std::sin(angle)};
            leaf.scale_y=leaf.scale*(.3f+.7f*std::abs(std::cos(angle-leaf.full_scale_angle)));
        }
    }
    void build_parts(std::vector<elysia::core::RenderCommand>& out,Vec center,float rotation) const {
        using namespace elysia::core;
        const float radians=rotation*std::numbers::pi_v<float>/180;
        const float scroll=static_cast<float>(std::floor(_time*_rate))*scroll_width()/60;
        const auto flow=[&](float x,float y,float w,Uint8 alpha) {
            const Vec local{width()/2-64+x+w/2,_height/2-y-_row_height/2};
            const Vec location=center+rotate_vector(local,radians);
            const Vec source=_flow._source_rect?_flow._source_rect->position():Vec{};
            RenderCommand command;command.texture=_flow._texture;command.alpha=alpha;
            command.command_rect={location.x-w/2,location.y-_row_height/2,w,_row_height};
            command.rotation_degrees=rotation;command.use_src_rect=true;
            command.src_rect={source.x+2*wrap(x+scroll,scroll_width()),source.y,2*w,2*_row_height};
            out.push_back(command);
        };
        for(int block=0;block<_blocks;++block) {
            const float trim_right=block==0&&_fade_in?12.f:0;
            const float trim_left=block==_blocks-1&&_fade_out?12.f:0;
            for(float y=0;y<_height;y+=_row_height)
                flow(-64.f*block+trim_left,y,64-trim_left-trim_right,_opacity);
        }
        // Twelve two-unit strips make each 24-unit fade fringe, straddling
        // the boundary. Match the original symmetric power-1.6 opacity curve.
        for(int edge=0;edge<2;++edge)if(edge==0?_fade_in:_fade_out)
            for(int strip=0;strip<12;++strip) {
                const float value=(12-strip)/13.f;
                const float fade=value<.5f?.5f*std::pow(2*value,1.6f):1-.5f*std::pow(2*(1-value),1.6f);
                const float x=edge==0?52+2.f*strip:left()+10-2.f*strip;
                for(float y=0;y<_height;y+=_row_height)flow(x,y,2,static_cast<Uint8>(_opacity*fade));
            }
        for(const auto& leaf:_leaves) {
            const Vec local{width()/2-64+leaf.presented.x,_height/2-leaf.presented.y};
            const Vec location=center+rotate_vector(local,radians);
            const Vec extent{_leaf._width*.5f*leaf.scale,_leaf._height*.5f*leaf.scale_y};
            RenderCommand command;command.texture=_leaf._texture;command.alpha=leaf.alpha;
            command.command_rect={location.x-extent.x/2,location.y-extent.y/2,extent.x,extent.y};
            command.rotation_degrees=rotation+leaf.rotation;
            command.use_src_rect=_leaf._source_rect.has_value();if(command.use_src_rect)command.src_rect=*_leaf._source_rect;
            out.push_back(command);
        }
    }
    void submit(std::vector<elysia::core::RenderCommand>& out,Vec center,float rotation) const {
        // The engine quantizes each command's destination rectangle. Compose
        // the native pieces first so rotated/scaled fields have one boundary,
        // rather than cracks between individually rounded blocks/fade strips.
        constexpr float margin=48;
        const Vec extent{width()+2*margin,_height+2*margin};
        auto* renderer=SDL_GetRendererFromTexture(_flow._texture);
        // SDL's software target path does not preserve the alpha needed by
        // this premultiplied composition. Keep correct component rendering
        // there; accelerated backends compose before the world transform.
        if(std::string_view(SDL_GetRendererName(renderer))=="software") {build_parts(out,center,rotation);return;}
        if(!_canvas) {
            _canvas.reset(SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,
                                            static_cast<int>(2*extent.x),static_cast<int>(2*extent.y)));
            if(!_canvas)throw std::runtime_error(SDL_GetError());
            // Drawing alpha sprites into a transparent target stores premultiplied
            // color. Avoid multiplying that color by alpha a second time on screen.
            checked(SDL_SetTextureBlendMode(_canvas.get(),SDL_BLENDMODE_BLEND_PREMULTIPLIED));
        }
        if(_dirty) {
            CanvasScope scope(renderer,_flow._texture,_leaf._texture);
            checked(SDL_SetRenderTarget(renderer,_canvas.get()));
            checked(SDL_SetRenderDrawColor(renderer,0,0,0,0));checked(SDL_RenderClear(renderer));
            std::vector<elysia::core::RenderCommand> parts;build_parts(parts,extent/2,0);
            for(const auto& part:parts) {
                if(part.alpha==0)continue;
                const auto& r=part.command_rect;
                const auto s=part.use_src_rect?part.src_rect:elysia::core::Rect{0,0,static_cast<float>(part.texture->w),static_cast<float>(part.texture->h)};
                const float angle=static_cast<float>(part.rotation_degrees)*std::numbers::pi_v<float>/180;
                const Vec pivot=r.center();std::array<SDL_Vertex,4> vertices;
                constexpr std::array<Vec,4> corners{{{0,0},{1,0},{1,1},{0,1}}};
                for(std::size_t i=0;i<corners.size();++i) {
                    const auto uv=corners[i];
                    const Vec position=2*(pivot+rotate_vector({(uv.x-.5f)*r.width(),(uv.y-.5f)*r.height()},angle));
                    vertices[i]={{position.x,position.y},{1,1,1,part.alpha/255.f},
                        {(s.x()+uv.x*s.width())/part.texture->w,(s.y()+uv.y*s.height())/part.texture->h}};
                }
                constexpr int indices[]{0,1,2,0,2,3};
                checked(SDL_RenderGeometry(renderer,part.texture,vertices.data(),4,indices,6));
            }
            _dirty=false;
        }
        elysia::core::RenderCommand command;command.texture=_canvas.get();command.rotation_degrees=rotation;
        command.command_rect={center.x-extent.x/2,center.y-extent.y/2,extent.x,extent.y};out.push_back(command);
    }
private:
    static void checked(bool result){if(!result)throw std::runtime_error(SDL_GetError());}
    struct CanvasScope {
        SDL_Renderer* renderer;
        SDL_Texture* previous;
        std::array<SDL_Texture*,2> sources;
        std::array<Uint8,2> alpha{};
        std::array<SDL_BlendMode,2> blend{};
        Uint8 r=0,g=0,b=0,a=0;
        CanvasScope(SDL_Renderer* target,SDL_Texture* flow,SDL_Texture* leaf):
            renderer(target),previous(SDL_GetRenderTarget(target)),sources{flow,leaf} {
            checked(SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a));
            for(std::size_t i=0;i<sources.size();++i){checked(SDL_GetTextureAlphaMod(sources[i],&alpha[i]));checked(SDL_GetTextureBlendMode(sources[i],&blend[i]));}
            for(auto* source:sources)checked(SDL_SetTextureBlendMode(source,SDL_BLENDMODE_BLEND));
        }
        ~CanvasScope() {
            for(std::size_t i=0;i<sources.size();++i){SDL_SetTextureAlphaMod(sources[i],alpha[i]);SDL_SetTextureBlendMode(sources[i],blend[i]);}
            SDL_SetRenderTarget(renderer,previous);SDL_SetRenderDrawColor(renderer,r,g,b,a);
        }
    };
    struct TextureDeleter {void operator()(SDL_Texture* texture)const{SDL_DestroyTexture(texture);}};
    struct Leaf {
        Vec position{},presented{},velocity{},noise{};
        float wait=0,acceleration=0,rotation=0,spin=0,scale=1,scale_y=1,full_scale_angle=0;
        Uint8 alpha=0;
    };
    float unit(){return static_cast<float>(_random()>>8)/16777216.f;}
    float width()const{return 64.f*_blocks;}
    float left()const{return 64-width();}
    float scroll_width()const{return _flow._width*.5f-64;}
    static float wrap(float value,float period){return value-period*std::floor(value/period);}
    void restart(Leaf& leaf) {
        leaf.position={64,(.05f+.9f*unit())*_height};leaf.presented=leaf.position;leaf.noise={};leaf.alpha=0;
        leaf.scale=.6f+.4f*unit();leaf.scale_y=leaf.scale;
        leaf.rotation=360*unit();leaf.spin=-320+640*unit();
        leaf.full_scale_angle=2*std::numbers::pi_v<float>*unit();
        leaf.velocity={-_leaf_speed*(.75f+.5f*unit()),-80+160*unit()};leaf.acceleration=0;
    }
    const elysia::resources::FrameInfo& _flow;
    const elysia::resources::FrameInfo& _leaf;
    std::mt19937 _random;
    std::vector<Leaf> _leaves;
    int _blocks=1;
    float _height=64,_row_height=64,_rate=120,_leaf_speed=320;
    Uint8 _opacity=255;
    bool _fade_in=false,_fade_out=false;
    double _time=0;
    mutable std::unique_ptr<SDL_Texture,TextureDeleter> _canvas;
    mutable bool _dirty=true;
};
}
