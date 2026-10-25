#include "game/gameplay/flight_objects.h"
#include "game/gameplay/projectile.h"
#include "game/gameplay/seesaw_parameters.h"
#include "game/gameplay/launcher_visual.h"
#include "engine/io/path/path_manager.h"
#include "engine/io/loaders/content_registry_loader.h"
#include "engine/loading/game_content_loader.h"
#include "engine/resources/resource_service.h"
#include "engine/core/render/render_command_projection.h"
#include "engine/core/render/sdl_render_command_executor.h"
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
void check_projectile_gravity() {
    game::play::Player player({10000,10000});
    for(bool homing:{false,true}) {
        physics::PhysicsWorld world({.units_per_meter=32,.gravity={0,320}});
        game::play::Projectile projectile({0,0},{100,0},homing,0,6);
        const auto handle=world.register_object(projectile,projectile.body_definition(),projectile.collider_definitions());
        require(handle.is_valid(),"Gravity test projectile registration failed");projectile.bind_physics(world,handle);
        for(int tick=0;tick<60;++tick){require(!projectile.advance(1./60,{1000,0},player),"Gravity test projectile reported a hit");world.step(1./60);}
        const auto state=projectile.physics_state();require(state.has_value(),"Projectile physics state missing");
        require(std::abs(state->position.x-100)<.1,"Gravity changed horizontal launch speed");
        if(homing)require(std::abs(state->position.y)<.01&&std::abs(state->velocity.y)<.01,"Homing projectile unexpectedly falls under gravity");
        else require(std::abs(state->velocity.y-320)<.1&&std::abs(state->position.y-160)<2,
            "Ordinary projectile must fall along a gravity-driven trajectory: y="+std::to_string(state->position.y)+", vy="+std::to_string(state->velocity.y)+", mass="+std::to_string(state->mass));
        projectile.unbind_physics();require(world.unregister_object(handle),"Gravity test cleanup failed");
    }
    {
        physics::PhysicsWorld world({.units_per_meter=32,.gravity={0,320}});
        game::play::Projectile projectile({0,0},{100,-160},false,1,6);
        const auto handle=world.register_object(projectile,projectile.body_definition(),projectile.collider_definitions());
        require(handle.is_valid(),"Arc test registration failed");projectile.bind_physics(world,handle);
        for(int tick=0;tick<30;++tick){projectile.advance(1./60,{1000,0},player);world.step(1./60);}
        require(projectile.position().y< -35&&std::abs(projectile.velocity().y)<1,"Upward projectile did not reach its ballistic apex");
        for(int tick=0;tick<30;++tick){projectile.advance(1./60,{1000,0},player);world.step(1./60);}
        require(projectile.velocity().y>150&&projectile.position().y> -2,"Projectile did not fall back after its apex");
        projectile.unbind_physics();require(world.unregister_object(handle),"Arc test cleanup failed");
    }
    {
        physics::PhysicsWorld world({.units_per_meter=32,.gravity={0,320}});
        core::GameObject floor(core::DepthLayer::Terrain);floor.set_world_rect({0,0,1,1});
        physics::Collider obstacle;obstacle.shape=physics::BoxShape::from_rect({-1000,60,2000,2});obstacle.filter={.category=2,.mask=8};
        const auto terrain=world.register_object(floor,physics::BodyDefinition{.type=physics::BodyType::Static},std::span(&obstacle,1));
        game::play::Projectile projectile({0,0},{100,0},false,1,6);
        const auto handle=world.register_object(projectile,projectile.body_definition(),projectile.collider_definitions());
        require(terrain.is_valid()&&handle.is_valid(),"Falling collision test registration failed");projectile.bind_physics(world,handle);
        for(int tick=0;tick<120&&!projectile.expired;++tick){projectile.advance(1./60,{1000,0},player);if(!projectile.expired)world.step(1./60);}
        require(projectile.expired&&projectile.position().y<=53.1f,"Falling projectile passed through terrain before expiring");
        projectile.unbind_physics();require(world.unregister_object(handle)&&world.unregister_object(terrain),"Falling collision cleanup failed");
    }
}
game::content::Json mechanism(int kind) {
    for(int level=1;level<=60;++level) {
        const auto level_data=game::content::level(level);
        for(auto data:level_data.at("entities"))
            if(data.at("kind")==kind){data["rotation"]=0;data["info"]["Spikes"]="0000";return data;}
    }
    throw std::runtime_error("Mechanism visual fixture missing: "+std::to_string(kind));
}
std::vector<core::RenderCommand> commands(const core::GameObject& object) {
    std::vector<core::RenderCommand> result;object.submit_render_commands(result);return result;
}
void check_platforms_and_rods() {
    for(int kind:{105,106,107})for(bool locked:{false,true}) {
        auto data=mechanism(kind);data["info"]["Lock"]=locked?"1":"0";
        game::play::Entity entity(data);
        const auto key=data.at("key").get<std::string>();
        const auto expected=locked?ELYSIA_RESOURCES->find_atlas(key)->texture_at(3):ELYSIA_RESOURCES->find_texture(key+".first");
        for(int frame=0;frame<180;++frame){entity.update(1./60);require(sprite(entity).texture==expected,"Platform is cycling between movement/lock/highlight states");}
    }
    for(int kind:{108,109,110,117})for(double anchor:{0.,.5,1.}) {
        auto data=mechanism(kind);data["info"]["Anchor"]=anchor;
        game::play::Entity entity(data);
        const auto key=data.at("key").get<std::string>();
        const auto expected=anchor==.5?ELYSIA_RESOURCES->find_texture(key+".first"):ELYSIA_RESOURCES->find_atlas(key)->texture_at(1);
        for(int frame=0;frame<180;++frame) {
            entity.update(1./60);const auto drawn=commands(entity);
            require(drawn.size()==2,"Rod must draw its plank and separate pivot together");
            require(drawn[0].texture==expected,"Rod plank state is cycling");
            require(drawn[0].flip==(anchor==1.?core::SpriteFlip::Horizontal:core::SpriteFlip::None),"End-mounted rod variant has incorrect orientation");
            require(drawn[1].texture==ELYSIA_RESOURCES->find_atlas(key+".S01")->texture_at(0),"Rod pivot texture missing");
            const auto offset=drawn[1].command_rect.center()-drawn[0].command_rect.center();
            require(std::abs(offset.x-game::play::seesaw_anchor(kind,data.at("info")))<.001&&std::abs(offset.y)<.001,"Rod pivot does not match its physical anchor");
        }
    }
}
void check_tracking_flower() {
    auto data=mechanism(113);data["info"]["Seq"]="0.1,2";data["info"]["Lag"]="0";
    game::play::Entity flower(data);
    const auto key=data.at("key").get<std::string>();const auto* atlas=ELYSIA_RESOURCES->find_atlas(key);
    const auto closed=commands(flower);
    require(closed.size()==3,"Tracking flower must draw its stem and both petals together");
    require(closed[0].texture==ELYSIA_RESOURCES->find_texture(key+".first")&&closed[1].texture==atlas->texture_at(2)&&closed[2].texture==atlas->texture_at(1),"Tracking flower component textures/order incorrect");
    for(int frame=0;frame<180;++frame) {
        flower.update(1./60);const auto drawn=commands(flower);
        require(drawn.size()==3,"Tracking flower component disappeared");
        for(int part=0;part<3;++part)require(drawn[part].texture==closed[part].texture&&drawn[part].command_rect==closed[part].command_rect,"Idle flower is cycling component frames");
    }
    require(flower.shots_due(.2,flower.center())>0,"Tracking flower test did not fire");
    flower.update(.25);const auto opened=commands(flower);
    require(std::abs(opened[1].rotation_degrees)>20&&std::abs(opened[2].rotation_degrees)>20,"Flower petals did not open after firing");
    flower.update(1);const auto recovered=commands(flower);
    require(std::abs(recovered[1].rotation_degrees)<.001&&std::abs(recovered[2].rotation_degrees)<.001,"Flower petals did not return to rest");
}
void check_mechanism_interpolation() {
    auto data=mechanism(109);data["info"]["Anchor"]=.25;
    physics::PhysicsWorld world({.gravity={0,0}});
    game::play::Entity rod(data);
    const auto handle=world.register_object(rod,rod.body_definition(),rod.collider_definitions());
    require(handle.is_valid(),"Rod registration failed");rod.bind_physics(world,handle);
    require(world.set_velocity(handle,{60,0})&&world.set_angular_velocity(handle,1),"Rod test motion rejected");
    world.step(1./60);world.finalize_frame(.25);
    const auto pose=world.render_pose(handle);const auto drawn=commands(rod);
    require(pose&&drawn.size()==2,"Interpolated rod pose missing");
    require((drawn[0].command_rect.center()-pose->position).length()<.001,"Rod bypasses position interpolation");
    require(std::abs(drawn[0].rotation_degrees-pose->angle*180/std::numbers::pi_v<float>)<.001,"Rod bypasses angle interpolation");
    const float anchor=game::play::seesaw_anchor(109,data.at("info"));
    const auto expected=pose->position+core::Vector2{anchor*std::cos(pose->angle),anchor*std::sin(pose->angle)};
    require((drawn[1].command_rect.center()-expected).length()<.001,"Pivot and plank use different presentation poses");
    rod.unbind_physics();require(world.unregister_object(handle),"Rod cleanup failed");
}
void check_portals() {
    for(bool exit:{false,true}) {
        auto data=mechanism(115);
        if(exit)data["info"].erase("TargetPortalID");else data["info"]["TargetPortalID"]="1";
        game::play::Entity portal(data);
        const auto key=data.at("key").get<std::string>()+(exit?".S02":".S01");
        const auto* atlas=ELYSIA_RESOURCES->find_atlas(key);
        const auto initial=commands(portal);
        require(initial.size()==24,"Portal must draw both groups of twelve ring sprites");
        bool animated=false;
        for(int tick=0;tick<180;++tick) {
            const auto drawn=commands(portal);require(drawn.size()==24,"Portal ring component disappeared");
            unsigned opacity=0;bool left=false,right=false,above=false,below=false;
            for(std::size_t i=0;i<drawn.size();++i) {
                const auto& part=drawn[i];
                require(part.texture==atlas->texture_at(i/12),"Portal entrance/exit or ring texture incorrect");
                const auto offset=part.command_rect.center()-portal.center();
                require(offset.length()<34&&part.command_rect.width()>0&&part.command_rect.height()>0,"Portal component escaped its ring");
                opacity+=part.alpha;
                if(part.alpha>50){left|=offset.x< -5;right|=offset.x>5;above|=offset.y< -5;below|=offset.y>5;}
                animated|=part.command_rect!=initial[i].command_rect||part.alpha!=initial[i].alpha;
            }
            require(opacity>1500&&left&&right&&above&&below,"Portal became invisible or no longer surrounds its center");
            portal.update(1./60);
        }
        require(animated,"Portal ring animation is frozen");
        game::play::Entity loop(data);loop.update(40./60.);const auto wrapped=commands(loop);
        for(std::size_t i=0;i<initial.size();++i)require(wrapped[i].command_rect==initial[i].command_rect&&wrapped[i].alpha==initial[i].alpha,"Portal 40-frame animation does not loop");
        data["rotation"]=90;game::play::Entity rotated(data);const auto turned=commands(rotated);
        for(std::size_t i=0;i<initial.size();++i) {
            const auto offset=initial[i].command_rect.center()-portal.center();
            require((turned[i].command_rect.center()-rotated.center()-core::Vector2{-offset.y,offset.x}).length()<.001,"Portal components do not follow its rotation");
        }
    }
}
void check_sleeping_bombs() {
    auto data=mechanism(116);data["info"]["Alarm"]="0.25";
    const auto key=data.at("key").get<std::string>();
    const auto* leaf=ELYSIA_RESOURCES->find_atlas(key+".S02")->frame_at(0);
    const auto* center=ELYSIA_RESOURCES->find_atlas(key+".S01")->frame_at(0);
    game::play::Entity bomb(data);const auto initial=commands(bomb);bool animated=false;
    for(int tick=0;tick<180;++tick) {
        const auto drawn=commands(bomb);
        require(bomb.bomb_state()==0&&drawn.size()==7,"Sleeping bomb must show six petals and its center before activation");
        for(std::size_t i=0;i<drawn.size();++i) {
            require(drawn[i].texture==(i<6?leaf->_texture:center->_texture)&&drawn[i].alpha==255,"Sleeping bomb is cycling between component/effect images");
            const auto offset=drawn[i].command_rect.center()-bomb.center();
            require(i<6?(offset.length()>7&&offset.length()<11):offset.length()<.001,"Sleeping bomb component is misplaced");
            require(std::abs(drawn[i].command_rect.width()-(i<6?12.5:7))<.001,"Sleeping bomb component does not retain its native size");
            if(tick>0)animated|=drawn[i].command_rect!=initial[i].command_rect;
        }
        bomb.update(1./60);
    }
    require(animated,"Sleeping bomb breathing animation is frozen");
    game::play::Entity loop(data);loop.update(1.5);const auto wrapped=commands(loop);
    for(std::size_t i=0;i<initial.size();++i)require(wrapped[i].command_rect==initial[i].command_rect,"Sleeping bomb 90-frame animation does not loop");
    data["rotation"]=90;game::play::Entity turned(data);const auto rotated=commands(turned);
    for(std::size_t i=0;i<initial.size();++i) {
        const auto offset=initial[i].command_rect.center()-bomb.center();
        require((rotated[i].command_rect.center()-turned.center()-core::Vector2{-offset.y,offset.x}).length()<.001,"Sleeping bomb components do not follow rotation");
    }
    physics::PhysicsWorld world({.gravity={0,0}});
    const auto handle=world.register_object(bomb,bomb.body_definition(),bomb.collider_definitions());
    require(handle.is_valid(),"Bomb visual transition registration failed");bomb.bind_physics(world,handle);
    core::GameObject target(core::DepthLayer::Character);target.set_world_rect({bomb.center().x,bomb.center().y,1,1});
    physics::Collider collider;collider.shape=physics::CircleShape{{0,0},1};collider.filter={.category=1,.mask=8};
    const auto role=world.register_object(target,physics::BodyDefinition{.type=physics::BodyType::Static},std::span(&collider,1));
    require(role.is_valid(),"Bomb activation fixture registration failed");
    bomb.update_bomb(0,{});require(bomb.bomb_state()==1&&commands(bomb).size()==1,"Bomb did not replace sleeping petals with its alarm visual");
    bomb.update_bomb(.26,{});require(bomb.bomb_state()==2&&commands(bomb).size()==1,"Bomb did not replace alarm with its blast visual");
    bomb.update_bomb(.5,{});require(bomb.bomb_state()==3&&commands(bomb).empty(),"Exploded bomb still draws sleeping petals");
    bomb.unbind_physics();require(world.unregister_object(handle)&&world.unregister_object(role),"Bomb visual transition cleanup failed");
}
void save_bomb_preview(SDL_Renderer* renderer) {
    require(SDL_SetRenderDrawColor(renderer,16,25,38,255)&&SDL_RenderClear(renderer),SDL_GetError());
    for(int column=0;column<4;++column) {
        auto data=mechanism(116);if(column==3)data["rotation"]=45;
        game::play::Entity bomb(data);bomb.update(column*.375);
        const camera::Camera camera(bomb.position(),{1280,720},8);
        for(const auto& command:commands(bomb)) {
            auto projected=core::project_render_command_to_screen(command,camera);
            projected.screen_rect=projected.screen_rect.translated(core::Vector2{160.f+320*column,360}-core::Vector2{640,360});
            core::require_render_success(core::execute_render_command(renderer,projected));
        }
    }
    SDL_Surface* captured=SDL_RenderReadPixels(renderer,nullptr);require(captured!=nullptr,SDL_GetError());
    std::filesystem::create_directories("out/qa");
    const bool saved=SDL_SaveBMP(captured,"out/qa/bomb-idle-visual-preview.bmp");SDL_DestroySurface(captured);require(saved,SDL_GetError());
}
void save_portal_preview(SDL_Renderer* renderer) {
    require(SDL_SetRenderDrawColor(renderer,16,25,38,255)&&SDL_RenderClear(renderer),SDL_GetError());
    for(int row=0;row<2;++row)for(int column=0;column<4;++column) {
        auto data=mechanism(115);
        if(row)data["info"].erase("TargetPortalID");else data["info"]["TargetPortalID"]="1";
        game::play::Entity portal(data);portal.update(column/6.);
        const camera::Camera camera(portal.position(),{1280,720},4);
        for(const auto& command:commands(portal)) {
            auto projected=core::project_render_command_to_screen(command,camera);
            projected.screen_rect=projected.screen_rect.translated(core::Vector2{160.f+320*column,180.f+360*row}-core::Vector2{640,360});
            core::require_render_success(core::execute_render_command(renderer,projected));
        }
    }
    SDL_Surface* captured=SDL_RenderReadPixels(renderer,nullptr);require(captured!=nullptr,SDL_GetError());
    std::filesystem::create_directories("out/qa");
    const bool saved=SDL_SaveBMP(captured,"out/qa/portal-visual-preview.bmp");SDL_DestroySurface(captured);require(saved,SDL_GetError());
}
void save_mechanism_preview(SDL_Renderer* renderer) {
    require(SDL_SetRenderDrawColor(renderer,16,25,38,255)&&SDL_RenderClear(renderer),SDL_GetError());
    const auto draw=[&](const core::GameObject& object,core::Vector2 screen,float zoom) {
        const camera::Camera camera(object.position(),{1280,720},zoom);
        for(const auto& command:commands(object)) {
            auto projected=core::project_render_command_to_screen(command,camera);
            projected.screen_rect=projected.screen_rect.translated(screen-core::Vector2{640,360});
            core::require_render_success(core::execute_render_command(renderer,projected));
        }
    };
    for(int column=0;column<6;++column) {
        auto data=mechanism(105+column/2);data["info"]["Lock"]=column%2;
        game::play::Entity platform(data);draw(platform,{100.f+210*column,90},2);
    }
    for(int column=0;column<3;++column) {
        auto data=mechanism(109);data["info"]["Anchor"]=column*.5;
        game::play::Entity rod(data);draw(rod,{220.f+420*column,250},2);
    }
    for(int column=0;column<3;++column) {
        auto data=mechanism(113);data["info"]["Seq"]="0.1,2";data["info"]["Lag"]="0";
        if(column==2)data["rotation"]=45;
        game::play::Entity flower(data);
        if(column>0){require(flower.shots_due(.2,flower.center())>0,"Preview flower did not fire");flower.update(.25);}
        draw(flower,{220.f+420*column,560},4);
    }
    SDL_Surface* captured=SDL_RenderReadPixels(renderer,nullptr);
    require(captured!=nullptr,SDL_GetError());
    std::filesystem::create_directories("out/qa");
    const bool saved=SDL_SaveBMP(captured,"out/qa/mechanism-visual-preview.bmp");
    SDL_DestroySurface(captured);require(saved,SDL_GetError());
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
            check_platforms_and_rods();
            check_tracking_flower();
            check_portals();
            check_sleeping_bombs();
            check_mechanism_interpolation();
            check_projectile(false);
            check_projectile(true);
            check_projectile_gravity();
            save_mechanism_preview(renderer);
            save_portal_preview(renderer);
            save_bomb_preview(renderer);
        }
        std::cout<<"PASS: idle/mechanism/portal/bomb visuals, ordinary projectile gravity/arcs/terrain hits, homing motion preserved\n";
        result=0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';
    }
    if(renderer)SDL_DestroyRenderer(renderer);
    if(surface)SDL_DestroySurface(surface);
    MIX_Quit();TTF_Quit();SDL_Quit();
    return result;
}
