#include "game/gameplay/flight_objects.h"
#include "game/gameplay/projectile.h"
#include "engine/io/path/path_manager.h"
#include "engine/io/loaders/content_registry_loader.h"
#include "engine/loading/game_content_loader.h"
#include "engine/resources/resource_service.h"
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <stdexcept>

namespace {
using namespace elysia;
void require(bool condition,const std::string& message) {
    if(!condition)throw std::runtime_error(message);
}
core::RenderCommand sprite(const core::GameObject& object) {
    std::vector<core::RenderCommand> commands;
    object.submit_render_commands(commands);
    require(commands.size()==1&&commands[0].texture&&commands[0].alpha==255,
        "An idle sprite must remain visible and opaque");
    return commands[0];
}
void check_pickups_and_moon() {
    const auto level=game::content::level(1);
    for(int kind:{3,4,5,101}) {
        const auto found=std::find_if(level.at("entities").begin(),level.at("entities").end(),
            [kind](const auto& data){return data.at("kind")==kind;});
        require(found!=level.at("entities").end(),"First-level visual fixture missing");
        game::play::Entity entity(*found);
        const auto expected=ELYSIA_RESOURCES->find_texture(found->at("key").template get<std::string>()+".first");
        require(expected!=nullptr,"Normal-state texture missing");
        for(int frame=0;frame<180;++frame) {
            entity.update(1./60);
            require(sprite(entity).texture==expected,
                "Pickup/moon is cycling between normal and effect images: kind "+std::to_string(kind));
        }
    }
}
void check_projectile(bool homing) {
    physics::PhysicsWorld world({.gravity={0,0}});
    game::play::Player player({10000,10000});
    game::play::Projectile projectile({0,0},{100,0},homing,1,6);
    const auto handle=world.register_object(projectile,projectile.body_definition(),projectile.collider_definitions());
    require(handle.is_valid(),"Projectile registration failed");
    projectile.bind_physics(world,handle);
    const auto initial=sprite(projectile);
    for(int frame=0;frame<180;++frame) {
        require(!projectile.advance(1./60,{1000,0},player),"Unobstructed projectile reported a hit");
        world.step(1./60);
        const auto current=sprite(projectile);
        require(current.texture==initial.texture&&current.src_rect==initial.src_rect,
            "Flying projectile is playing a fading effect sequence");
    }
    require(!projectile.expired&&projectile.position().x>100,"Visual fix changed projectile travel/lifetime");
    projectile.unbind_physics();
    require(world.unregister_object(handle),"Projectile cleanup failed");
}
}

int main() {
    SDL_Surface* surface=nullptr;
    SDL_Renderer* renderer=nullptr;
    int result=1;
    try {
        require(io::PathManager::instance()->initialize(std::filesystem::current_path()).has_value(),"Path initialization failed");
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
        require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        require(TTF_Init(),SDL_GetError());
        require(MIX_Init(),SDL_GetError());
        surface=SDL_CreateSurface(1280,720,SDL_PIXELFORMAT_RGBA32);
        require(surface!=nullptr,SDL_GetError());
        renderer=SDL_CreateSoftwareRenderer(surface);
        require(renderer!=nullptr,SDL_GetError());
        {
            io::ContentRegistryLoader registry_loader;
            const auto registry=registry_loader.load(io::PathManager::instance()->content_registry());
            require(registry.has_value(),"Content registry load failed");
            loading::GameContentLoader loader;
            const auto started=loader.start(renderer,*registry,{});
            require(started.has_value(),started?"":started.error().diagnostic.message);
            const auto deadline=SDL_GetTicks()+30000;
            while(loader.is_running()&&SDL_GetTicks()<deadline){loader.update();SDL_Delay(1);}
            require(loader.is_finished(),loader.failure()?loader.failure()->diagnostic.message:"Content load timed out");
            check_pickups_and_moon();
            check_projectile(false);
            check_projectile(true);
        }
        std::cout<<"PASS: idle star/crystals/moon and flying projectiles remain visible over 180 frames; projectile movement preserved\n";
        result=0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';
    }
    if(renderer)SDL_DestroyRenderer(renderer);
    if(surface)SDL_DestroySurface(surface);
    MIX_Quit();TTF_Quit();SDL_Quit();
    return result;
}
