#pragma once
#include <type_traits>

namespace game::testing {
inline void burst_check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
inline auto burst_commands(const play::ProjectileBurst& burst){
    std::vector<elysia::core::RenderCommand> result;burst.submit_render_commands(result);return result;
}
inline void check_burst_frames(SDL_Renderer* renderer){
    using namespace elysia;
    static_assert(!std::is_base_of_v<physics::PhysicsParticipant,play::ProjectileBurst>);
    const auto* atlas=ELYSIA_RESOURCES->find_atlas("fly.DynamicObjectsLite.DynamicObject_112.S02");
    burst_check(atlas&&atlas->size()==16,"Ordinary burst atlas must have all 16 original frames");
    play::ProjectileBurst burst({240,360});
    for(int frame=0;frame<16;++frame){
        if(frame)burst.advance(1./60.);
        const auto commands=burst_commands(burst);const auto* expected=atlas->frame_at(frame);
        burst_check(!burst.finished()&&commands.size()==1,"Burst ended before its last frame");
        const auto& command=commands.front();
        burst_check(command.texture==expected->_texture&&command.use_src_rect==expected->_source_rect.has_value(),"Burst frame does not follow the native 60 Hz sequence");
        burst_check(!expected->_source_rect||command.src_rect==*expected->_source_rect,"Burst atlas source rectangle is incorrect");
        burst_check(command.command_rect.center()==core::Vector2{240,360},"Burst drifted away from its impact center");
        burst_check(command.command_rect.width()==expected->_width*.5f&&command.command_rect.height()==expected->_height*.5f,"HD debris frame was squeezed into the flying sprite's size");
        burst.advance(-1);burst_check(burst_commands(burst).front().command_rect==command.command_rect,"Negative delta rewound the burst");
    }
    burst.advance(1./60.);burst_check(burst.finished()&&burst_commands(burst).empty(),"Burst did not finish after 16/60 seconds");
    burst.advance(10);burst_check(burst_commands(burst).empty(),"Finished burst looped");

    // A fast swept collision must place debris at the hit, not the old body position.
    physics::PhysicsWorld world({.gravity={0,0}});
    core::GameObject wall(core::DepthLayer::Terrain);wall.set_world_rect({100,0,1,1});
    physics::Collider shape;shape.shape=physics::BoxShape::from_rect({0,-100,2,200});shape.filter={.category=2,.mask=8};
    const auto terrain=world.register_object(wall,physics::BodyDefinition{.type=physics::BodyType::Static},std::span(&shape,1));
    play::Player player({10000,10000});play::Projectile shot({0,0},{1000,0},false,1,6);
    const auto handle=world.register_object(shot,shot.body_definition(),shot.collider_definitions());shot.bind_physics(world,handle);
    burst_check(!shot.advance(.2,player.position(),player)&&shot.expired,"Terrain collision changed its damage result");
    burst_check(shot.burst_position()&&std::abs(shot.burst_position()->x-93)<1,"Debris did not use the swept impact position");
    const auto impact=*shot.burst_position();
    burst_check(!shot.advance(1,player.position(),player)&&shot.burst_position()==impact,"Expired projectile repeated or moved its hit");
    shot.unbind_physics();burst_check(world.unregister_object(handle)&&world.unregister_object(terrain),"Burst sweep fixture cleanup failed");
    play::Projectile timeout({0,0},{},false,1,6);const auto timeout_body=world.register_object(timeout,timeout.body_definition(),timeout.collider_definitions());timeout.bind_physics(world,timeout_body);
    burst_check(!timeout.advance(12.001,player.position(),player)&&timeout.expired&&timeout.burst_position().has_value(),"Ordinary timeout changed lifetime or omitted its burst");
    timeout.unbind_physics();burst_check(world.unregister_object(timeout_body),"Burst timeout fixture cleanup failed");
    play::Projectile homing({0,0},{},true,0,-1);const auto homing_body=world.register_object(homing,homing.body_definition(),homing.collider_definitions());homing.bind_physics(world,homing_body);
    homing.advance(32,player.position(),player);burst_check(homing.expired&&!homing.burst_position(),"Ordinary debris was incorrectly attached to a homing spore");
    homing.unbind_physics();burst_check(world.unregister_object(homing_body),"Homing burst fixture cleanup failed");

    SDL_SetRenderDrawColor(renderer,24,36,54,255);SDL_RenderClear(renderer);
    const camera::Camera preview({640,360},{1280,720});
    for(int column=0;column<8;++column){
        play::ProjectileBurst sample({80.f+160*column,360});sample.advance(column*2./60.);
        for(auto command:burst_commands(sample)){
            command.command_rect=core::Rect::from_center(command.command_rect.center(),{command.command_rect.width()*3,command.command_rect.height()*3});
            core::require_render_success(core::execute_render_command(renderer,core::project_render_command_to_screen(command,preview)));
        }
    }
    auto* surface=SDL_RenderReadPixels(renderer,nullptr);burst_check(surface!=nullptr,SDL_GetError());
    const bool saved=SDL_SaveBMP(surface,"out/qa/ordinary-spore-burst-preview.bmp");SDL_DestroySurface(surface);burst_check(saved,SDL_GetError());
}
}
namespace game::scene {
class ProjectileTestAccess {
    static int sounds(const FlightScene& scene){const auto& events=scene._audio.events();const auto at=events.find("34-1");return at==events.end()?0:at->second;}
public:
    static void run(FlightScene& scene){
        using game::testing::burst_check;using Access=elysia::scene::SceneTestAccess;
        scene._opening_phase=FlightScene::OpeningPhase::Ready;scene._camera_move={};scene._follow_role=true;scene._input_enabled=true;
        scene._player->present_at(std::nullopt);scene._player->play_effect(play::RoleEffect::None);scene.resume();
        const auto origin=scene._player->position();
        scene.create_and_add_object<play::Edge>(origin+play::Vec{80,60},origin+play::Vec{200,60});
        auto* shot=scene.create_and_add_object<play::Projectile>(origin+play::Vec{140,0},play::Vec{},false,1.f,6.f);
        scene._projectiles.push_back(shot);
        for(int tick=0;tick<90&&scene._projectile_bursts.empty();++tick)Access::update(scene,1./60.);
        burst_check(scene._projectile_bursts.size()==1&&scene._projectiles.empty(),"Real scene did not replace the expired ordinary shot with debris");
        burst_check(sounds(scene)==1,"Impact sound did not play exactly once at burst creation");
        Access::update(scene,2./60.);const auto frozen=game::testing::burst_commands(*scene._projectile_bursts.front());
        scene.pause();Access::update(scene,.1);
        const auto scripted_pause=game::testing::burst_commands(*scene._projectile_bursts.front());
        burst_check(scripted_pause.front().texture==frozen.front().texture&&scripted_pause.front().src_rect==frozen.front().src_rect,"Scripted logic pause advanced debris");
        scene.resume();
        scene.open_pause_menu();burst_check(scene._pause_menu_open,"Burst pause fixture could not open pause");
        Access::update(scene,.2);const auto paused=game::testing::burst_commands(*scene._projectile_bursts.front());
        burst_check(paused.front().texture==frozen.front().texture&&paused.front().src_rect==frozen.front().src_rect,"Modal pause advanced the debris");
        scene.close_pause_menu();for(int tick=0;tick<30&&!scene._projectile_bursts.empty();++tick)Access::update(scene,1./60.);
        burst_check(scene._projectile_bursts.empty()&&sounds(scene)==1,"Burst cleanup leaked an effect or replayed its sound");

        // Impact still damages the role once; debris finishes during death's physics pause.
        shot=scene.create_and_add_object<play::Projectile>(scene._player->position()+play::Vec{0,-10},play::Vec{},false,1.f,6.f);scene._projectiles.push_back(shot);
        Access::update(scene,1./60.);
        burst_check(scene._death_pending&&scene.is_paused()&&scene._projectile_bursts.size()==1&&scene._deaths==1&&sounds(scene)==2,"Role impact lost damage, burst, or one-shot audio");
        Access::update(scene,.3);
        burst_check(scene._projectile_bursts.empty()&&scene._deaths==1&&sounds(scene)==2,"Death pause froze debris or repeated the impact");
        scene.restore_player();
        shot=scene.create_and_add_object<play::Projectile>(origin+play::Vec{140,0},play::Vec{},false,1.f,6.f);scene._projectiles.push_back(shot);
        for(int tick=0;tick<90&&scene._projectile_bursts.empty();++tick)Access::update(scene,1./60.);
        burst_check(!scene._projectile_bursts.empty(),"Restore cleanup fixture did not create debris");
        scene.restore_player();burst_check(scene._projectile_bursts.empty()&&scene._projectiles.empty(),"Scripted restore retained debris");
        // Scene recreation is the retry path; it must also clear live debris.
        scene._projectile_bursts.push_back(scene.create_and_add_object<play::ProjectileBurst>(origin));
        Access::exit(scene);Access::enter(scene,FlightPayload{1,true});
        burst_check(scene._projectile_bursts.empty()&&scene._projectiles.empty(),"Retry retained projectile presentation state");
    }
};
}
namespace game::testing {
inline void check_projectile_bursts(SDL_Renderer* renderer,const elysia::io::ContentRegistry& registry){
    std::filesystem::create_directories("out/qa");check_burst_frames(renderer);
    const auto directory=std::filesystem::path("out/qa/projectile-bursts")/std::to_string(SDL_GetTicksNS());
    burst_check(ELYSIA_SAVE->initialize(directory).has_value(),"Burst save fixture failed");
    burst_check(ELYSIA_AUDIO->initialize({}),"Burst audio fixture failed");
    const auto previous=game::progress::Store::instance().data;
    elysia::scene::SceneRuntimeContext context(renderer,registry,1280,720);game::scene::FlightScene scene;
    elysia::scene::SceneTestAccess::bind(scene,context);elysia::scene::SceneTestAccess::enter(scene,game::scene::FlightPayload{1});
    game::scene::ProjectileTestAccess::run(scene);elysia::scene::SceneTestAccess::exit(scene);
    game::progress::Store::instance().data=previous;ELYSIA_AUDIO->shutdown();ELYSIA_SAVE->shutdown();
    std::cout<<"PASS: ordinary burst 16 frames/60 Hz, native extents, swept impact, damage/audio once, modal pause, death pause, restore/retry; flight gravity and homing unchanged\n";
}
}
