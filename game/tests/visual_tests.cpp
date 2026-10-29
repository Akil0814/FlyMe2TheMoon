#include "game/gameplay/flight_objects.h"
#include "game/gameplay/role_geometry.h"
#include "game/gameplay/projectile.h"
#include "game/gameplay/seesaw_parameters.h"
#include "game/gameplay/launcher_visual.h"
#include "game/gameplay/decor_transform.h"
#include "game/gameplay/laser_visual.h"
#include "game/gameplay/homing_parameters.h"
#include "game/frontend/moon_ui.h"
#include "game/frontend/pickup_effects.h"
#include "game/frontend/tutorial_ui.h"
#include "game/frontend/story_text.h"
#include "game/scene/main_menu_scene.h"
#include "game/scene/selection_scene.h"
#include "game/scene/settings_scene.h"
#include "game/scene/credits_scene.h"
#include "game/scene/flight_scene.h"
#include "game/tests/original_music_fixture.h"
#include "engine/audio/audio_service.h"
#include "engine/config/user_config_service.h"
#include "engine/input/input_system.h"
#include "engine/save/save_service.h"
#include "engine/ui/window/ui_window.h"
#include "engine/io/path/path_manager.h"
#include "engine/io/loaders/content_registry_loader.h"
#include "engine/loading/game_content_loader.h"
#include "engine/resources/resource_service.h"
#include "engine/core/render/render_command_projection.h"
#include "engine/core/render/sdl_render_command_executor.h"
#include "engine/builtin/resources/builtin_resources.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/localization/localization_manager.h"
#include "engine/localization/localization_service.h"
#include "engine/typography/font_resolver.h"
#include "engine/ui/widgets/image/ui_image.h"
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <stdexcept>
#include <set>
#include <regex>
#include <fstream>

namespace elysia::scene {
class SceneTestAccess {
public:
    static void bind(Scene& scene,const SceneRuntimeContext& context){scene.bind_runtime_context(context);}
    static void enter(Scene& scene,const ScenePayload& payload={}){scene.lifecycle_enter(payload);}
    static void exit(Scene& scene){scene.lifecycle_exit();}
    static void update(Scene& scene,double delta){scene.lifecycle_update(delta);}
    static void input(Scene& scene,const input::InputSnapshot& snapshot){scene.lifecycle_input(snapshot);}
    static ui::UiWindow& window(Scene& scene){return dynamic_cast<ui::UiWindow&>(*scene._ui_roots.at(0));}
};
}

#include "game/progress/progress.h"
#include "game/tests/audio_scene_checks.h"

