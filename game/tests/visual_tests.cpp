#include "game/gameplay/flight_objects.h"
#include "game/gameplay/projectile.h"
#include "game/gameplay/seesaw_parameters.h"
#include "game/gameplay/launcher_visual.h"
#include "game/gameplay/decor_transform.h"
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
std::vector<core::RenderCommand> commands(const core::GameObject& object);
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
void check_role_effects(SDL_Renderer* renderer) {
    using namespace game::play;
    Player player({100,300});
    require(commands(player).size()==1,"Normal role must draw one body sprite");
    for(const auto effect:{RoleEffect::Enter,RoleEffect::Moon,RoleEffect::PortalIn}) {
        player.play_effect(effect);
        for(int frame=0;frame<=40;++frame) {
            const auto pose=role_effect_pose(effect,frame/30.);
            const auto drawn=commands(player);
            require(drawn.size()==static_cast<size_t>(pose.body_visible)+(pose.image!=BallImage::None),"Role body/effect visibility does not follow native timeline");
            if(pose.image!=BallImage::None) {
                const std::string atlas="fly.CharActions.charEffects."+std::string(pose.image==BallImage::Ball?"ball":pose.image==BallImage::Disappear?"ballDisapear":"ballExplode");
                const auto* expected=ELYSIA_RESOURCES->find_atlas(atlas)->frame_at(pose.frame);
                require(drawn.back().texture==expected->_texture&&drawn.back().use_src_rect==expected->_source_rect.has_value(),"Role effect uses wrong atlas/frame");
                require(!expected->_source_rect||drawn.back().src_rect==*expected->_source_rect,"Role effect atlas frame rectangle wrong");
                require(std::abs(drawn.back().command_rect.width()-90*pose.scale)<.001,"Role ball native scale incorrect");
            }
            player.update(1./30);
        }
    }
    require(role_effect_pose(RoleEffect::Enter,9./30).body_visible==false&&role_effect_pose(RoleEffect::Enter,10./30).body_visible,"Enter must reveal body at frame 10");
    require(role_effect_pose(RoleEffect::Moon,5./30).body_visible&&!role_effect_pose(RoleEffect::Moon,6./30).body_visible,"Moon must conceal body at frame 6");
    require(role_effect_pose(RoleEffect::PortalIn,19./30).image==BallImage::Disappear&&role_effect_pose(RoleEffect::PortalIn,20./30).image==BallImage::None,"Portal collapse must finish at frame 20");
    player.die();player.update(3);player.dead=false;player.play_effect(RoleEffect::Enter);
    require(commands(player).empty(),"Respawn must reset effect frame/visibility");
    player.update(1);require(commands(player).size()==1,"Respawn did not restore normal body");
    player.present_at(core::Vector2{500,500},.5f,true);require(commands(player).empty(),"Role must be hidden during portal camera travel");
    player.present_at(std::nullopt);
    // Opening pan hides the role without consuming its later entrance animation.
    player.play_effect(RoleEffect::None);player.present_at(std::nullopt,1,true);player.update(3);
    require(!player.body_visible()&&commands(player).empty(),"Role appeared before the opening camera returned");
    player.present_at(std::nullopt);player.play_effect(RoleEffect::Enter);
    require(!player.entry_effect_finished()&&commands(player).empty(),"Deferred entrance did not restart at frame zero");
    player.update(15./30.);require(player.body_visible()&&!player.entry_effect_finished(),"Controls must wait for the whole entrance effect");
    player.update(1./30.);require(player.entry_effect_finished()&&commands(player).size()==1,"Deferred entrance did not finish cleanly");

    // Exercise the public body disable/teleport/enable path with an actual role.
    physics::PhysicsWorld world({.units_per_meter=32,.gravity={0,320}});
    const auto handle=world.register_object(player,player.body_definition(),player.collider_definitions());
    require(handle.is_valid(),"Transition role registration failed");player.bind_physics(world,handle);player.set_velocity({40,-60});const auto velocity=player.velocity();
    const auto start=player.position();require(world.set_body_enabled(handle,false),"Transition body disable failed");
    for(int i=0;i<120;++i)world.step(1./60);
    require((player.position()-start).length()<.001,"Disabled role drifted under gravity");
    require(world.teleport_object(handle,{700,800},physics::TeleportVelocityMode::Preserve)&&world.set_body_enabled(handle,true),"Transition body restore failed");
    require(world.set_velocity(handle,velocity),"Portal velocity snapshot restore failed");
    world.step(1./60);require(player.position().x>700&&player.position().y<800,"Portal velocity was lost or body remained disabled");
    player.unbind_physics();require(world.unregister_object(handle),"Transition role cleanup failed");

    require(SDL_SetRenderDrawColor(renderer,24,36,54,255)&&SDL_RenderClear(renderer),SDL_GetError());
    const std::array<RoleEffect,3> effects{RoleEffect::Enter,RoleEffect::Moon,RoleEffect::PortalIn};
    const std::array<std::array<int,8>,3> times{{{1,4,5,9,10,12,15,16},{0,5,6,15,25,31,33,36},{0,5,6,10,15,16,19,20}}};
    for(int row=0;row<3;++row)for(int column=0;column<8;++column) {
        Player sample({0,0});sample.play_effect(effects[row]);sample.update(times[row][column]/30.);
        sample.present_at(core::Vector2{80.f+160*column,120.f+240*row},1.5f);
        const camera::Camera preview_camera({640,360},{1280,720},1);
        for(const auto& command:commands(sample))core::require_render_success(core::execute_render_command(renderer,core::project_render_command_to_screen(command,preview_camera)));
    }
    auto* captured=SDL_RenderReadPixels(renderer,nullptr);require(captured!=nullptr,SDL_GetError());
    std::filesystem::create_directories("out/qa");const bool saved=SDL_SaveBMP(captured,"out/qa/role-transition-effects.bmp");SDL_DestroySurface(captured);require(saved,SDL_GetError());
}
void check_moon_highlight(SDL_Renderer* renderer) {
    const auto level=game::content::level(1);
    const auto data=*std::find_if(level.at("entities").begin(),level.at("entities").end(),[](const auto& e){return e.at("kind")==101;});
    game::play::Entity moon(data);
    const auto normal=sprite(moon);
    const auto* highlight=ELYSIA_RESOURCES->find_atlas(data.at("key").get<std::string>())->frame_at(1);
    require(highlight&&moon.moon_highlight_alpha()==0,"Idle moon must not light up");
    moon.light_moon();
    const std::map<int,int> samples{{0,0},{30,23},{60,93},{99,255},{144,255},{218,255},{219,0},{240,0}};
    for(int frame=0;frame<=240;++frame) {
        if(samples.contains(frame))require(moon.moon_highlight_alpha()==samples.at(frame),"Moon opacity differs from original table samples");
        const auto drawn=commands(moon);
        require(drawn[0].texture==normal.texture&&drawn[0].alpha==255,"Moon glow must retain its normal base image");
        require(drawn.size()==(moon.moon_highlight_alpha()>0?2:1),"Moon highlight layer visibility incorrect");
        if(drawn.size()==2) {
            require(drawn[1].texture==highlight->_texture&&drawn[1].alpha==moon.moon_highlight_alpha(),"Moon must overlay the second image at the table opacity");
            require(drawn[1].command_rect==drawn[0].command_rect&&drawn[1].rotation_degrees==drawn[0].rotation_degrees,"Moon highlight and base image do not align");
            require(drawn[1].use_src_rect==highlight->_source_rect.has_value()&&(!highlight->_source_rect||drawn[1].src_rect==*highlight->_source_rect),"Moon highlight atlas frame incorrect");
        }
        moon.update(1./60);
    }
    moon.light_moon();moon.update(1.65);require(moon.moon_highlight_alpha()==255,"Moon highlight did not replay after tutorial reset");
    moon.light_moon();require(moon.moon_highlight_alpha()==0,"Repeated moon entry did not restart light-up");
    require(SDL_SetRenderDrawColor(renderer,24,36,54,255)&&SDL_RenderClear(renderer),SDL_GetError());
    const std::array<double,6> times{0,.5,1,1.65,2.4,3.65};
    const camera::Camera preview_camera({640,360},{1280,720},1);
    for(int i=0;i<6;++i) {
        auto preview=data;preview["x"]=210+420*(i%3);preview["y"]=180+360*(i/3);
        game::play::Entity sample(preview);sample.light_moon();sample.update(times[i]);
        for(const auto& command:commands(sample))core::require_render_success(core::execute_render_command(renderer,core::project_render_command_to_screen(command,preview_camera)));
    }
    auto* captured=SDL_RenderReadPixels(renderer,nullptr);require(captured!=nullptr,SDL_GetError());
    const bool saved=SDL_SaveBMP(captured,"out/qa/moon-highlight-preview.bmp");SDL_DestroySurface(captured);require(saved,SDL_GetError());
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
void check_thrower_head(SDL_Renderer* renderer) {
    auto data=mechanism(112);data["info"]={{"Seq","0.1,2"},{"Lag","0"},{"FixedAngle","90"}};
    const auto key=data.at("key").get<std::string>();const auto* head=ELYSIA_RESOURCES->find_atlas(key+".S01")->frame_at(0);
    for(float angle:{-90.f,0.f,48.f,90.f,180.f,270.f}) {
        data["rotation"]=angle;game::play::Entity thrower(data);
        const auto rest=commands(thrower);require(rest.size()==2,"Thrower must draw base and S01 head together");
        require(rest[0].texture==ELYSIA_RESOURCES->find_texture(key+".first")&&rest[1].texture==head->_texture,"Thrower base/head textures incorrect");
        require(std::abs(rest[1].command_rect.width()-37.5f)<.001&&std::abs(rest[1].command_rect.height()-38.5f)<.001,"Thrower head native size incorrect");
        require(rest[1].rotation_origin==core::Vector2{.5f,.656f}&&std::abs(rest[1].rotation_degrees-angle)<.001,"Thrower fixed aim or head anchor incorrect");
        const auto mount=rest[1].command_rect.position()+core::Vector2{rest[1].command_rect.width()*.5f,rest[1].command_rect.height()*.656f};
        const float rad=angle*std::numbers::pi_v<float>/180;
        require((mount-(thrower.center()+core::Vector2{21*std::sin(rad),-21*std::cos(rad)})).length()<.001,"Thrower head does not join the rotated base");
        require(thrower.shots_due(.2,thrower.center())==1,"Thrower visual changed firing schedule");
        thrower.update(.15);const auto recoil=commands(thrower);
        require(recoil.size()==2&&recoil[0].command_rect==rest[0].command_rect,"Thrower recoil changed the base");
        require(std::abs(recoil[1].command_rect.width()-37.5f*1.11f)<.001&&std::abs(recoil[1].command_rect.height()-38.5f*.85f)<.001,"Thrower recoil differs from original frame 9");
        thrower.update(1);require(commands(thrower)[1].command_rect==rest[1].command_rect,"Thrower head did not recover after firing");
        thrower.shots_due(2,thrower.center());require(commands(thrower)[1].command_rect==rest[1].command_rect,"Repeated thrower shot did not restart recoil");
    }
    data["rotation"]=0;data["info"].erase("FixedAngle");game::play::Entity tracking(data);
    tracking.shots_due(.1,tracking.center()+core::Vector2{100,0});
    require(std::abs(commands(tracking)[1].rotation_degrees-45)<.001,"Tracking thrower head must use native 5*dt smoothing");
    data["info"]["FixedAngle"]="90";data["info"]["CarrierID"]="0";
    game::play::Entity carried(data);physics::PhysicsWorld world({.gravity={0,0}});
    const auto handle=world.register_object(carried,carried.body_definition(),carried.collider_definitions());
    require(handle.is_valid(),"Carried thrower registration failed");carried.bind_physics(world,handle);
    require(world.set_velocity(handle,{60,0})&&world.set_angular_velocity(handle,1),"Carried thrower test motion rejected");
    world.step(1./60);world.finalize_frame(.25);const auto pose=world.render_pose(handle);const auto drawn=commands(carried);
    require(pose&&std::abs(drawn[1].rotation_degrees-pose->angle*180/std::numbers::pi_v<float>)<.001,"Carried thrower head bypasses angle interpolation");
    const auto mount=drawn[1].command_rect.position()+core::Vector2{18.75f,38.5f*.656f};
    require((mount-(pose->position+core::Vector2{21*std::sin(pose->angle),-21*std::cos(pose->angle)})).length()<.001,"Carried thrower head bypasses position interpolation");
    carried.unbind_physics();require(world.unregister_object(handle),"Carried thrower cleanup failed");

    require(SDL_SetRenderDrawColor(renderer,24,36,54,255)&&SDL_RenderClear(renderer),SDL_GetError());
    data["info"].erase("CarrierID");
    for(int row=0;row<2;++row)for(int column=0;column<3;++column) {
        data["rotation"]=(column-1)*90;game::play::Entity sample(data);
        if(row){sample.shots_due(.2,sample.center());sample.update(.15);}
        const camera::Camera preview(sample.position(),{1280,720},3);
        for(const auto& command:commands(sample)) {
            auto projected=core::project_render_command_to_screen(command,preview);
            projected.screen_rect=projected.screen_rect.translated(core::Vector2{210.f+420*column-640,180.f+360*row-360});
            core::require_render_success(core::execute_render_command(renderer,projected));
        }
    }
    auto* captured=SDL_RenderReadPixels(renderer,nullptr);require(captured!=nullptr,SDL_GetError());
    const bool saved=SDL_SaveBMP(captured,"out/qa/thrower-head-preview.bmp");SDL_DestroySurface(captured);require(saved,SDL_GetError());
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
void check_decoration_anchors() {
    // Independent corner mapping from the native lower-left anchor and signed
    // local scale, including 45-degree rotations and all mirror combinations.
    for(bool x:{false,true})for(bool y:{false,true})for(float angle:{0.f,45.f,90.f,-90.f,180.f}) {
        game::content::Json d={{"x",200},{"y",300},{"w",64},{"h",32},{"rotation",angle},{"flip",x},{"flip_y",y},
            {"key","fly.terrain3.Texture_001"},{"layer","Texture"},{"layerWidth",1000},{"layerHeight",1000}};
        const auto command=game::play::decor_transform(d);
        const core::Vector2 anchor{200,332},extent{64,32};
        const auto pivot=command.command_rect.position()+core::Vector2{command.rotation_origin.x*extent.x,command.rotation_origin.y*extent.y};
        require((pivot-anchor).length()<.001,"Mirrored decoration moved its original lower-left anchor");
        for(float u:{0.f,64.f})for(float v:{0.f,32.f}) {
            const core::Vector2 signed_point{x?-u:u,y?v:-v};
            const auto expected=anchor+game::play::rotate_vector(signed_point,angle*std::numbers::pi_v<float>/180);
            const core::Vector2 inside{x?64-u:u,y?v:32-v};
            const auto actual=pivot+game::play::rotate_vector(command.command_rect.position()+inside-pivot,angle*std::numbers::pi_v<float>/180);
            require((actual-expected).length()<.001,"Mirrored/rotated wall corner differs from the native signed-scale transform");
        }
        const auto expected_flip=x?(y?core::SpriteFlip::Both:core::SpriteFlip::Horizontal):(y?core::SpriteFlip::Vertical:core::SpriteFlip::None);
        require(command.flip==expected_flip,"Decoration mirror flags were dropped");
        const core::Vector2 offset{17,-80};const auto shifted=game::play::decor_transform(d,offset);
        require((shifted.command_rect.position()-command.command_rect.position()-offset).length()<.001,"Parallax coordinate conversion changed the decoration anchor");
    }
    int mirrored=0,vertical=0;
    for(int level=1;level<=60;++level) {
        const auto data=game::content::level(level);
        game::play::Decor terrain(data,false),foreground(data,true);
        for(bool front:{false,true}) {
            const auto drawn=commands(front?foreground:terrain);std::size_t index=0;
            for(const auto& d:data.at("decor")) {
                const auto layer=d.at("layer").get<std::string>();
                if((layer=="Foreground")!=front||layer=="Farground"||layer=="Background")continue;
                require(index<drawn.size()&&drawn[index].texture==ELYSIA_RESOURCES->find_texture(d.at("key").get<std::string>()),"Decoration resource binding or layer order changed");
                if(d.value("flip",false))++mirrored;if(d.value("flip_y",false))++vertical;
                const auto& part=drawn[index++];
                const auto pivot=part.command_rect.position()+core::Vector2{part.rotation_origin.x*part.command_rect.width(),part.rotation_origin.y*part.command_rect.height()};
                const core::Vector2 expected{d.at("x"),d.at("y").get<float>()+d.at("h").get<float>()};
                require((pivot-expected).length()<.001,"Level wall image detached from its original placement anchor");
            }
            require(index==drawn.size(),"Decoration layer contains an unexpected wall image");
        }
    }
    require(mirrored==1002&&vertical==348,"All-level mirrored wall metadata was not imported");
}
void check_wind_visuals() {
    for(int kind:{8,24,25})for(int blocks:{1,2,4})for(const std::string fade:{"00","10","01","11"}) {
        auto data=mechanism(kind);data["rotation"]=0;data["info"]["NumBlocks"]=blocks;
        data["info"]["Strength"]=20;data["info"]["Fade"]=fade;
        game::play::Entity wind(data);
        const auto* atlas=ELYSIA_RESOURCES->find_atlas(data.at("key").get<std::string>());
        game::play::WindVisual visual(kind,data.at("info"),*atlas->frame_at(1),*atlas->frame_at(2),data.at("id"));
        const auto parts=[&](const game::play::WindVisual& value,core::Vector2 center,float angle=0.f){std::vector<core::RenderCommand> drawn;value.build_parts(drawn,center,angle);return drawn;};
        const int rows=kind==25?2:1,leaves=(blocks/2)*(kind==8?1:kind==24?2:3);
        const int flow_count=rows*(blocks+12*((fade[0]=='1')+(fade[1]=='1')));
        const auto initial=parts(visual,wind.center());bool scrolling=false,leaf_visible=leaves==0;
        require(wind.size().x==64*blocks&&wind.size().y==(kind==8?64:kind==24?128:256),"Wind visual changed its physical dimensions");
        for(int tick=0;tick<180;++tick) {
            const auto drawn=parts(visual,wind.center());
            require(drawn.size()==flow_count+leaves,"Wind block/fade/leaf count incorrect");
            for(std::size_t i=0;i<drawn.size();++i) {
                const auto& part=drawn[i];
                require(part.type==core::RenderCommandType::Texture&&part.texture==atlas->texture_at(i<flow_count?1:2),"Wind still uses placeholder lines or cycles component images");
                if(i<flow_count) {
                    require(part.alpha>0&&part.use_src_rect,"Wind flow window became invisible");
                    require(part.src_rect.x()>=0&&part.src_rect.x()+part.src_rect.width()<=768.001f&&part.src_rect.y()+part.src_rect.height()<=256.001f,"Wind flow sampled outside the source texture");
                    require(std::abs(part.src_rect.width()-2*part.command_rect.width())<.001&&std::abs(part.src_rect.height()-2*part.command_rect.height())<.001,"Wind flow texture was stretched instead of cropped");
                    scrolling|=part.src_rect!=initial[i].src_rect;
                } else leaf_visible|=part.alpha>0;
            }
            auto* renderer=SDL_GetRendererFromTexture(atlas->texture_at(1));
            auto* target=SDL_GetRenderTarget(renderer);Uint8 alpha_before=0,alpha_after=0;
            require(SDL_GetTextureAlphaMod(atlas->texture_at(1),&alpha_before),SDL_GetError());
            const auto composed=commands(wind);
            const bool software=std::string_view(SDL_GetRendererName(renderer))=="software";
            if(software)require(composed.size()==drawn.size()&&composed[0].texture==atlas->texture_at(1),"Software wind fallback lost its component textures");
            else require(composed.size()==1&&composed[0].texture!=atlas->texture_at(1)&&composed[0].texture!=atlas->texture_at(2),"Wind must compose its pieces before rotation to prevent tile cracks");
            require(SDL_GetRenderTarget(renderer)==target&&SDL_GetTextureAlphaMod(atlas->texture_at(1),&alpha_after)&&alpha_before==alpha_after,"Wind composition leaked renderer/texture state");
            if(!software)require(composed[0].texture->w==2*(64*blocks+96)&&composed[0].texture->h==2*(wind.size().y+96),"Wind canvas does not retain native texture resolution and padding");
            wind.update(1./60);visual.update(1./60);
        }
        require(scrolling&&leaf_visible,"Wind flow or leaves do not animate");
        // Only the local coordinates rotate; the texture window stays the same.
        game::play::Entity straight(data);data["rotation"]=90;game::play::Entity rotated(data);
        const auto a=parts(visual,straight.center()),b=parts(visual,rotated.center(),90);
        for(std::size_t i=0;i<a.size();++i) {
            const auto offset=a[i].command_rect.center()-straight.center();
            require((b[i].command_rect.center()-rotated.center()-core::Vector2{-offset.y,offset.x}).length()<.001&&a[i].src_rect==b[i].src_rect,"Wind components do not follow the field rotation");
        }
        if(fade=="11") {
            const auto left=initial[blocks*rows].alpha,right=initial[(blocks+11)*rows].alpha;
            require(left>right&&right>0,"Wind fade fringe has incorrect opacity ordering");
        }
    }
    auto data=mechanism(8);data["info"]["NumBlocks"]=2;data["info"]["Fade"]="00";data["rotation"]=0;
    data["info"]["Strength"]=10;game::play::Entity weak(data);
    data["info"]["Strength"]=20;game::play::Entity normal(data);
    const auto* atlas=ELYSIA_RESOURCES->find_atlas(data.at("key").get<std::string>());
    game::play::WindVisual weak_visual(8,weak.info,*atlas->frame_at(1),*atlas->frame_at(2),0),normal_visual(8,normal.info,*atlas->frame_at(1),*atlas->frame_at(2),0);
    weak_visual.update(.1);normal_visual.update(.1);
    std::vector<core::RenderCommand> weak_parts,normal_parts;
    weak_visual.build_parts(weak_parts,{} ,0);normal_visual.build_parts(normal_parts,{},0);
    require(weak_parts[0].alpha<normal_parts[0].alpha&&weak_parts[0].src_rect.x()<normal_parts[0].src_rect.x(),"Wind Strength does not affect flow opacity and scrolling rate");
}
void save_decor_preview(SDL_Renderer* renderer) {
    require(SDL_SetRenderDrawColor(renderer,42,81,116,255)&&SDL_RenderClear(renderer),SDL_GetError());
    const auto level=game::content::level(13);
    // Both panels use identical level data and cyan collision outlines.
    // Left reproduces the previous in-rectangle mirrors; right uses signed scale.
    for(int panel=0;panel<2;++panel) {
        const camera::Camera camera({440,2450},{640,720},1);
        const SDL_Rect clip{panel*640,0,640,720};require(SDL_SetRenderClipRect(renderer,&clip),SDL_GetError());
        const auto draw=[&](const core::RenderCommand& command) {
            auto projected=core::project_render_command_to_screen(command,camera);
            const core::Vector2 offset{panel*640.f,0};
            projected.screen_rect=projected.screen_rect.translated(offset);
            projected.line_start+=offset;projected.line_end+=offset;
            core::require_render_success(core::execute_render_command(renderer,projected));
        };
        for(const auto& d:level.at("decor")) {
            if(d.at("layer")!="Texture"&&d.at("layer")!="Foreground")continue;
            auto command=game::play::decor_transform(d);
            if(panel==0) {
                command.command_rect={d.at("x"),d.at("y"),d.at("w"),d.at("h")};command.rotation_origin={0,1};
                command.flip=d.value("flip",false)?core::SpriteFlip::Horizontal:core::SpriteFlip::None;
            }
            command.texture=ELYSIA_RESOURCES->find_texture(d.at("key").get<std::string>());draw(command);
        }
        for(const auto& edge:level.at("edges"))for(std::size_t i=1;i<edge.size();++i) {
            core::RenderCommand line;line.type=core::RenderCommandType::DrawLine;line.color={0,255,210,220};line.stroke_width=1;
            line.line_start={edge[i-1][0],edge[i-1][1]};line.line_end={edge[i][0],edge[i][1]};draw(line);
        }
    }
    require(SDL_SetRenderClipRect(renderer,nullptr),SDL_GetError());
    auto* captured=SDL_RenderReadPixels(renderer,nullptr);require(captured!=nullptr,SDL_GetError());
    std::filesystem::create_directories("out/qa");
    const bool saved=SDL_SaveBMP(captured,"out/qa/decor-anchor-before-after.bmp");SDL_DestroySurface(captured);require(saved,SDL_GetError());
}
void save_wind_preview(SDL_Renderer* renderer) {
    require(SDL_SetRenderDrawColor(renderer,16,25,38,255)&&SDL_RenderClear(renderer),SDL_GetError());
    for(int row=0;row<3;++row)for(int column=0;column<3;++column) {
        auto data=mechanism(row==0?8:row==1?24:25);
        data["info"]["NumBlocks"]=2;data["info"]["Fade"]=column==0?"00":"11";data["info"]["Strength"]=20;
        data["rotation"]=column==2?90:0;
        game::play::Entity wind(data);for(int frame=0;frame<69;++frame)wind.update(1./60);
        const camera::Camera camera(wind.position(),{1280,720},1.f);
        for(const auto& command:commands(wind)) {
            auto projected=core::project_render_command_to_screen(command,camera);
            projected.screen_rect=projected.screen_rect.translated(core::Vector2{220.f+420*column,row==0?100.f:row==1?300.f:560.f}-core::Vector2{640,360});
            core::require_render_success(core::execute_render_command(renderer,projected));
        }
    }
    SDL_Surface* captured=SDL_RenderReadPixels(renderer,nullptr);require(captured!=nullptr,SDL_GetError());
    std::filesystem::create_directories("out/qa");
    const bool saved=SDL_SaveBMP(captured,"out/qa/wind-visual-preview.bmp");SDL_DestroySurface(captured);require(saved,SDL_GetError());
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
    require(handle.is_valid(),"Bomb visual transition registration failed: type="+std::to_string(static_cast<int>(bomb.body_definition().type))+", mass="+std::to_string(bomb.body_definition().mass_properties.index())+", destroyed="+std::to_string(bomb.is_destroyed()));bomb.bind_physics(world,handle);
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
            check_role_effects(renderer);
            check_moon_highlight(renderer);
            check_decoration_anchors();
            save_decor_preview(renderer);
            check_platforms_and_rods();
            check_tracking_flower();
            check_thrower_head(renderer);
            check_portals();
            check_wind_visuals();
            save_wind_preview(renderer);
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
