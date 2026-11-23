#pragma once
#include <map>
// Included after SceneTestAccess in visual_tests.cpp; uses the real scene lookup
// and overlap handler. Fixture positioning is separate from input-only replays.
namespace game::scene {
class PortalTestAccess {
    static void check(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);}
    static play::Entity& portal(FlightScene& scene,int id){
        for(auto* entity:scene._entities)if(!entity->item&&entity->kind==115&&entity->id==id)return *entity;
        throw std::runtime_error("Portal fixture absent: "+std::to_string(id));
    }
    static void ready(FlightScene& scene){
        scene._opening_phase=FlightScene::OpeningPhase::Ready;scene._camera_move={};
        scene._follow_role=true;scene._input_enabled=true;scene._event_pending=false;
        scene._player->present_at(std::nullopt);scene._player->play_effect(play::RoleEffect::None);scene.resume();
    }
public:
    static content::Json links(FlightScene& scene){
        content::Json rows=content::Json::array();
        for(auto* source:scene._entities)if(source->kind==115&&source->info.contains("TargetPortalID")){
            const int id=static_cast<int>(play::parameter(source->info,"TargetPortalID"));
            auto* destination=scene.find_portal_destination(id);
            check(destination&&destination->id==id,std::format("Level {} portal {} expected object {}, got {}",scene._level,source->id,id,destination?destination->id:-1));
            check(!destination->info.contains("TargetPortalID"),"Portal destination must be an exit");
            rows.push_back({{"source",source->id},{"target",destination->id}});
        }
        check(scene.find_portal_destination(-1)==nullptr,"Missing portal must not resolve to another object");
        for(auto* entity:scene._entities)if(!entity->item&&entity->kind!=115){
            check(scene.find_portal_destination(entity->id)==nullptr,"A non-portal ID must not be accepted as a portal");break;
        }
        if(scene._level==48){
            const std::map<int,int> expected{{1,16},{3,16},{4,16},{10,22},{11,19},{13,2},{14,5},{20,16},{21,16}};
            check(rows.size()==expected.size(),"Level 48 entrance inventory changed");
            for(const auto& row:rows)check(expected.at(row.at("source").get<int>())==row.at("target").get<int>(),"Level 48 connection snapshot changed");
            check(portal(scene,22).info.at("PortalID")=="5","Original unused PortalID attribute must remain intact");
        }
        return rows;
    }
    static void transit(FlightScene& scene,SDL_Renderer* renderer){
        using Access=elysia::scene::SceneTestAccess;
        ready(scene);
        auto& source=portal(scene,14);auto& destination=portal(scene,5);
        const play::Vec velocity{37,-41};
        auto& world=scene.physics_world();const auto body=scene._player->physics_handle();
        const auto place=[&](play::Vec center){check(world.teleport_object(body,center+play::Vec{0,13},elysia::physics::TeleportVelocityMode::Clear),"Portal fixture teleport failed");};
        const auto capture=[&](const char* suffix){
            SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
            Access::render(scene,renderer);
            auto* surface=SDL_RenderReadPixels(renderer,nullptr);check(surface!=nullptr,SDL_GetError());
            const bool saved=SDL_SaveBMP(surface,(std::string("out/qa/level48-portal-")+suffix+".bmp").c_str());
            SDL_DestroySurface(surface);check(saved,SDL_GetError());
        };
        const auto snap=[&]{
            const auto slot=elysia::camera::CameraSlot::Main;
            // Standalone fixtures do not have the Application's viewport setup.
            // Center captures immediately; runtime snap requests are frame queued.
            elysia::camera::CameraManager::instance()->set_viewport_size(slot,{1280,720});
            scene.camera_runtime().set_center(slot,scene._player->center());
            scene.physics_world().finalize_frame(0);
        };
        place(source.center()+play::Vec{0,60});Access::update(scene,0);
        check(!scene._transition.active(),"Fixture started outside the entrance's trigger radius");
        place(source.center());check(world.set_velocity(body,velocity),"Portal fixture velocity failed");
        snap();capture("before");
        scene.on_after_update(0);
        check(scene._transition.active()&&scene._portal_lock==&destination,"Actual overlap did not select level 48 exit 5");
        check(scene._transition.destination==destination.center(),"Transit destination differs from selected portal");
        check(!scene._player->physics_state()->enabled&&scene._player->thrust.length_squared()==0,"Transit did not disable the role body and thrust");
        Access::update(scene,.1);const auto frozen=scene._transition.time;
        scene.open_pause_menu();check(scene._pause_menu_open&&scene.is_paused(),"Pause was unavailable during transit");
        Access::update(scene,.2);check(scene._transition.time==frozen,"Pause advanced the portal transition");capture("paused");
        scene.close_pause_menu();check(!scene.is_paused(),"Closing pause did not restore transit updates");
        bool travel=false,emerge=false;
        for(int tick=0;tick<600&&scene._transition.active();++tick){
            Access::update(scene,1./60.);
            if(scene._transition.phase()==play::RoleTransitionPhase::PortalTravel&&!travel){travel=true;capture("travel");}
            if(scene._transition.phase()==play::RoleTransitionPhase::PortalEmerge&&!emerge){emerge=true;capture("emerge");}
        }
        check(travel&&emerge&&!scene._transition.active(),"Transit phases did not complete");
        check(scene._portal_transits==1&&scene._player->physics_state()->enabled,"Transit did not restore physics exactly once");
        check((scene._player->center()-destination.center()).length()<.01f,"Role did not emerge at exit 5");
        check((scene._player->velocity()-velocity).length()<.01f,"Transit changed the restored velocity");
        snap();capture("after");
        scene.on_after_update(0);check(scene._portal_transits==1&&!scene._transition.active(),"Standing at exit triggered another transit");
        place(destination.center()+play::Vec{75,0});scene.on_after_update(0);
        check(scene._portal_lock==nullptr,"Portal lock did not clear after leaving the exit");
        // Missing targets still fail through the actual overlap handler.
        const auto saved=source.info.at("TargetPortalID");source.info["TargetPortalID"]="-1";place(source.center());
        bool rejected=false;try{scene.on_after_update(0);}catch(const std::runtime_error& error){rejected=std::string_view(error.what())=="Portal destination absent";}
        source.info["TargetPortalID"]=saved;check(rejected,"Missing portal target no longer reports the existing error");
        scene.on_after_update(0);check(scene._transition.active(),"Second overlap did not begin a transit");
        struct Requests final:elysia::scene::SceneRequestObserver{
            std::vector<elysia::scene::SceneRequest> rows;
            void on_scene_request(const elysia::scene::SceneRequest& request)override{rows.push_back(request);}
        } requests;
        scene.attach(&requests);scene.open_pause_menu();scene.retry_level();scene.detach(&requests);
        check(requests.rows.size()==1&&requests.rows[0].route.target==scene_keys::Flight,"Retry did not request FlightScene");
        const auto* retry=elysia::scene::try_scene_payload<FlightPayload>(requests.rows[0].route.payload);
        check(retry&&retry->level==48&&retry->restart,"Retry lost its level or restart payload");
        const auto payload=requests.rows[0].route.payload;
        Access::exit(scene);Access::enter(scene,payload);
        check(!scene._transition.active()&&!scene._portal_lock&&!scene._portal_emerging&&scene._portal_transits==0&&!scene._pause_menu_open,"Retry retained portal or pause state");
        ready(scene);
        check(scene.physics_world().teleport_object(scene._player->physics_handle(),portal(scene,14).center()+play::Vec{0,13},elysia::physics::TeleportVelocityMode::Clear),"Retry fixture teleport failed");
        scene.on_after_update(0);
        check(scene._portal_lock==&portal(scene,5),"Retry changed the corrected connection");
    }
};
}
namespace game::testing {
inline void check_portal_links(SDL_Renderer* renderer,const elysia::io::ContentRegistry& registry){
    const auto directory=std::filesystem::path("out/qa/portal-links")/std::to_string(SDL_GetTicksNS());
    std::filesystem::create_directories(directory);
    if(!ELYSIA_SAVE->initialize(directory))throw std::runtime_error("Portal save fixture failed");
    if(!ELYSIA_AUDIO->initialize({}))throw std::runtime_error("Portal audio fixture failed");
    const auto previous=game::progress::Store::instance().data;
    elysia::scene::SceneRuntimeContext context(renderer,registry,1280,720);
    content::Json rows=content::Json::array();
    // Check the known collision first, so the old implementation fails clearly.
    for(int index=0;index<60;++index){
        const int level=index==0?48:index<48?index:index+1;
        game::scene::FlightScene flight;elysia::scene::SceneTestAccess::bind(flight,context);
        elysia::scene::SceneTestAccess::enter(flight,game::scene::FlightPayload{level});
        rows.push_back({{"level",level},{"links",game::scene::PortalTestAccess::links(flight)}});
        if(level==48)game::scene::PortalTestAccess::transit(flight,renderer);
        elysia::scene::SceneTestAccess::exit(flight);
    }
    game::progress::Store::instance().data=previous;ELYSIA_AUDIO->shutdown();ELYSIA_SAVE->shutdown();
    std::ofstream("out/qa/portal-scene-regression.json")<<content::Json{{"passed",true},{"levels",rows},{"runtime","level 48 actual overlap, pull/travel/emerge, velocity, pause/resume, exit lock, missing target and retry"}}.dump(2);
    std::cout<<"PASS: 60 real scene portal lookups; level 48 overlap, transit, pause, velocity and retry\n";
}
}