namespace {
using namespace elysia;
void require(bool condition,const std::string& message) {
    if(!condition)throw std::runtime_error(message);
}
void check_level_music_playback(SDL_Renderer* renderer,const io::ContentRegistry& registry){
    const auto directory=std::filesystem::path("out/qa/level-music")/std::to_string(SDL_GetTicksNS());
    require(ELYSIA_SAVE->initialize(directory).has_value(),"Isolated level music save initialization failed");
    require(ELYSIA_AUDIO->initialize({}),"Level music audio initialization failed");
    scene::SceneRuntimeContext context(renderer,registry,1280,720);
    auto* track=audio::detail::mixer_backend().music;
    game::content::Json rows=game::content::Json::array();bool passed=true;
    for(int id=1;id<=60;++id){
        const auto& expected=game::testing::expected_level_music(id);
        game::content::Json row{{"level",id},{"expected",expected},{"passed",false}};
        try{
            game::scene::FlightScene flight;scene::SceneTestAccess::bind(flight,context);
            scene::SceneTestAccess::enter(flight,game::scene::FlightPayload{id});
            const auto deadline=SDL_GetTicks()+500;
            while(MIX_TrackPlaying(track)&&MIX_GetTrackPlaybackPosition(track)<=0&&SDL_GetTicks()<deadline)SDL_Delay(5);
            std::string actual;
            for(const auto& [music,levels]:game::testing::original_level_music)
                if(MIX_GetTrackAudio(track)==ELYSIA_RESOURCES->find_music(music))actual=music;
            const bool playing=MIX_TrackPlaying(track);
            const auto frames=MIX_GetTrackPlaybackPosition(track);
            row["suite"]=game::content::level(id).at("suite");row["actual"]=actual;
            row["playing"]=playing;row["playback_frames"]=frames;
            scene::SceneTestAccess::exit(flight);
            row["stopped_on_exit"]=!MIX_TrackPlaying(track);
            row["passed"]=actual==expected&&playing&&frames>0&&!MIX_TrackPlaying(track);
        }catch(const std::exception& error){row["error"]=error.what();ELYSIA_AUDIO->stop_music();}
        passed&=row.at("passed").get<bool>();rows.push_back(std::move(row));
    }
    ELYSIA_AUDIO->shutdown();ELYSIA_SAVE->shutdown();
    std::ofstream report("out/qa/level-music-playback.json");
    report<<game::content::Json{{"passed",passed},{"scope","60 real FlightScene entries with SDL dummy audio; track identity, advancing playback and stop on exit"},{"levels",rows}}.dump(2);
    report.close();require(report.good(),"Could not save level music playback evidence");
    require(passed,"Some level music playback checks failed; see out/qa/level-music-playback.json");
    std::cout<<"PASS: all 60 real FlightScene music tracks match the original snapshot, advance playback and stop on exit\n";
}
void check_frontend_music(SDL_Renderer* renderer,const io::ContentRegistry& registry){
    const auto save_directory=std::filesystem::path("out/qa/menu-music")/
        std::to_string(SDL_GetTicksNS());
    require(ELYSIA_SAVE->initialize(save_directory).has_value(),"Isolated menu music save initialization failed");
    require(ELYSIA_AUDIO->initialize({}),"Menu music audio initialization failed");
    scene::SceneRuntimeContext context(renderer,registry,1280,720);
    game::scene::MainMenuScene menu;
    game::scene::SelectionScene selection;
    game::scene::AchievementsScene achievements;
    game::scene::SettingsScene settings;
    game::scene::CreditsScene credits;
    auto* track=audio::detail::mixer_backend().music;
    auto* sky_tunnel=ELYSIA_RESOURCES->find_music("fly.SkyTunnel");
    auto* moon_trip=ELYSIA_RESOURCES->find_music("fly.MoonTrip");
    require(sky_tunnel&&moon_trip,"Menu/credits music resource missing");
    const auto check_playing=[&]{
        require(MIX_TrackPlaying(track)&&MIX_GetTrackAudio(track)==sky_tunnel,"Main menu, settings, achievements and selection must play SkyTunnel");
    };
    const auto check_credits=[&]{
        require(MIX_TrackPlaying(track)&&MIX_GetTrackAudio(track)==moon_trip,"Credits must retain MoonTrip");
    };
    constexpr Sint64 position=240000;
    const auto check_position=[&]{
        check_playing();
        require(MIX_GetTrackPlaybackPosition(track)>=position,"Menu scene transition must preserve music position");
    };
    const auto enter=[&](scene::Scene& current,const scene::ScenePayload& payload={}){
        scene::SceneTestAccess::bind(current,context);scene::SceneTestAccess::enter(current,payload);
    };
    enter(menu);check_playing();
    require(MIX_SetTrackPlaybackPosition(track,position),SDL_GetError());
    scene::SceneTestAccess::exit(menu);check_position();
    const auto visit=[&](scene::Scene& current,const scene::ScenePayload& payload={}){
        enter(current,payload);check_position();
        scene::SceneTestAccess::exit(current);check_position();
        enter(menu);check_position();
        scene::SceneTestAccess::exit(menu);check_position();
    };
    visit(settings);
    for(int chapter=0;chapter<4;++chapter)visit(selection,game::scene::SelectionPayload{chapter});
    for(int page=0;page<3;++page)visit(achievements,game::scene::AchievementsPayload{page});
    ELYSIA_AUDIO->set_music_volume(0);check_position();
    ELYSIA_AUDIO->set_music_volume(100);check_position();
    enter(credits);check_credits();
    require(MIX_SetTrackPlaybackPosition(track,position),SDL_GetError());
    scene::SceneTestAccess::exit(credits);check_credits();
    enter(credits);check_credits();
    require(MIX_GetTrackPlaybackPosition(track)>=position,"Reentering credits must not restart MoonTrip");
    scene::SceneTestAccess::exit(credits);
    enter(menu);check_playing();scene::SceneTestAccess::exit(menu);
    // Each scene must also recover the menu track after gameplay or stopped audio.
    for(auto* current:std::array<scene::Scene*,4>{&menu,&settings,&achievements,&selection}){
        ELYSIA_AUDIO->stop_music();enter(*current);check_playing();
        scene::SceneTestAccess::exit(*current);
        require(ELYSIA_AUDIO->play_music("fly.Hectic"),"Gameplay music start failed");
        enter(*current);check_playing();scene::SceneTestAccess::exit(*current);
    }
    ELYSIA_AUDIO->stop_music();enter(credits);check_credits();scene::SceneTestAccess::exit(credits);
    require(ELYSIA_AUDIO->play_music("fly.Hectic"),"Gameplay music start failed");
    enter(credits);check_credits();scene::SceneTestAccess::exit(credits);
    enter(menu);check_playing();scene::SceneTestAccess::exit(menu);
    for(const auto* music:{"fly.LifeforSpeed","fly.Hectic","fly.SpiralofAdventure","fly.SymphonyofRush"}){
        auto* audio=ELYSIA_RESOURCES->find_music(music);
        require(audio&&ELYSIA_AUDIO->play_music(music),std::string("Original level music failed to start: ")+music);
        require(MIX_TrackPlaying(track)&&MIX_GetTrackAudio(track)==audio,"Mixer must play the requested original level music");
    }
    ELYSIA_AUDIO->shutdown();ELYSIA_SAVE->shutdown();
    std::cout<<"PASS: SkyTunnel survives menu/settings/selection/achievement transitions, every page and mute; credits retain MoonTrip; cold/gameplay returns restore each scene's track\n";
}
void check_original_tutorial_ui(SDL_Renderer* renderer){
    using namespace game::frontend;
    StoryText text({212,492,1024,144});text.set_padding(8);text.set_style_overrides({.text=core::Color{16,46,77,255}});
    text.set_story_text("A#cffc600中#nB");
    require(text.glyph_count()==3&&text.text_content().value=="A中B"&&!text.complete(),"Story parsing must retain UTF-8 glyphs and consume color controls without displaying them");
    text.advance(1./60.);std::vector<core::UiRenderCommand> partial;text.submit_ui_render_commands(partial);
    require(partial.size()==1&&text.visible_count()==1,"Typewriter must reveal whole Unicode glyphs at the original 60 Hz cadence");
    text.reveal();std::vector<core::UiRenderCommand> full;text.submit_ui_render_commands(full);
    require(full.size()==3&&full[0].screen_rect==partial[0].screen_rect,"Completing a line must preserve its existing glyph positions");
    localization::LocalizedTextStyle gold{.typography_role=typography::UiTypographyRole::DialogBody,.color={255,198,0,255}};
    require(full[1].texture==ELYSIA_LOCALIZATION->get_raw_text_texture("中",gold),"Original color-coded teaching text lost its highlight");
    text.set_story_text("\xF0\x9F\x8C\x99");require(text.glyph_count()==1&&!text.complete(),"Four-byte UTF-8 must be revealed as one glyph");
    TutorialUi tutorial;
    const auto texture=[](const char* key){return ELYSIA_RESOURCES->find_texture(std::string("fly.TutorialUI.")+key);};
    const auto contains=[](const auto& commands,SDL_Texture* expected){return std::any_of(commands.begin(),commands.end(),[&](const auto& c){return c.texture==expected;});};
    tutorial.set_type(1);std::vector<core::UiRenderCommand> up;tutorial.submit_ui_render_commands(up);
    require(contains(up,texture("tutBubble"))&&contains(up,texture("tutUp"))&&contains(up,texture("tutCharStand"))&&!contains(up,texture("tutArrow")),"First tutorial must begin with its bubble, raised finger and standing doll");
    tutorial.advance(1);std::vector<core::UiRenderCommand> down;tutorial.submit_ui_render_commands(down);
    require(contains(down,texture("tutDown"))&&contains(down,texture("tutCharUp"))&&contains(down,texture("tutArrow")),"First tutorial must show press, upward flight and the direction arrow together");
    tutorial.advance(1);require(tutorial.frame()==0,"First tutorial cycle must last two seconds");
    tutorial.set_type(2);const auto frames=game::content::read("tutorial_visual.json").at("direction_frames");
    constexpr const char* dolls[]{"tutCharStand","tutCharLeft","tutCharRight","tutCharFall","tutCharUp"};
    for(int index=0;index<80;++index){
        std::vector<core::UiRenderCommand> commands;tutorial.submit_ui_render_commands(commands);
        require(tutorial.frame()==index&&contains(commands,texture(dolls[frames[index].at("doll").get<int>()])),"Second tutorial doll does not match the original eighty-frame lookup");
        for(const auto& [name,key]:std::array<std::pair<const char*,const char*>,3>{{{"up","tutUp"},{"down","tutDown"},{"blank","tutBlank"}}}){
            const auto& point=frames[index].at(name);
            require(contains(commands,texture(key))==(point[0]!=0||point[1]!=0),"Second tutorial finger/mask visibility does not match its original frame");
        }
        tutorial.advance(.1);
    }
    require(tutorial.frame()==0,"Second tutorial cycle must last eight seconds");
    tutorial.set_type(0);tutorial.set_demo(true);tutorial.set_pointer_pressed(true);
    std::vector<core::UiRenderCommand> demo;tutorial.submit_ui_render_commands(demo);
    const auto language=game::content::language()=="zh-Hans"?std::string("zh"):game::content::language();
    require(contains(demo,ELYSIA_RESOURCES->find_texture("fly.InGameUI.fingerDown"))&&contains(demo,ELYSIA_RESOURCES->find_texture("fly.frontend.tutorial_demo_"+language)),"Automatic demonstration needs its recorded finger and original localized Demo indicator");
    const auto pressed_finger=*std::find_if(demo.begin(),demo.end(),[&](const auto& c){return c.texture==ELYSIA_RESOURCES->find_texture("fly.InGameUI.fingerDown");});
    tutorial.set_pointer_pressed(false);demo.clear();tutorial.submit_ui_render_commands(demo);
    require(contains(demo,ELYSIA_RESOURCES->find_texture("fly.InGameUI.fingerUp")),"Releasing recorded input must change the demonstration finger");
    const auto released_finger=*std::find_if(demo.begin(),demo.end(),[&](const auto& c){return c.texture==ELYSIA_RESOURCES->find_texture("fly.InGameUI.fingerUp");});
    require(pressed_finger.screen_rect==released_finger.screen_rect&&released_finger.screen_rect.bottom()<474,"Both demo finger states must share a fixed position above the dialogue");
    tutorial.set_demo(false);demo.clear();tutorial.submit_ui_render_commands(demo);require(demo.empty(),"Player handoff must remove all demonstration overlays");
    require(SDL_SetRenderDrawColor(renderer,12,20,35,255)&&SDL_RenderClear(renderer),SDL_GetError());
    const auto draw=[&](const auto& element){std::vector<core::UiRenderCommand> commands;element.submit_ui_render_commands(commands);for(const auto& command:commands)core::require_render_success(core::execute_render_command(renderer,command));};
    HudPanel panel({24,474,1232,182});draw(panel);
    tutorial.set_type(2);tutorial.advance(1);draw(tutorial);
    text.set_story_text(game::content::read("text/zh-Hans/strings.json").at("Level2-5"));text.reveal();draw(text);
    std::filesystem::create_directories("out/qa");auto* image=SDL_RenderReadPixels(renderer,nullptr);require(image!=nullptr,SDL_GetError());
    const bool saved=SDL_SaveBMP(image,"out/qa/tutorial-restoration-preview.bmp");SDL_DestroySurface(image);require(saved,SDL_GetError());
}
void check_run_stats(SDL_Renderer* renderer){
    using namespace game::frontend;
    auto* plate=ELYSIA_RESOURCES->find_texture("fly.InGameUI.btn1Up");
    const std::array<SDL_Texture*,3> icons{{ELYSIA_RESOURCES->find_texture("fly.InGameUI.timeIcon"),ELYSIA_RESOURCES->find_texture("fly.InGameUI.uiCrystalLarge"),ELYSIA_RESOURCES->find_texture("fly.InGameUI.uiCrystalSmall")}};
    RunStats stats({312,12,620,64});stats.set_values(123.4,12,37);
    require(stats.large_crystals()==12&&stats.small_crystals()==37,"Run stats must retain distinct large and small crystal counts");
    std::vector<core::UiRenderCommand> drawn;stats.submit_ui_render_commands(drawn);
    int plates=0;std::array<int,3> icon_counts{};std::array<core::Rect,3> icon_rects{};
    for(const auto& command:drawn){
        require(command.type==core::UiRenderCommandType::Texture,"Run stats must use original textures without extra lines or geometric backgrounds");
        if(command.texture==plate)++plates;
        for(int i=0;i<3;++i)if(command.texture==icons[i]){++icon_counts[i];icon_rects[i]=command.screen_rect;}
    }
    require(plates==9&&icon_counts==std::array<int,3>{1,1,1},"Time and both crystal counts must each use btn1Up and their own icon");
    require(icon_rects[0].x()<icon_rects[1].x()&&icon_rects[1].x()<icon_rects[2].x()&&icon_rects[2].height()<icon_rects[1].height(),"Time must precede the distinct large and small crystal slots");
    stats.set_values(0,0,0);require(stats.large_crystals()==0&&stats.small_crystals()==0,"Retry stats must clear both crystal counters");
    HudLabel label({16,12,280,64},0,ui::ui_raw_text(game::content::format("hud_stage",12)));
    std::vector<core::UiRenderCommand> label_commands;label.submit_ui_render_commands(label_commands);
    require(label_commands.size()>3&&label.style().text.r<40,"HUD stage text must render in a readable dark color over btn1Up");
    require(SDL_SetRenderDrawColor(renderer,12,20,35,255)&&SDL_RenderClear(renderer),SDL_GetError());
    const auto render=[&](const ui::UiElement& element){std::vector<core::UiRenderCommand> output;element.submit_ui_render_commands(output);for(const auto& command:output)core::require_render_success(core::execute_render_command(renderer,command));};
    stats.set_values(123.4,12,37);render(label);render(stats);
    StarRating stars({982,24,136,40});stars.set_collected(2);render(stars);
    PauseIconButton pause({1192,12,64,64},[]{});render(pause);
    // A settlement preview reuses the same metrics and keeps its stars unbacked.
    HudPanel panel({300,150,680,550});render(panel);
    ui::UiLabel title({340,174,600,48},0,ui::ui_raw_text(game::content::format("result_success",12)));title.set_horizontal_align(ui::TextHorizontalAlign::Center);title.set_style_overrides({.text=core::Color{16,46,77,255}});render(title);
    StarRating result_stars({532,234,216,64});result_stars.set_collected(2);render(result_stars);
    RunStats result({340,310,600,64});result.set_values(123.4,12,37);render(result);
    HudLabel score({340,386,600,40},0,ui::ui_raw_text(game::content::format("result_score",42500)));render(score);
    ui::UiLabel record({340,426,600,24},0,ui::ui_raw_text(game::content::ui("result_new_record")));record.set_horizontal_align(ui::TextHorizontalAlign::Center);record.set_style_overrides({.text=core::Color{120,65,8,255}});render(record);
    GlassButton next(game::content::ui("next_level"),440,48,[]{});next.set_screen_rect({420,450,440,48});render(next);
    GlassButton retry(game::content::ui("retry"),440,48,[]{});retry.set_screen_rect({420,510,440,48});render(retry);
    GlassButton select(game::content::ui("select_level"),440,48,[]{});select.set_screen_rect({420,570,440,48});render(select);
    std::filesystem::create_directories("out/qa");auto* preview=SDL_RenderReadPixels(renderer,nullptr);require(preview!=nullptr,SDL_GetError());
    const bool saved=SDL_SaveBMP(preview,"out/qa/hud-original-assets-preview.bmp");SDL_DestroySurface(preview);require(saved,SDL_GetError());
}
void check_dialogue_and_pause(SDL_Renderer* renderer){
    using namespace game::frontend;
    auto* plate=ELYSIA_RESOURCES->find_texture("fly.InGameUI.btn1Up");
    for(const auto size:{core::Vector2{1232,182},core::Vector2{156,156},core::Vector2{680,560}}){
        HudPanel panel({24,40,size.x,size.y});std::vector<core::UiRenderCommand> drawn;panel.submit_ui_render_commands(drawn);
        require(drawn.size()==9,"Dialogue, portrait and modal frames must use nine-slice panels");
        float area=0;for(const auto& command:drawn){
            require(command.type==core::UiRenderCommandType::Texture&&command.texture==plate&&command.use_src_rect,"Frames must use btn1Up without additional lines or geometric borders");
            area+=command.screen_rect.width()*command.screen_rect.height();
        }
        require(std::abs(area-size.x*size.y)<.1f,"Nine-slice panels must cover their entire target rectangle without overlap");
        panel.set_visible(false);drawn.clear();panel.submit_ui_render_commands(drawn);require(drawn.empty(),"Hidden dialogue panels must not render");
    }
    auto* up=ELYSIA_RESOURCES->find_texture("fly.InGameUI.pauseBtnUp");auto* down=ELYSIA_RESOURCES->find_texture("fly.InGameUI.pauseBtnDown");auto* text=ELYSIA_RESOURCES->find_texture("fly.InGameUI.pauseBtnText");
    int clicks=0;PauseIconButton button({1192,12,64,64},[&]{++clicks;});
    const auto check=[&](SDL_Texture* state){
        std::vector<core::UiRenderCommand> drawn;button.submit_ui_render_commands(drawn);
        require(drawn.size()==2&&drawn[0].texture==state&&drawn[1].texture==text,"Pause must compose exactly the selected state texture and pauseBtnText");
        require((drawn[0].screen_rect.center()-drawn[1].screen_rect.center()).length()<.001f,"Pause text must remain centered across button states");
    };
    check(up);
    ui::UiInputEvent pointer{.type=ui::UiInputEventType::PointerPressed,.device=input::InputDevice::Mouse,.control=input::RawInputControl::MouseLeft,.mouse_x=1228,.mouse_y=42};
    require(button.on_ui_input_event(pointer),"Pause must accept a mouse press");check(down);require(clicks==0,"Pause must activate on release");
    pointer.type=ui::UiInputEventType::PointerReleased;require(button.on_ui_input_event(pointer),"Pause must accept release");check(up);require(clicks==1,"Mouse click must activate once");
    pointer.type=ui::UiInputEventType::PointerPressed;button.on_ui_input_event(pointer);pointer.type=ui::UiInputEventType::PointerReleased;pointer.mouse_x=1100;button.on_ui_input_event(pointer);
    require(clicks==1,"Releasing outside the pause button must cancel activation");
    button.set_focused(true);ui::UiInputEvent confirm{.action=ui::UiAction::Confirm,.type=ui::UiInputEventType::ActionPressed,.device=input::InputDevice::Gamepad,.control=input::RawInputControl::GamepadSouth};
    require(button.on_ui_input_event(confirm),"Pause must accept gamepad confirmation");check(down);
    confirm.type=ui::UiInputEventType::ActionReleased;button.on_ui_input_event(confirm);check(up);require(clicks==2,"Gamepad confirmation must activate once");
    confirm.type=ui::UiInputEventType::ActionPressed;button.on_ui_input_event(confirm);button.cancel_input_interaction();check(up);
    confirm.type=ui::UiInputEventType::ActionReleased;button.on_ui_input_event(confirm);require(clicks==2,"Cancelled confirmation must not click");

    require(SDL_SetRenderDrawColor(renderer,12,20,35,255)&&SDL_RenderClear(renderer),SDL_GetError());
    const auto render=[&](const ui::UiElement& element){std::vector<core::UiRenderCommand> output;element.submit_ui_render_commands(output);for(const auto& command:output)core::require_render_success(core::execute_render_command(renderer,command));};
    HudLabel stage({16,12,280,64},0,ui::ui_raw_text(game::content::format("hud_stage",39)));render(stage);
    RunStats stats({312,12,620,64});stats.set_values(32.1,2,6);render(stats);StarRating stars({982,24,136,40});stars.set_collected(1);render(stars);render(button);
    HudLabel states({900,172,340,40},0,ui::ui_raw_text("Up / Down"));render(states);
    PauseIconButton normal({970,232,64,64},[]{}),pressed({1120,232,64,64},[]{});render(normal);
    pointer.type=ui::UiInputEventType::PointerPressed;pointer.mouse_x=1152;pointer.mouse_y=264;pressed.on_ui_input_event(pointer);render(pressed);
    HudPanel dialogue({24,474,1232,182}),portrait({38,486,156,156});render(dialogue);render(portrait);
    ui::UiImage face(ELYSIA_RESOURCES->find_texture("fly.ui.faceJingya"),core::Rect{46,494,140,140});render(face);
    ui::UiTextBlock line({212,492,1024,144});line.set_padding(8);line.set_style_overrides({.text=core::Color{16,46,77,255}});
    auto source=game::content::read("text/zh-Hans/strings.json").at("Level39-1").get<std::string>();source=std::regex_replace(source,std::regex("#c[0-9a-fA-F]{6}|#n"),"");line.set_text_content(ui::ui_raw_text(source));render(line);
    auto* preview=SDL_RenderReadPixels(renderer,nullptr);require(preview!=nullptr,SDL_GetError());const bool saved=SDL_SaveBMP(preview,"out/qa/dialogue-pause-assets-preview.bmp");SDL_DestroySurface(preview);require(saved,SDL_GetError());
}
void check_main_menu_background(SDL_Renderer* renderer,const io::ContentRegistry& registry){
    scene::SceneRuntimeContext context(renderer,registry,640,960);
    game::scene::MainMenuScene menu;scene::SceneTestAccess::bind(menu,context);scene::SceneTestAccess::enter(menu);
    auto& window=scene::SceneTestAccess::window(menu);window.update(0);
    std::vector<core::UiRenderCommand> drawn;window.submit_ui_render_commands(drawn);
    auto* expected=ELYSIA_RESOURCES->find_texture("fly.frontend.background_menu");require(expected!=nullptr,"Requested main-menu background missing");
    int backgrounds=0;
    for(const auto& command:drawn){
        if(command.texture==expected){++backgrounds;require(command.screen_rect==core::Rect{0,0,640,960},"Menu background must fill the portrait viewport");}
        // Pruned artwork resolves to nullptr; solid-color commands also have no texture.
        // Only texture commands can display an obsolete artwork layer.
        if(command.type==core::UiRenderCommandType::Texture)
            require(command.texture&&command.texture!=ELYSIA_RESOURCES->find_texture("fly.ui.mainBg2")&&command.texture!=ELYSIA_RESOURCES->find_texture("fly.ui.mainTitle2")&&command.texture!=ELYSIA_RESOURCES->find_texture("fly.frontend.background_menu_outpaint"),"New menu must not retain the old artwork layers");
    }
    require(backgrounds==1&&window.child_count()==3&&!window.child_at(2)->is_visible(),"Main menu must draw its background/navigation with the exit confirmation initially hidden");
    auto* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,640,960);
    require(target&&SDL_SetRenderTarget(renderer,target),SDL_GetError());
    require(SDL_SetRenderDrawColor(renderer,12,20,35,255)&&SDL_RenderClear(renderer),SDL_GetError());
    for(const auto& command:drawn)core::require_render_success(core::execute_render_command(renderer,command));
    auto* preview=SDL_RenderReadPixels(renderer,nullptr);require(preview!=nullptr,SDL_GetError());
    std::filesystem::create_directories("out/qa");const bool saved=SDL_SaveBMP(preview,"out/qa/main-menu-background-preview.bmp");SDL_DestroySurface(preview);require(saved,SDL_GetError());
    require(SDL_SetRenderTarget(renderer,nullptr),SDL_GetError());SDL_DestroyTexture(target);
    scene::SceneTestAccess::exit(menu);
}
void check_portrait_pages(SDL_Renderer* renderer,const io::ContentRegistry& registry){
    struct Requests final:scene::SceneRequestObserver {
        std::vector<scene::SceneRequest> received;
        void on_scene_request(const scene::SceneRequest& request)override{received.push_back(request);}
    };
    const auto directory=std::filesystem::path("out/qa/portrait-pages")/std::to_string(SDL_GetTicksNS());
    std::filesystem::create_directories(directory);
    config::UserConfigData defaults;defaults.window.windowed_size={640,960};defaults.language="zh-Hans";
    require(ELYSIA_USER_CONFIG->initialize(defaults,directory/"user_config.json").has_value(),"Portrait settings fixture config initialization failed");
    scene::SceneRuntimeContext context(renderer,registry,640,960);
    const auto click=[&](ui::UiWindow& window,const ui::UiElement& element){
        const auto point=element.screen_rect().center();
        ui::UiInputEvent event{.type=ui::UiInputEventType::PointerPressed,.device=input::InputDevice::Mouse,.control=input::RawInputControl::MouseLeft,.mouse_x=static_cast<int>(point.x),.mouse_y=static_cast<int>(point.y)};
        window.on_ui_input_event(event);window.on_ui_input_frame({});window.update(1./60.);
        event.type=ui::UiInputEventType::PointerReleased;window.on_ui_input_event(event);window.on_ui_input_frame({});window.update(1./60.);
    };
    const auto capture=[&](ui::UiWindow& window,const char* filename){
        auto* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,640,960);
        require(target&&SDL_SetRenderTarget(renderer,target),SDL_GetError());
        require(SDL_SetRenderDrawColor(renderer,12,20,35,255)&&SDL_RenderClear(renderer),SDL_GetError());
        std::vector<core::UiRenderCommand> drawn;window.submit_ui_render_commands(drawn);
        for(const auto& command:drawn)core::require_render_success(core::execute_render_command(renderer,command));
        auto* preview=SDL_RenderReadPixels(renderer,nullptr);require(preview!=nullptr,SDL_GetError());
        const bool saved=SDL_SaveBMP(preview,filename);SDL_DestroySurface(preview);require(saved,SDL_GetError());
        require(SDL_SetRenderTarget(renderer,nullptr),SDL_GetError());SDL_DestroyTexture(target);
    };
    game::scene::SettingsScene settings;Requests settings_requests;settings.attach(&settings_requests);
    scene::SceneTestAccess::bind(settings,context);scene::SceneTestAccess::enter(settings);
    auto& settings_window=scene::SceneTestAccess::window(settings);settings_window.update(0);
    auto& panel=dynamic_cast<ui::SettingsPanel&>(*settings_window.child_at(1));
    require(settings_window.screen_rect()==core::Rect{0,0,640,960}&&panel.screen_rect()==core::Rect{24,24,592,912},"Portrait settings panel must fit within 24 pixel margins");
    auto& actions=dynamic_cast<ui::UiListContainer&>(*panel.child_at(panel.child_count()-1));
    auto& back=*actions.child_at(0);auto& save=*actions.child_at(1);
    require(std::abs(actions.screen_rect().center().x-320)<.001f&&back.screen_rect().y()==save.screen_rect().y()&&back.screen_rect().right()<save.screen_rect().x()&&save.screen_rect().bottom()<=936,"Settings actions must be centered, horizontal and inside the viewport");
    bool has_window_field=false;
    const auto check_hidden=[&](auto&& self,ui::UiElement& element)->void{
        if(auto* host=dynamic_cast<ui::UiChildHost*>(&element))for(std::size_t i=0;i<host->child_count();++i){
            if(auto* label=dynamic_cast<ui::UiLabel*>(host->child_at(i));label&&(label->text_content().value=="engine.settings.fields.window_size"||label->text_content().value=="engine.settings.fields.window_mode"))has_window_field=true;
            self(self,*host->child_at(i));
        }
    };
    check_hidden(check_hidden,panel);require(!panel.visibility().window_mode&&!has_window_field,"Mobile settings must omit both window mode and window size controls");
    auto hidden_draft=panel.draft();hidden_draft.window_mode=ui::SettingsWindowMode::BorderlessFullscreen;hidden_draft.window_size={1920,1080};panel.set_draft(hidden_draft);
    click(settings_window,save);
    auto& status=dynamic_cast<ui::UiLabel&>(*panel.child_at(panel.child_count()-2));
    require(status.text_content().value=="engine.settings.status.saved"&&std::filesystem::is_regular_file(directory/"user_config.json"),"Portrait Save button must persist settings successfully");
    require(ELYSIA_USER_CONFIG->user_config().snapshot().window==defaults.window,"Saving mobile settings must preserve window mode and size even if hidden draft values change");
    capture(settings_window,"out/qa/settings-portrait-preview.bmp");
    click(settings_window,back);require(settings_requests.received.size()==1&&settings_requests.received.back().route.target==game::scene_keys::MainMenu,"Portrait settings Back button must return to the menu");
    scene::SceneTestAccess::exit(settings);ELYSIA_USER_CONFIG->shutdown();

    game::scene::CreditsScene credits;Requests credits_requests;credits.attach(&credits_requests);
    scene::SceneTestAccess::bind(credits,context);scene::SceneTestAccess::enter(credits);
    auto& credits_window=scene::SceneTestAccess::window(credits);credits_window.update(0);
    auto& scroll=dynamic_cast<ui::UiScrollContainer&>(*credits_window.child_at(1));
    auto& controls=dynamic_cast<ui::UiListContainer&>(*credits_window.child_at(2));
    require(scroll.screen_rect()==core::Rect{24,24,592,787}&&controls.screen_rect()==core::Rect{24,827,592,109},"Portrait credits must reserve the bottom controls outside its scroll area");
    const auto& images=dynamic_cast<const ui::UiListContainer&>(*scroll.content());
    require(images.child_count()==7,"Portrait credits lost an original image");
    for(std::size_t i=0;i<images.child_count();++i){
        const auto& image=dynamic_cast<const ui::UiImage&>(*images.child_at(i));
        const auto rect=image.screen_rect();
        require(rect.width()<=544.001f&&std::abs(rect.center().x-320)<.001f,"Portrait credits images must fit and remain horizontally centered");
        std::vector<core::UiRenderCommand> drawn;image.submit_ui_render_commands(drawn);
        require(drawn.size()==1,"Credits image draw missing");float width=0,height=0;require(SDL_GetTextureSize(drawn.front().texture,&width,&height),SDL_GetError());
        require(std::abs(rect.width()/rect.height()-width/height)<.001f&&rect.width()/width<=1.5001f,"Credits artwork must preserve aspect and the original scale limit");
    }
    capture(credits_window,"out/qa/credits-portrait-start-preview.bmp");
    scene::SceneTestAccess::update(credits,2.1);require(scroll.scroll_offset_y()>0,"Portrait credits auto scroll did not start");
    input::InputSystem input_system;input_system.initialize();
    const auto key=[&](SDL_Scancode scancode){
        for(bool down:{true,false}){input_system.begin_frame();SDL_Event event{};event.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;event.key.scancode=scancode;event.key.key=SDL_GetKeyFromScancode(scancode,SDL_KMOD_NONE,false);event.key.down=down;input_system.process_event(event);scene::SceneTestAccess::input(credits,input_system.snapshot());}
    };
    key(SDL_SCANCODE_SPACE);const float paused=scroll.scroll_offset_y();scene::SceneTestAccess::update(credits,.5);require(scroll.scroll_offset_y()==paused,"Space must pause portrait credits auto scroll");
    key(SDL_SCANCODE_SPACE);scene::SceneTestAccess::update(credits,.5);require(scroll.scroll_offset_y()>paused,"Space must resume portrait credits auto scroll");
    const float before=scroll.scroll_offset_y();key(SDL_SCANCODE_DOWN);require(scroll.scroll_offset_y()>before,"Down must manually scroll portrait credits");
    const float manual=scroll.scroll_offset_y();scene::SceneTestAccess::update(credits,.5);require(scroll.scroll_offset_y()==manual,"Manual scrolling must stop automatic scrolling");
    scroll.scroll_to_bottom();credits_window.update(0);require(scroll.scroll_offset_y()==scroll.max_scroll_offset().y&&images.child_at(6)->screen_rect().bottom()<=scroll.screen_rect().bottom()+.001f,"Last credits image must be reachable above the fixed controls");
    capture(credits_window,"out/qa/credits-portrait-end-preview.bmp");
    click(credits_window,*controls.child_at(1));require(credits_requests.received.size()==1&&credits_requests.received.back().route.target==game::scene_keys::MainMenu,"Portrait credits Back button must remain clickable");
    input_system.shutdown();scene::SceneTestAccess::exit(credits);
}
void check_frontend_artwork(SDL_Renderer* renderer){
    auto* gold=ELYSIA_RESOURCES->find_texture("fly.InGameUI.starDown");auto* empty=ELYSIA_RESOURCES->find_texture("fly.InGameUI.starUp");
    require(gold&&empty,"Collected and uncollected in-game star textures must both exist");
    require(SDL_SetRenderDrawColor(renderer,12,20,35,255)&&SDL_RenderClear(renderer),SDL_GetError());
    for(int count=0;count<=3;++count)for(auto size:{core::Vector2{136,40},core::Vector2{216,64}}){
        const core::Rect rect{160.f+320*count-size.x/2,size.y<50?150.f:460.f,size.x,size.y};
        game::frontend::StarRating stars(rect);stars.set_collected(count);
        std::vector<core::UiRenderCommand> icons;stars.submit_ui_render_commands(icons);require(icons.size()==3,"HUD / modal must always display all 3 star slots");
        for(int index=0;index<3;++index){require(icons[index].texture==(index<count?gold:empty)&&icons[index].alpha==255,"Collected stars must use InGameUI.starDown and uncollected stars InGameUI.starUp");require(icons[index].screen_rect.width()==icons[index].screen_rect.height(),"Collection icon aspect ratio changed");}
        game::frontend::HudPanel panel({rect.x()-20,rect.y()-12,rect.width()+40,rect.height()+24});
        std::vector<core::UiRenderCommand> background;panel.submit_ui_render_commands(background);
        for(const auto& command:background)core::require_render_success(core::execute_render_command(renderer,command));
        for(const auto& command:icons)core::require_render_success(core::execute_render_command(renderer,command));
    }
    std::filesystem::create_directories("out/qa");
    auto* star_preview=SDL_RenderReadPixels(renderer,nullptr);require(star_preview!=nullptr,SDL_GetError());
    const bool star_preview_saved=SDL_SaveBMP(star_preview,"out/qa/ingame-stars-preview.bmp");SDL_DestroySurface(star_preview);require(star_preview_saved,SDL_GetError());
    require(SDL_SetRenderDrawColor(renderer,12,20,35,255)&&SDL_RenderClear(renderer),SDL_GetError());
    std::vector<SDL_Texture*> selection_icons;
    for(char suffix:std::string("PYOG"))for(const char* prefix:{"point","lock","starDown","starUp","starGrey"}){
        auto* icon=ELYSIA_RESOURCES->find_texture(std::string("fly.frontend.")+prefix+suffix);
        require(icon!=nullptr,"Selection color/state texture missing");selection_icons.push_back(icon);
    }
    const auto& level_colors=game::content::catalog().at("selection").at("levels");
    constexpr char first_page_colors[]="OOOGGGYYYGGGGPP";
    for(int id=1;id<=60;++id){
        const int type=level_colors.at(std::to_string(id)).at(2).get<int>();
        std::string color;
        switch(type){case 1:color="P";break;case 2:color="Y";break;case 3:color="O";break;case 4:color="G";break;default:require(false,"Unknown original level color type");}
        if(id<=15)require(color[0]==first_page_colors[id-1],"First page must retain the original per-level color sequence");
        auto* point=ELYSIA_RESOURCES->find_texture("fly.frontend.point"+color);
        auto* lock=ELYSIA_RESOURCES->find_texture("fly.frontend.lock"+color);
        auto* full=ELYSIA_RESOURCES->find_texture("fly.frontend.starDown"+color);
        auto* empty=ELYSIA_RESOURCES->find_texture("fly.frontend.starUp"+color);
        auto* grey=ELYSIA_RESOURCES->find_texture("fly.frontend.starGrey"+color);
        for(int stars=-1;stars<=3;++stars)for(bool open:{true,false}){
            game::frontend::LevelNode node(id,stars,open,[]{});
            node.set_screen_rect({50.f+((id-1)%5)*240,130.f+((id-1)/5)*170,210,94});
            std::vector<core::UiRenderCommand> drawn;node.submit_ui_render_commands(drawn);int index=0,points=0,locks=0,icons=0;
            for(const auto& command:drawn){
                if(std::find(selection_icons.begin(),selection_icons.end(),command.texture)!=selection_icons.end())++icons;
                if(command.texture==point){++points;require(command.alpha==(open?255:65),"Selection point lock opacity changed");}
                if(command.texture==lock)++locks;
                if(command.texture==full||command.texture==empty||command.texture==grey){
                    require(command.texture==(!open||stars<0?grey:index<stars?full:empty),"Selection earned/empty/unplayed star texture is wrong");
                    require(command.alpha==(open?255:85),"Selection star lock opacity changed");++index;
                }
            }
            require(index==3,"Each level must retain three rating stars");
            require(points==1&&locks==(open?0:1)&&icons==(open?4:5),"Point, lock and stars must all use this level's configured color");
            if(id<=15&&stars==(id-1)%5-1&&open==(id%3!=0))
                for(const auto& command:drawn)core::require_render_success(core::execute_render_command(renderer,command));
        }
    }
    std::filesystem::create_directories("out/qa/selection-stars-audit");
    auto* preview=SDL_RenderReadPixels(renderer,nullptr);require(preview!=nullptr,SDL_GetError());
    const bool saved=SDL_SaveBMP(preview,"out/qa/selection-stars-audit/preview.bmp");SDL_DestroySurface(preview);require(saved,SDL_GetError());
    const auto achievements=game::content::read("text/en/gcDescriptions.json").at("achievements");
    for(const auto& [key,description]:achievements.items()){
        auto* badge=ELYSIA_RESOURCES->find_texture("fly.frontend.achv"+key);require(badge!=nullptr,"Original achievement badge is missing: "+key);
        for(bool earned:{false,true}){
            game::frontend::AchievementCard card(std::stoi(key),description.at("title"),description.at("earnedDesc"),earned);
            std::vector<core::UiRenderCommand> drawn;card.submit_ui_render_commands(drawn);
            int badges=0,masks=0;
            for(const auto& command:drawn){
                if(command.texture==badge){++badges;require(command.screen_rect.width()==92&&command.screen_rect.height()==92,"Achievement badge must preserve its square aspect");}
                if(command.color==core::Color{4,12,31,170}){++masks;require(command.screen_rect.width()==92&&command.screen_rect.height()==92,"Locked mask must cover the badge");}
            }
            require(badges==1&&masks==(earned?0:1),"Locked achievements must show the original badge under a dark mask; earned badges must stay clear");
        }
    }
    for(const auto viewport:{core::Rect{0,0,640,960},core::Rect{0,0,1280,720}})for(const char* name:{"menu","settings","moon"}){
        game::frontend::MoonBackdrop background(viewport,name);std::vector<core::UiRenderCommand> drawn;background.submit_ui_render_commands(drawn);
        auto* source=ELYSIA_RESOURCES->find_texture(std::string("fly.frontend.background_")+name);
        require(source!=nullptr,"Supplied page background missing");
        const auto found=std::find_if(drawn.begin(),drawn.end(),[&](const auto& command){return command.texture==source;});
        require(found!=drawn.end()&&found->use_src_rect,"Background cover must use an aspect-preserving source crop");
        require(found->screen_rect==viewport,"Page background did not fill its current viewport");
        require(std::abs(found->src_rect.width()/found->src_rect.height()-viewport.width()/viewport.height())<.001f,"Page background was stretched");
        float width=0,height=0;require(SDL_GetTextureSize(source,&width,&height),SDL_GetError());
        require(found->src_rect.x()>=-.001f&&found->src_rect.y()>=-.001f&&found->src_rect.right()<=width+.001f&&found->src_rect.bottom()<=height+.001f,"Page background crop escaped the supplied image");
    }
}
std::vector<core::RenderCommand> commands(const core::GameObject& object);
void check_driven_rotor_pause() {
    using namespace game::play;
    const auto level=game::content::level(39);
    const auto data=*std::find_if(level.at("entities").begin(),level.at("entities").end(),[](const auto& e){return e.at("kind")==108&&e.at("id")==0;});
    Entity rotor(data);Pivot pivot(rotor.origin);
    physics::PhysicsWorld world({.units_per_meter=32,.gravity={0,320}});
    const auto body=world.register_object(rotor,rotor.body_definition(),rotor.collider_definitions());
    const auto anchor=world.register_object(pivot,pivot.body_definition(),pivot.collider_definitions());
    require(body.is_valid()&&anchor.is_valid(),"Driven rotor fixture registration failed");rotor.bind_physics(world,body);
    const auto joint=world.create_revolute_joint({.first=anchor,.second=body,.local_anchor_first={},.local_anchor_second={},
        .settings={.enable_motor=true,.motor_speed=seesaw_speed(rotor.info),.max_motor_torque=102400}});
    require(joint.is_valid(),"Driven rotor test joint failed");
    for(int cycle=0;cycle<3;++cycle) {
        for(int i=0;i<90;++i)world.step(1./60);
        const auto before=world.body_state(body);
        require(before&&before->awake&&std::abs(before->angular_velocity-seesaw_speed(rotor.info))<.001,
            "Powered rotor fell asleep: angular velocity="+std::to_string(before->angular_velocity)+", awake="+std::to_string(before->awake));
        // Scene pause omits physics steps, while still finalizing display frames.
        for(int i=0;i<120;++i)world.finalize_frame(.5);
        require(world.body_state(body)->angle==before->angle,"Paused rotor changed physical angle");
        world.step(1./60);const auto after=world.body_state(body);
        require(after->awake&&std::abs(after->angular_velocity-seesaw_speed(rotor.info))<.001&&after->angle!=before->angle,"Powered rotor did not continue after pause");
    }
    rotor.unbind_physics();require(world.unregister_object(body)&&world.unregister_object(anchor),"Driven rotor fixture cleanup failed");
    auto passive_data=data;passive_data["info"]["Speed"]="0";Entity passive(passive_data);
    require(passive.body_definition().enable_sleep,"Passive rods should retain normal sleeping policy");
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
        if(kind==101){
            physics::PhysicsWorld world({.units_per_meter=32,.gravity={0,0}});
            const auto moon=world.register_object(entity,entity.body_definition(),entity.collider_definitions());
            require(moon.is_valid(),"Moon trigger fixture registration failed");
            for(const auto [distance,expected_overlap]:std::array<std::pair<float,bool>,3>{{{50.f,true},{62.f,true},{64.f,false}}}){
                std::vector<physics::CollisionOverlapQueryHit> hits;
                world.overlap_shape({.shape=game::play::role_sensor_shape(),.pose={entity.center()+core::Vector2{-distance,13},0},
                    .filter={.collision={.category=1,.mask=4}}},hits);
                require((!hits.empty())==expected_overlap,"Moon must accept the original expanded trigger range and reject points beyond it");
            }
            require(world.unregister_object(moon),"Moon trigger fixture cleanup failed");
        }
        const auto expected=ELYSIA_RESOURCES->find_texture(found->at("key").template get<std::string>()+".first");
        require(expected!=nullptr,"Normal-state texture missing");
        for(int frame=0;frame<180;++frame) {
            entity.update(1./60);
            require(sprite(entity).texture==expected,
                "Pickup/moon is cycling between normal and effect images: kind "+std::to_string(kind));
        }
    }
}
void check_result_star_effects(SDL_Renderer* renderer){
    using game::frontend::StarRating;
    StarRating stars({532,234,216,64});
    const auto drawn=[&]{std::vector<core::UiRenderCommand> out;stars.submit_ui_render_commands(out);return out;};
    auto* empty=ELYSIA_RESOURCES->find_texture("fly.InGameUI.starUp");
    auto* full=ELYSIA_RESOURCES->find_texture("fly.InGameUI.starDown");
    auto* spark=ELYSIA_RESOURCES->find_texture("fly.result_star_particles");
    require(spark!=nullptr,"Original result particle texture is unregistered");
    stars.present_result(3,.1);const auto before=drawn();
    require(stars.collected()==0&&before.size()>3&&before[0].texture==empty,"Result particles must precede the first bright star");
    for(std::size_t i=0;i<before.size();++i){
        require(before[i].rotation_degrees==0,"Settlement must not rotate stars or particle sprites");
        if(i<3)require(before[i].screen_rect==stars.slot_rect(static_cast<int>(i)),"Settlement must not move or scale the star slots");
        else require(before[i].texture==spark&&before[i].texture_color_modulation.has_value(),"Settlement must use its own grayscale texture and lifetime color");
    }
    SDL_BlendMode blend{};require(SDL_GetTextureBlendMode(spark,&blend)&&blend==SDL_BLENDMODE_ADD,"Original zero-alpha premultiplied star particles must add their RGB");
    require(drawn().size()==before.size()&&drawn().back().screen_rect==before.back().screen_rect,"Redrawing settlement must not advance or randomize particles");
    stars.present_result(3,.199);require(stars.collected()==0,"Bright star appeared too early");
    stars.present_result(3,.2);require(stars.collected()==1&&drawn()[0].texture==full,"First star must light at 0.2 seconds");
    stars.present_result(3,.6);require(stars.collected()==1&&drawn().size()>3,"Second particle burst must precede its own bright star");
    stars.present_result(3,.7);require(stars.collected()==2,"Second star must light at 0.7 seconds");
    stars.present_result(3,1.2);require(stars.collected()==3,"Third star must light at 1.2 seconds");
    stars.present_result(3,1.9);require(drawn().size()==3,"All particles must expire after their finite emission and life");
    stars.present_result(1,.95);require(stars.collected()==1&&drawn().size()==3,"Uncollected stars must not emit settlement particles");
    stars.present_result(0,.1);require(drawn().size()==3&&stars.collected()==0,"Zero-star settlement must have no bursts");
    stars.present_result(3,.1);stars.set_collected(2);require(drawn().size()==3&&stars.collected()==2,"Static/failure/skip displays must clear particle presentation");

    require(SDL_SetRenderDrawColor(renderer,12,20,35,255)&&SDL_RenderClear(renderer),SDL_GetError());
    const auto render=[&](const ui::UiElement& element){std::vector<core::UiRenderCommand> out;element.submit_ui_render_commands(out);for(const auto& c:out)core::require_render_success(core::execute_render_command(renderer,c));};
    const std::array<double,9> times{{0,.1,.199,.2,.6,.7,1.1,1.2,1.9}};
    for(int i=0;i<9;++i){
        const float x=(i%3)*426.f,y=(i/3)*240.f;
        game::frontend::HudPanel panel({x+12,y+12,402,216});render(panel);
        game::frontend::HudLabel label({x+40,y+28,346,40},0,ui::ui_raw_text(std::format("{:.3f} s",times[i])));render(label);
        StarRating sample({x+105,y+106,216,64});sample.present_result(3,times[i]);render(sample);
    }
    auto* preview=SDL_RenderReadPixels(renderer,nullptr);require(preview!=nullptr,SDL_GetError());
    std::filesystem::create_directories("out/qa");
    const bool saved=SDL_SaveBMP(preview,"out/qa/result-star-effects-preview.bmp");SDL_DestroySurface(preview);require(saved,SDL_GetError());
}
void check_pickup_effects(SDL_Renderer* renderer){
    const auto level=game::content::level(1);
    for(int kind:{4,5}){
        const auto found=std::find_if(level.at("entities").begin(),level.at("entities").end(),[kind](const auto& data){return data.at("kind")==kind;});
        game::play::Entity crystal(*found);const auto idle=sprite(crystal);
        crystal.begin_pickup();crystal.update(.15);auto enlarged=sprite(crystal);
        require(enlarged.texture==idle.texture&&std::abs(enlarged.command_rect.width()/idle.command_rect.width()-1.1f)<.001f&&enlarged.command_rect.center()==idle.command_rect.center(),"Both crystal kinds must grow in place using their normal sprite");
        crystal.begin_pickup();crystal.update(.15);
        require(std::abs(sprite(crystal).command_rect.width()/idle.command_rect.width()-1.2f)<.001f,"Duplicate pickup must not restart the 0.3 second enlargement");
        crystal.update(.15);require(std::abs(sprite(crystal).command_rect.width()/idle.command_rect.width()-.65f)<.001f,"Crystal must shrink toward 0.1 scale during its second phase");
        crystal.update(.15);require(commands(crystal).empty()&&!crystal.is_visible(),"Crystal must disappear after 0.6 seconds");
        crystal.restore_pickup();require(!crystal.collected&&crystal.is_visible()&&sprite(crystal).command_rect==idle.command_rect,"Scripted restore must reset collection, size and visibility");
    }
    using game::frontend::StarPickupEffects;
    game::frontend::StarRating hud({982,24,136,40});StarPickupEffects flight;
    const core::Vector2 from{180,480},destination=hud.slot_rect(0).center();
    const auto drawn=[&]{std::vector<core::UiRenderCommand> out;flight.submit_ui_render_commands(out);return out;};
    flight.launch(from,destination);const auto birth=drawn();
    require(birth.size()==1&&birth[0].texture==ELYSIA_RESOURCES->find_texture("fly.InGameUI.animateStar")&&birth[0].screen_rect.center()==from,"Star pickup must start at the captured screen position using animateStar");
    flight.advance(.05);const auto grown=drawn();
    require((grown[0].screen_rect.center()-from).length()<.001f&&std::abs(grown[0].screen_rect.width()/birth[0].screen_rect.width()-1.4f)<.001f,"Star must grow to 1.4 over 0.05 seconds before moving");
    flight.advance(.3);const auto middle=drawn();
    require((middle[0].screen_rect.center()-(from+destination)*.5f).length()<.001f&&std::abs(middle[0].screen_rect.width()-birth[0].screen_rect.width())<.001f,"Star must move linearly toward its actual HUD slot while shrinking");
    require(drawn()[0].screen_rect==middle[0].screen_rect,"Rendering must not advance the star pickup timeline");
    flight.launch({400,520},hud.slot_rect(1).center());flight.advance(.3);
    require(flight.pending_count()==1,"Overlapping star pickups must finish independently in collection order");
    flight.clear();require(flight.pending_count()==0&&drawn().empty(),"Reset and settlement must remove every in-flight star");

    require(SDL_SetRenderDrawColor(renderer,12,20,35,255)&&SDL_RenderClear(renderer),SDL_GetError());
    const auto render_ui=[&](const ui::UiElement& element){std::vector<core::UiRenderCommand> out;element.submit_ui_render_commands(out);for(const auto& c:out)core::require_render_success(core::execute_render_command(renderer,c));};
    const std::array<double,6> times{{0,.15,.3,.45,.599,.65}};
    for(int column=0;column<6;++column){
        const float x=100.f+210*column;
        game::frontend::HudLabel time({x-80,28,160,40},0,ui::ui_raw_text(std::format("{:.2f} s",times[column])));render_ui(time);
        for(int kind:{4,5}){
            const auto found=std::find_if(level.at("entities").begin(),level.at("entities").end(),[kind](const auto& data){return data.at("kind")==kind;});
            game::play::Entity crystal(*found);crystal.begin_pickup();crystal.update(times[column]);
            for(auto command:commands(crystal)){
                const auto extent=command.command_rect.size()*2;
                command.command_rect=core::Rect::from_center({x,kind==4?160.f:280.f},extent);
                core::require_render_success(core::execute_render_command(renderer,core::project_render_command_to_screen(command,camera::Camera({640,360},{1280,720},1))));
            }
        }
        game::frontend::StarRating slot({x-68,400,136,40});slot.set_collected(times[column]>=.65?1:0);render_ui(slot);
        StarPickupEffects sample;sample.launch({x,640},slot.slot_rect(0).center());sample.advance(times[column]);render_ui(sample);
    }
    auto* preview=SDL_RenderReadPixels(renderer,nullptr);require(preview!=nullptr,SDL_GetError());
    const bool saved=SDL_SaveBMP(preview,"out/qa/pickup-effects-preview.bmp");SDL_DestroySurface(preview);require(saved,SDL_GetError());
}
void check_thrust_particles(SDL_Renderer* renderer){
    using namespace game::play;
    Player player({100,300});
    require(commands(player).size()==1,"Idle role must not draw particles");
    auto* fire=ELYSIA_RESOURCES->find_texture("fly.role_fire");SDL_BlendMode blend{};
    require(fire&&SDL_GetTextureBlendMode(fire,&blend)&&blend==SDL_BLENDMODE_ADD,"Role particles must use their dedicated additive fire texture");
    player.thrust={0,-1};player.update(.1);const auto drawn=commands(player);
    require(drawn.size()==31,"A tenth of a second must emit 30 particles before the role sprite");
    for(std::size_t i=0;i+1<drawn.size();++i)require(drawn[i].texture==fire&&drawn[i].texture_color_modulation.has_value(),"Particles must render with the source sprite and lifetime color");
    require(drawn.front().alpha<drawn[drawn.size()-2].alpha,"Older particles must fade below newborn particle opacity");
    const auto repeated=commands(player);require(repeated.size()==drawn.size()&&repeated.front().command_rect==drawn.front().command_rect,"Rendering must not advance particle simulation");
    player.thrust={};require(commands(player).size()==1,"Releasing thrust must hide particles immediately");player.update(.1);
    player.thrust={0,-1};player.update(1./60.);require(commands(player).size()==6,"A new burst must start without stale particles");
    player.update(.1);player.present_at(Vec{500,500});require(commands(player).size()==1,"Portal and moon presentation must hide particles");
    player.present_at(std::nullopt);player.die();require(commands(player).size()==1,"Death must hide particles");

    const Vec fly_left{-.7660444f,-.6427876f},fly_right{.7660444f,-.6427876f};
    for(const auto thrust:{Vec{0,-1},fly_left,fly_right,Vec{.19f,-.8f}}){
        Player sample({100,300});sample.thrust=thrust;sample.update(1./60.);
        const auto particles=commands(sample);const auto role=sample.render_rect();
        const auto birth=particles[particles.size()-2].command_rect.center();
        const bool tilted=std::abs(thrust.x)>.2f;
        const Vec expected{role.center().x+(tilted?(thrust.x<0?9.f:-9.f):0.f),role.bottom()-(tilted?8.f:4.f)};
        require((birth-expected).length()<.001f,"Newborn particles must follow the feet of the actual straight or side-flight sprite");
        if(tilted)require((particles.front().command_rect.center().x-birth.x)*thrust.x<0,"Side-flight particles must travel opposite to thrust");
    }

    require(SDL_SetRenderDrawColor(renderer,24,36,54,255)&&SDL_RenderClear(renderer),SDL_GetError());
    const std::array<Vec,4> thrusts{{{0,-1},{-.7660444f,-.6427876f},{.7660444f,-.6427876f},{0,-.25f}}};
    for(int row=0;row<2;++row)for(int column=0;column<4;++column){
        Player sample({0,0});sample.thrust=thrusts[column];
        for(int i=0;i<(row==0?18:3);++i)sample.update(1./60.);
        const camera::Camera preview(sample.render_rect().center(),{1280,720},2);
        for(const auto& command:commands(sample)){
            auto screen=core::project_render_command_to_screen(command,preview);
            screen.screen_rect=screen.screen_rect.translated({160.f+320*column-640,110.f+360*row-360});
            core::require_render_success(core::execute_render_command(renderer,screen));
        }
    }
    auto* captured=SDL_RenderReadPixels(renderer,nullptr);require(captured!=nullptr,SDL_GetError());
    std::filesystem::create_directories("out/qa");const bool saved=SDL_SaveBMP(captured,"out/qa/role-particles-preview.bmp");SDL_DestroySurface(captured);require(saved,SDL_GetError());
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
        if(!homing)require(std::abs(state->position.x-100)<.1,"Gravity changed horizontal launch speed");
        if(homing)require(std::abs(state->world_center.y+1.6f)<.01&&std::abs(state->velocity.y)<.01&&state->position.x>94&&state->position.x<96&&state->velocity.x>90&&state->velocity.x<91,"Homing gravity compensation or native linear damping incorrect");
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
void check_homing_launch_and_motion(SDL_Renderer* renderer) {
    using namespace game::play;
    auto data=mechanism(113);data["info"]=game::content::Json::object();data["x"]=100;data["y"]=100;
    require(homing_power(data["info"])==1&&homing_max_speed(data["info"])==-1&&homing_active_range(data["info"])==1000,"Native homing defaults differ");
    for(float degrees:{0.f,45.f,90.f,180.f,270.f}) {
        data["rotation"]=degrees;Entity launcher(data);const float radians=degrees*std::numbers::pi_v<float>/180;
        const Vec expected=launcher.center()+rotate_vector({-1.75f,-31},radians);
        require((launcher.fire_position({1,0})-expected).length()<.001&&(launcher.fire_position({0,-1})-expected).length()<.001,"Homing muzzle depends on target or ignores native anchor/rotation");
    }
    data["rotation"]=0;
    Entity defaults(data);require(defaults.shots_due(.99,defaults.center())==0&&defaults.shots_due(.02,defaults.center())==1,"Default homing sequence must first fire at 1 second");
    require(defaults.shots_due(1.99,defaults.center())==0&&defaults.shots_due(.02,defaults.center())==1,"Default homing sequence must repeat every 2 seconds");
    data["info"]={{"Seq","0,2"},{"Lag","100"}};Entity lag(data);require(lag.shots_due(.01,lag.center())==1,"Original homing launcher does not consume Lag");
    Entity in_range(data),out_range(data);const Vec anchor=in_range.center()+Vec{-1.75f,-13};
    require(in_range.shots_due(.01,anchor+Vec{999,0})==1&&out_range.shots_due(.01,anchor+Vec{1001,0})==0,"Homing active range must use native launcher anchor");
    data["info"]["CarrierID"]="0";Entity carried(data);physics::PhysicsWorld carrier_world({.units_per_meter=32,.gravity={0,320}});
    const auto carrier_body=carrier_world.register_object(carried,carried.body_definition(),carried.collider_definitions());require(carrier_body.is_valid(),"Homing carrier fixture failed");carried.bind_physics(carrier_world,carrier_body);
    require(carrier_world.set_transform(carrier_body,{{200,300},std::numbers::pi_v<float>/2}),"Homing carrier move failed");
    require((carried.fire_position({1,0})-Vec{231,298.25f}).length()<.001,"Moving carrier launch point is stale");
    carried.unbind_physics();require(carrier_world.unregister_object(carrier_body),"Homing carrier cleanup failed");

    physics::PhysicsWorld world({.units_per_meter=32,.gravity={0,320}});Player player({10000,10000});
    Projectile missile({0,0},{},true,1,-1);const auto def=missile.body_definition();
    require(def.type==physics::BodyType::Dynamic&&!def.fixed_rotation&&def.velocity==Vec{}&&def.linear_damping==.1f&&def.angular_damping==4,"Native homing body settings differ");
    require(std::get<physics::ConvexPolygonShape>(missile.collider_definitions()[0].shape).vertex_count==5,"Homing collider must be the native five-point shape");
    const auto body=world.register_object(missile,def,missile.collider_definitions());require(body.is_valid(),"Homing physics registration failed");missile.bind_physics(world,body);
    const auto mass=*missile.physics_state();require(std::abs(mass.mass-.1875f)<1e-6&&std::abs(mass.local_center.y+1.6f)<1e-5&&std::abs(mass.rotational_inertia-3.63479187f)<1e-5,"Homing mass, COM or independent inertia wrong");
    missile.advance(1./60,{1000,-1.6f},player);world.step(1./60);
    const auto first=*missile.physics_state();
    require(first.velocity.x>.53f&&first.velocity.x<.534f&&std::abs(first.velocity.y)<1e-4&&first.angular_velocity>0,"Homing thrust acceleration or nose torque wrong");
    require(std::abs(first.angle)>1e-5&&std::abs(first.angle-std::atan2(first.velocity.y,first.velocity.x))>1e-5,"Homing orientation must come from body torque");
    for(int i=0;i<120;++i){missile.advance(1./60,{1000,-1.6f},player);world.step(1./60);}
    world.finalize_frame(1);
    auto command=sprite(missile);const auto pose=*world.render_pose(body);
    require(command.rotation_degrees>10&&std::abs(command.rotation_degrees-pose.angle*180/std::numbers::pi_v<float>)<.001,"Homing sprite must rotate with interpolated body pose");
    require(std::abs(command.command_rect.width()-16)<.001&&std::abs(command.command_rect.height()-25.5f)<.001,"Homing sprite must retain native aspect ratio");
    require(world.set_velocity(body,{300,0}),"Homing speed fixture setup failed");
    missile.advance(0,{1000,0},player);require(missile.velocity().x==300,"Negative MaxSpeed must leave speed uncapped");
    missile.unbind_physics();require(world.unregister_object(body),"Homing cleanup failed");
    Projectile capped({0,0},{300,0},true,0,2.5f);const auto cap_body=world.register_object(capped,capped.body_definition(),capped.collider_definitions());require(cap_body.is_valid(),"Homing cap fixture registration failed");capped.bind_physics(world,cap_body);
    capped.advance(0,{1000,0},player);require(std::abs(capped.velocity().length()-80)<.001,"Positive MaxSpeed must use 32 EU/m");
    capped.advance(12.1,{1000,0},player);require(!capped.expired,"Homing lifetime must exceed old 12 seconds");
    capped.advance(19.9,{1000,0},player);require(capped.expired,"Homing must expire at native 32 seconds");
    capped.unbind_physics();require(world.unregister_object(cap_body),"Homing cap cleanup failed");

    require(SDL_SetRenderDrawColor(renderer,12,20,35,255)&&SDL_RenderClear(renderer),SDL_GetError());
    const camera::Camera view({640,360},{1280,720},1);
    for(int i=0;i<5;++i){Projectile sample({160.f+240*i,350},{},true,1,-1,i*std::numbers::pi_v<float>/4);auto c=sprite(sample);const auto p=c.command_rect.center();c.command_rect={p.x-c.command_rect.width()*2,p.y-c.command_rect.height()*2,c.command_rect.width()*4,c.command_rect.height()*4};core::require_render_success(core::execute_render_command(renderer,core::project_render_command_to_screen(c,view)));}
    auto* image=SDL_RenderReadPixels(renderer,nullptr);require(image!=nullptr,SDL_GetError());const bool saved=SDL_SaveBMP(image,"out/qa/homing-rotation-preview.bmp");SDL_DestroySurface(image);require(saved,SDL_GetError());
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
void check_platform_spikes(SDL_Renderer* renderer){
    using namespace game::play;
    const auto anchor_of=[](const core::RenderCommand& c){return c.command_rect.position()+core::Vector2{c.rotation_origin.x*c.command_rect.width(),c.rotation_origin.y*c.command_rect.height()};};
    const auto verify=[&](const Entity& platform,const std::string& flags){
        const int blocks=platform.kind-104;
        const auto drawn=commands(platform);const auto& body=drawn.at(0);
        const auto* frame=ELYSIA_RESOURCES->find_atlas("fly.DynamicObjectsLite.DynamicObject_"+std::to_string(platform.kind)+".S01")->frame_at(0);
        std::vector<std::pair<core::Vector2,float>> expected;
        // Independent native E/N/W/S attachment points, with 32-unit pitch.
        const float end=std::array<float,3>{18,34,50}[blocks-1];
        if(flags[0]=='1')expected.push_back({{end,0},90.f});
        if(flags[1]=='1')for(int i=0;i<blocks;++i)expected.push_back({{16.f*(blocks-1)-32.f*i,-18},0.f});
        if(flags[2]=='1')expected.push_back({{-end,0},270.f});
        if(flags[3]=='1')for(int i=0;i<blocks;++i)expected.push_back({{16.f*(blocks-1)-32.f*i,18},180.f});
        require(drawn.size()==1+expected.size(),"Platform must use one strip per end and one per block on top/bottom");
        const float radians=body.rotation_degrees*std::numbers::pi_v<float>/180;
        for(std::size_t i=0;i<expected.size();++i){
            const auto& spike=drawn[i+1];
            require(spike.texture==frame->_texture&&spike.use_src_rect==frame->_source_rect.has_value()&&(!spike.use_src_rect||spike.src_rect==*frame->_source_rect),"Platform spikes must use their own S01 frame and atlas rectangle");
            require(std::abs(spike.command_rect.width()-27.5f)<.001f&&std::abs(spike.command_rect.height()-18)<.001f,"Spike strips must retain the original HD half-size instead of a 16x16 square");
            require((spike.rotation_origin-core::Vector2{.5f,2.f/3}).length()<.001f,"Spike strips must rotate about the original bottom-third anchor");
            require((anchor_of(spike)-(body.command_rect.center()+rotate_vector(expected[i].first,radians))).length()<.002f,"Spike anchor must follow the same interpolated position and angle as the platform");
            require(std::abs(spike.rotation_degrees-body.rotation_degrees-expected[i].second)<.001f,"Spike strips must add their own outward-facing side rotation");
        }
    };
    for(int kind:{105,106,107})for(int mask=0;mask<16;++mask)for(float angle:{0.f,45.f,90.f,180.f,270.f}){
        std::string flags="0000";for(int side=0;side<4;++side)if(mask&(1<<side))flags[side]='1';
        auto data=mechanism(kind);data["info"]["Spikes"]=flags;data["rotation"]=angle;
        Entity platform(data);verify(platform,flags);
    }
    int fixtures=0;
    for(int level=1;level<=60;++level){
        const auto level_data=game::content::level(level);
        for(const auto& data:level_data.at("entities")){
            const int kind=data.at("kind");const auto flags=data.at("info").value("Spikes",std::string("0000"));
            if(kind>=105&&kind<=107&&flags.find('1')!=std::string::npos){Entity platform(data);verify(platform,flags);++fixtures;}
        }
    }
    require(fixtures==146,"All 146 shipped spiked platforms must retain their side assignments");
    {
        auto data=mechanism(106);data["info"]["Spikes"]="1111";
        Entity platform(data);physics::PhysicsWorld world({.gravity={0,0}});
        const auto body=world.register_object(platform,platform.body_definition(),platform.collider_definitions());
        require(body.is_valid(),"Spiked platform interpolation fixture failed");platform.bind_physics(world,body);
        require(world.set_velocity(body,{60,20})&&world.set_angular_velocity(body,1),"Spiked platform motion fixture failed");
        world.step(1./60);world.finalize_frame(.35);verify(platform,"1111");
        const auto pose=world.render_pose(body);require(pose&&(commands(platform)[0].command_rect.center()-pose->position).length()<.001f,"Spiked platform must use its physics presentation pose");
        // Original sensor centers coincide with the four attachment lines.
        const auto colliders=platform.collider_definitions();require(colliders.size()==5,"Spiked platform must retain all four lethal side sensors");
        const std::array<core::Vector2,4> centers{{{34,0},{0,-18},{-34,0},{0,18}}};
        for(int i=0;i<4;++i)require(platform.lethal(platform.physics_collider(i+1))&&std::get<physics::BoxShape>(colliders[i+1].shape).local_center==centers[i],"Side-facing artwork must retain the original lethal sensor attachment");
        platform.unbind_physics();require(world.unregister_object(body),"Spiked platform fixture cleanup failed");
    }
    require(SDL_SetRenderDrawColor(renderer,16,25,38,255)&&SDL_RenderClear(renderer),SDL_GetError());
    const std::array<std::string,6> flags{{"1000","0100","0010","0001","1111","1111"}};
    const std::array<const char*,6> headings{{"右侧","上侧","左侧","下侧","四侧","旋转 45°"}};
    for(int column=0;column<6;++column){
        const float x=110.f+210*column;
        game::frontend::HudLabel heading({x-85,16,170,40},0,ui::ui_raw_text(headings[column]));
        std::vector<core::UiRenderCommand> label;heading.submit_ui_render_commands(label);for(const auto& command:label)core::require_render_success(core::execute_render_command(renderer,command));
        for(int row=0;row<3;++row){
            auto data=mechanism(105+row);data["info"]["Spikes"]=flags[column];if(column==5)data["rotation"]=45;
            Entity platform(data);const camera::Camera view(platform.position(),{1280,720},1.5f);
            for(const auto& command:commands(platform)){
                auto projected=core::project_render_command_to_screen(command,view);
                projected.screen_rect=projected.screen_rect.translated(core::Vector2{x,160.f+220*row}-core::Vector2{640,360});
                core::require_render_success(core::execute_render_command(renderer,projected));
            }
        }
    }
    auto* preview=SDL_RenderReadPixels(renderer,nullptr);require(preview!=nullptr,SDL_GetError());
    const bool saved=SDL_SaveBMP(preview,"out/qa/platform-spikes-preview.bmp");SDL_DestroySurface(preview);require(saved,SDL_GetError());
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
void check_laser_visuals(SDL_Renderer* renderer) {
    using namespace game::play;
    {
        // Level 30's mirrored static island must shield the actual spawn from
        // the downward laser. A generic box-wall test cannot catch import bugs.
        const auto level=game::content::level(30);
        const auto source=std::find_if(level.at("entities").begin(),level.at("entities").end(),[](const auto& e){return !e.at("item").template get<bool>()&&e.at("id")==4;});
        require(source!=level.at("entities").end(),"Level 30 downward laser missing");
        physics::PhysicsWorld spawn_world({.units_per_meter=32,.gravity={0,0}});
        Entity spawn_laser(*source);Player spawn_player({level.at("born")[0],level.at("born")[1]});
        const auto laser_handle=spawn_world.register_object(spawn_laser,spawn_laser.body_definition(),spawn_laser.collider_definitions());
        const auto role_handle=spawn_world.register_object(spawn_player,spawn_player.body_definition(),spawn_player.collider_definitions());
        require(laser_handle.is_valid()&&role_handle.is_valid(),"Level 30 laser/role registration failed");
        spawn_laser.bind_physics(spawn_world,laser_handle);spawn_player.bind_physics(spawn_world,role_handle);
        std::vector<std::unique_ptr<Edge>> terrain;
        std::vector<physics::PhysicsObjectHandle> terrain_handles;
        for(const auto& line:level.at("edges"))for(std::size_t i=1;i<line.size();++i){
            auto edge=std::make_unique<Edge>(Vec{line[i-1][0],line[i-1][1]},Vec{line[i][0],line[i][1]});
            const auto handle=spawn_world.register_object(*edge,edge->body_definition(),edge->collider_definitions());
            require(handle.is_valid(),"Level 30 terrain registration failed");
            terrain_handles.push_back(handle);terrain.push_back(std::move(edge));
        }
        require(!spawn_laser.update_laser(1./60,spawn_player.center(),spawn_player)&&spawn_laser.laser_active(),"Level 30 island failed to shield the player at spawn");
        require(spawn_laser.laser_length()>80&&spawn_laser.laser_length()<120,"Level 30 laser must stop on the middle island");
        for(const auto handle:terrain_handles)require(spawn_world.unregister_object(handle),"Level 30 terrain cleanup failed");
        require(spawn_laser.update_laser(1./60,spawn_player.center(),spawn_player),"Level 30 regression did not exercise a lethal unobstructed beam");
        spawn_laser.unbind_physics();spawn_player.unbind_physics();
        require(spawn_world.unregister_object(laser_handle)&&spawn_world.unregister_object(role_handle),"Level 30 fixture cleanup failed");
    }
    auto data=mechanism(114);data["x"]=0;data["y"]=0;data["rotation"]=0;data["info"]={{"FixedAngle","90"},{"ActiveRange","-1"}};
    const auto key=data.at("key").get<std::string>();
    Entity laser(data);const auto idle=commands(laser);
    require(idle.size()==3,"Laser grass must draw two S01 head parts plus its base when idle");
    require(idle[0].texture==ELYSIA_RESOURCES->find_atlas(key+".S01")->texture_at(1)&&idle[1].texture==ELYSIA_RESOURCES->find_atlas(key+".S01")->texture_at(0),"Laser head texture order incorrect");
    require(idle[0].rotation_origin==core::Vector2{0,1}&&idle[1].rotation_origin==core::Vector2{0,1},"Laser head must rotate around its original bottom-left anchor");
    physics::PhysicsWorld world({.units_per_meter=32,.gravity={0,0}});
    const auto handle=world.register_object(laser,laser.body_definition(),laser.collider_definitions());require(handle.is_valid(),"Laser visual fixture registration failed");laser.bind_physics(world,handle);
    Player player({-25,-300});const auto player_handle=world.register_object(player,player.body_definition(),player.collider_definitions());require(player_handle.is_valid(),"Laser target registration failed");player.bind_physics(world,player_handle);
    core::GameObject wall(core::DepthLayer::Terrain);wall.set_world_rect({0,-200,1,1});physics::BodyDefinition wall_body;wall_body.type=physics::BodyType::Static;
    physics::Collider wall_shape;wall_shape.shape=physics::BoxShape::from_rect({-200,-5,400,10});wall_shape.filter={.category=2,.mask=1|8};
    const auto wall_handle=world.register_object(wall,wall_body,std::span(&wall_shape,1));require(wall_handle.is_valid(),"Laser blocking wall registration failed");
    require(!laser.update_laser(1./60,player.center(),player)&&laser.laser_active(),"Laser visual change bypassed terrain occlusion");
    require(laser.laser_length()>170&&laser.laser_length()<190,"Laser beam did not stop at the blocking wall");
    auto active=commands(laser);require(active.size()==7,"Active laser must draw cap, shaft, lightning, two head parts, base and impact");
    require(active[0].texture==ELYSIA_RESOURCES->find_atlas(key+".S02")->texture_at(1)&&active[1].texture==ELYSIA_RESOURCES->find_atlas(key+".S02")->texture_at(0),"Laser core textures are reversed or missing");
    const auto start=laser_muzzle_offset(std::numbers::pi_v<float>/2,laser.size());
    const auto anchor=[](const core::RenderCommand& c){return c.command_rect.position()+core::Vector2{c.command_rect.width()*c.rotation_origin.x,c.command_rect.height()*c.rotation_origin.y};};
    require((anchor(active[0])-start).length()<.001&&std::abs(active[0].command_rect.height()-9)<.001,"Laser cap does not align with native muzzle");
    require((anchor(active[1])-(start+core::Vector2{0,-9})).length()<.001&&std::abs(active[1].command_rect.height()-(laser.laser_length()-9))<.001,"Laser shaft does not join cap or stop at hit distance");
    require(active[2].command_rect.height()<=53&&std::abs(active[6].command_rect.center().y+195)<.001,"Lightning extent or impact point incorrect");
    const auto first_lightning=active[2].texture;laser.update(.1);active=commands(laser);
    require(active.size()>7&&active[2].texture!=first_lightning,"Laser lightning/particle animation did not advance");
    const auto* stars=ELYSIA_RESOURCES->find_texture("fly.laser_stars");require(std::any_of(active.begin(),active.end(),[&](const auto& c){return c.texture==stars;}),"Laser world particles are missing");
    std::set<SDL_Texture*> impacts;for(int i=0;i<20;++i){laser.update(1./60);impacts.insert(commands(laser)[6].texture);}
    require(impacts.size()==20,"Laser impact must use all twenty S04 frames");
    laser.info["ActiveRange"]="1";laser.update_laser(1./60,player.center(),player);laser.update(.3);
    require(!laser.laser_active()&&commands(laser).size()==3,"Inactive laser retains beam, impact or expired particles");
    laser.info["ActiveRange"]="-1";require(world.unregister_object(wall_handle),"Laser wall cleanup failed");
    require(laser.update_laser(1./60,player.center(),player),"Laser no longer damages an unobstructed player");
    require(world.set_body_enabled(player_handle,false),"Laser target disable failed");
    require(!laser.update_laser(1./60,player.center(),player)&&commands(laser).size()==6,"Unobstructed laser invented a surface impact");
    laser.unbind_physics();player.unbind_physics();require(world.unregister_object(handle)&&world.unregister_object(player_handle),"Laser fixture cleanup failed");

    // Short beams must not create negative shaft/lightning lengths.
    LaserVisual short_beam(key,1);std::vector<core::RenderCommand> short_commands;
    short_beam.submit(short_commands,{0,0},{41.5f,45},0,std::numbers::pi_v<float>/2,true,4,true);
    require(short_commands.size()==5&&short_commands[0].command_rect.height()==4,"Short laser geometry is invalid");
    std::vector<core::RenderCommand> normal_pose,rotated_pose;
    short_beam.submit(normal_pose,{0,0},{41.5f,45},0,std::numbers::pi_v<float>/2,false,100,false);
    short_beam.submit(rotated_pose,{50,70},{41.5f,45},90,std::numbers::pi_v<float>/2,false,100,false);
    const auto pivot=anchor(normal_pose[1]);const auto rotated=anchor(rotated_pose[1]);
    require((rotated-(core::Vector2{50,70}+core::Vector2{-pivot.y,pivot.x})).length()<.001&&std::abs(rotated_pose[1].rotation_degrees-normal_pose[1].rotation_degrees-90)<.001,"Laser head does not follow carrier translation/rotation");
    short_beam.update(.1,true,true,{100,200});normal_pose.clear();rotated_pose.clear();
    short_beam.submit(normal_pose,{0,0},{41.5f,45},0,std::numbers::pi_v<float>/2,false,100,false);
    short_beam.submit(rotated_pose,{50,70},{41.5f,45},90,std::numbers::pi_v<float>/2,false,100,false);
    require(normal_pose.size()>3&&normal_pose.size()==rotated_pose.size(),"World-space laser particle fixture missing");
    for(std::size_t i=3;i<normal_pose.size();++i)require(normal_pose[i].command_rect==rotated_pose[i].command_rect,"Emitted laser particles moved with their old carrier");
    require(SDL_SetRenderDrawColor(renderer,24,36,54,255)&&SDL_RenderClear(renderer),SDL_GetError());
    for(int row=0;row<2;++row)for(int column=0;column<3;++column) {
        const float rotation=(column-1)*90.f;LaserVisual visual(key,7+column);
        const float rad=rotation*std::numbers::pi_v<float>/180;
        const core::Vector2 muzzle{start.x*std::cos(rad)-start.y*std::sin(rad),start.x*std::sin(rad)+start.y*std::cos(rad)};
        const core::Vector2 end=muzzle+core::Vector2{std::sin(rad)*100,-std::cos(rad)*100};
        if(row)visual.update(.15,true,true,end);
        std::vector<core::RenderCommand> drawn;visual.submit(drawn,{0,0},{41.5f,45},rotation,std::numbers::pi_v<float>/2,row!=0,100,true);
        const camera::Camera preview({0,0},{1280,720},1.8f);
        for(const auto& c:drawn){auto screen=core::project_render_command_to_screen(c,preview);screen.screen_rect=screen.screen_rect.translated(core::Vector2{210.f+420*column-640,180.f+360*row-360});core::require_render_success(core::execute_render_command(renderer,screen));}
    }
    auto* captured=SDL_RenderReadPixels(renderer,nullptr);require(captured!=nullptr,SDL_GetError());
    const bool saved=SDL_SaveBMP(captured,"out/qa/laser-visual-preview.bmp");SDL_DestroySurface(captured);require(saved,SDL_GetError());
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

int main(int argc,char** argv) {
    const bool level_music_only=argc==2&&std::string_view(argv[1])=="--level-music-only";
    const bool audio_only=argc==2&&std::string_view(argv[1])=="--audio-only";
    typography::FontResolver font_resolver;
    SDL_Surface* surface=nullptr;
    SDL_Renderer* renderer=nullptr;
    int result=1;
    try {
        require(io::PathManager::instance()->initialize(std::filesystem::current_path()).has_value(),"Path initialization failed");
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy");
        require(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO),SDL_GetError());
        require(TTF_Init(),SDL_GetError());
        require(MIX_Init(),SDL_GetError());
        surface=SDL_CreateSurface(1280,720,SDL_PIXELFORMAT_RGBA32);
        require(surface!=nullptr,SDL_GetError());
        renderer=SDL_CreateSoftwareRenderer(surface);
        require(renderer!=nullptr,SDL_GetError());
        const auto font_settings=typography::resolve_font_settings({});require(font_settings.has_value(),"Font settings resolution failed");
        require(builtin::BuiltinResources::instance()->initialize(renderer,builtin::BuiltinAssetCatalog(*io::PathManager::instance()),font_settings->engine_point_sizes()).has_value(),"Built-in preview fonts failed to load");
        require(localization::LocalizationManager::instance()->initialize(renderer,io::PathManager::instance()->configs()/"manifests/i18n_manifest.json","zh-Hans",&font_resolver).has_value(),"Preview localization initialization failed");
        require(font_resolver.configure(*font_settings,*ELYSIA_RESOURCES,ELYSIA_LOCALIZATION->supported_languages()).has_value(),"Preview font configuration failed");
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
            if(audio_only){
                game::testing::check_scene_audio(renderer,*registry);
                check_frontend_music(renderer,*registry);
            }else if(level_music_only){
                check_level_music_playback(renderer,*registry);
            }else{
            check_frontend_music(renderer,*registry);
            check_frontend_artwork(renderer);
            check_original_tutorial_ui(renderer);
            check_main_menu_background(renderer,*registry);
            check_portrait_pages(renderer,*registry);
            check_run_stats(renderer);
            check_dialogue_and_pause(renderer);
            check_pickups_and_moon();
            check_pickup_effects(renderer);
            check_result_star_effects(renderer);
            check_driven_rotor_pause();
            check_role_effects(renderer);
            check_thrust_particles(renderer);
            check_moon_highlight(renderer);
            check_decoration_anchors();
            save_decor_preview(renderer);
            check_platforms_and_rods();
            check_platform_spikes(renderer);
            check_tracking_flower();
            check_homing_launch_and_motion(renderer);
            check_thrower_head(renderer);
            check_laser_visuals(renderer);
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
        }
        if(!level_music_only)std::cout<<"PASS: idle/mechanism/portal/bomb visuals, ordinary projectile gravity/arcs/terrain hits, homing motion preserved\n";
        result=0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';
    }
    localization::LocalizationManager::instance()->shutdown();font_resolver.shutdown();builtin::BuiltinResources::instance()->shutdown();
    if(renderer)SDL_DestroyRenderer(renderer);
    if(surface)SDL_DestroySurface(surface);
    ELYSIA_AUDIO->shutdown();ELYSIA_SAVE->shutdown();audio::detail::mixer_backend().shutdown();
    MIX_Quit();TTF_Quit();SDL_Quit();
    return result;
}
