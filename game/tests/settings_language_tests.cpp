#define SDL_MAIN_HANDLED

#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/builtin/resources/builtin_resources.h"
#include "engine/core/render/sdl_render_command_executor.h"
#include "engine/io/path/path_manager.h"
#include "engine/localization/locale.h"
#include "engine/localization/localization_manager.h"
#include "engine/localization/localization_service.h"
#include "engine/resources/resource_service.h"
#include "engine/typography/font_resolver.h"
#include "engine/ui/composites/ui_dropdown.h"
#include "engine/ui/presets/settings_panel.h"
#include "engine/ui/style/ui_style_defaults.h"
#include "engine/ui/window/ui_window.h"

#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
using namespace elysia;

void require(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
}

class Fixture {
public:
    Fixture() {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy");
        require(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO),SDL_GetError());
        require(TTF_Init(),SDL_GetError());
        require(MIX_Init(),SDL_GetError());
        surface=SDL_CreateSurface(800,600,SDL_PIXELFORMAT_RGBA32);
        require(surface!=nullptr,SDL_GetError());
        renderer=SDL_CreateSoftwareRenderer(surface);
        require(renderer!=nullptr,SDL_GetError());
        auto* paths=io::PathManager::instance();
        require(paths->initialize(std::filesystem::current_path()).has_value(),"Project paths must initialize");
        typography::UiTypographyProfile::PointSizes sizes{};sizes.fill(20);
        typography::FontSettings requested;
        requested.ui.typography_override=typography::UiTypographyProfile(sizes);
        const auto settings=typography::resolve_font_settings(requested);
        require(settings.has_value(),"Font settings must resolve");
        require(builtin::BuiltinResources::instance()->initialize(renderer,
            builtin::BuiltinAssetCatalog(*paths),settings->engine_point_sizes()).has_value(),"Built-in fonts must load");
        require(localization::LocalizationManager::instance()->initialize(renderer,
            paths->configs()/"manifests/i18n_manifest.json","en",&fonts).has_value(),"Localization must initialize");
        require(fonts.configure(*settings,*ELYSIA_RESOURCES,
            ELYSIA_LOCALIZATION->supported_languages()).has_value(),"Font resolver must configure");
    }
    ~Fixture() {
        localization::LocalizationManager::instance()->shutdown();fonts.shutdown();
        builtin::BuiltinResources::instance()->shutdown();
        if(renderer)SDL_DestroyRenderer(renderer);
        if(surface)SDL_DestroySurface(surface);
        MIX_Quit();TTF_Quit();SDL_Quit();
    }
    SDL_Surface* surface=nullptr;
    SDL_Renderer* renderer=nullptr;
    typography::FontResolver fonts;
};

ui::UiDropdown* find_dropdown(ui::UiChildHost& host) {
    for(std::size_t i=0;i<host.child_count();++i){
        if(auto* dropdown=dynamic_cast<ui::UiDropdown*>(host.child_at(i)))return dropdown;
        if(auto* child=dynamic_cast<ui::UiChildHost*>(host.child_at(i)))
            if(auto* found=find_dropdown(*child))return found;
    }
    return nullptr;
}

bool contains_texture(const std::vector<core::UiRenderCommand>& commands,SDL_Texture* texture) {
    return texture&&std::ranges::any_of(commands,[texture](const auto& command){return command.texture==texture;});
}

void check_languages(Fixture& fixture) {
    auto* localization=ELYSIA_LOCALIZATION;
    auto* resources=builtin::BuiltinResources::instance();
    const auto languages=localization->supported_languages();
    require(languages.size()==5,"All five existing languages must remain supported");
    const std::array<std::pair<std::string,std::string>,5> names{{
        {"en","English"},{"ja","日本語"},{"ko","한국어"},{"zh-Hans","简体中文"},{"zh-Hant","繁體中文"}}};
    ui::SettingsPanel panel(core::Rect{0,0,700,500},{
        .window_mode=false,.target_fps=false,.vsync=false,
        .master_volume=false,.music_volume=false,.sound_volume=false,.language=true});
    auto* scroll=dynamic_cast<ui::UiScrollContainer*>(panel.child_at(1));
    require(scroll!=nullptr,"Settings must have scroll content");
    auto* host=dynamic_cast<ui::UiChildHost*>(const_cast<ui::UiElement*>(scroll->content()));
    require(host!=nullptr,"Settings must have child content");
    auto* dropdown=find_dropdown(*host);
    require(dropdown!=nullptr,"Settings must have a language dropdown");
    ui::UiWindow window(core::Rect{0,0,800,600});dropdown->register_with_window(window);
    for(const auto& current:languages){
        require(localization->set_language(current).has_value(),"Each UI language must load");
        panel.set_options({.languages=languages});
        dropdown->set_screen_rect(core::Rect{20,20,300,44});
        require(dropdown->options().size()==languages.size(),"Every language must remain listed");
        std::vector<SDL_Texture*> expected;
        for(std::size_t i=0;i<languages.size();++i){
            const auto& language=languages[i];const auto& option=dropdown->options()[i];
            const auto name=std::ranges::find_if(names,[&](const auto& pair){return pair.first==language;});
            require(name!=names.end(),"Language fixture must define each native name");
            require(option.content.kind==ui::UiTextContentKind::TextKey
                &&option.content.value=="engine.settings.languages."+std::string(localization::locale_key_segment(language)),
                "Existing language translation keys must remain in use");
            require(localization->tr(option.content.value)==name->second,"Native names must stay unchanged in every UI language");
            require(option.font_language_override==language
                &&option.font_source_override==typography::FontSource::EngineBuiltIn,"Every option must use its own Engine font");
            auto* font=resources->find_font(builtin::builtin_font_id(builtin::builtin_locale_id(language)),20);
            require(font!=nullptr,"Native-name font must exist");
            const char* cursor=name->second.c_str();
            while(*cursor)require(TTF_FontHasGlyph(font,SDL_StepUTF8(&cursor,nullptr)),"Every native-name glyph must exist");
            localization::LocalizedTextStyle style;
            style.typography_role=typography::UiTypographyRole::Button;
            style.color=ui::UiStyleDefaults::button().text.enabled;
            style.font_language_override=language;style.font_source_override=typography::FontSource::EngineBuiltIn;
            auto* texture=localization->get_text_texture(option.content.value,style);
            require(texture!=nullptr,"Native name must render");expected.push_back(texture);
            require(dropdown->set_selected_index(i),"Each language must be selectable");
            std::vector<core::UiRenderCommand> trigger;dropdown->submit_ui_render_commands(trigger);
            require(contains_texture(trigger,texture),"Selected trigger must use the option's font");
            int width=0,height=0,font_width=0,font_height=0;
            require(localization->measure_raw_text(name->second,style,width,height)
                &&TTF_GetStringSize(font,name->second.c_str(),0,&font_width,&font_height)
                &&width==font_width&&height==font_height,"Measurement and rendering must use the same language font");
        }
        dropdown->open();require(dropdown->is_open(),"Language popup must open");
        std::vector<core::UiRenderCommand> popup;dropdown->submit_popup_render_commands(popup);
        for(auto* texture:expected)require(contains_texture(popup,texture),"Every popup row must use its own language font");
        require(localization->current_language()==current,"Rendering must not change the global language");
        if(const char* directory=SDL_getenv("FLY_SETTINGS_LANGUAGE_QA_DIR")){
            std::filesystem::create_directories(directory);
            SDL_SetRenderDrawColor(fixture.renderer,22,26,36,255);
            require(SDL_RenderClear(fixture.renderer),"QA canvas must clear");
            dropdown->submit_ui_render_commands(popup);
            require(core::execute_render_commands(fixture.renderer,popup).has_value(),"QA drawing must succeed");
            require(SDL_RenderPresent(fixture.renderer),"QA canvas must present before capture");
            require(IMG_SavePNG(fixture.surface,(std::filesystem::path(directory)/(current+".png")).string().c_str()),"QA image must save");
        }
        dropdown->close();dropdown->open();require(dropdown->is_open(),"Popup must reopen after selection");
        dropdown->clear_options();
        std::vector<core::UiRenderCommand> commands;dropdown->submit_ui_render_commands(commands);
        require(!dropdown->is_open()&&!dropdown->selected_index()
            &&std::ranges::none_of(commands,[](const auto& command){return command.texture!=nullptr;}),"Clearing must remove selection and text");
        dropdown->set_options({{ui::ui_raw_text("Moon")}});
        localization::LocalizedTextStyle plain;plain.typography_role=typography::UiTypographyRole::Button;
        plain.color=ui::UiStyleDefaults::button().text.enabled;
        commands.clear();dropdown->submit_ui_render_commands(commands);
        require(contains_texture(commands,localization->get_raw_text_texture("Moon",plain)),"Ordinary options must clear old font overrides");
        auto overridden=plain;overridden.font_language_override="ko";
        auto* overridden_raw=localization->get_raw_text_texture("Moon",overridden);
        require(overridden_raw&&overridden_raw!=localization->get_raw_text_texture("Moon",plain),
            "Raw cache must distinguish overridden and inherited font settings");
        auto other=overridden;other.font_language_override="ja";
        require(localization->get_raw_text_texture("Moon",overridden)!=localization->get_raw_text_texture("Moon",other)
            &&localization->get_text_texture("engine.settings.title",overridden)!=localization->get_text_texture("engine.settings.title",other),
            "Both raw and translated caches must distinguish font languages");
    }
    panel.set_options({.languages={"unknown-locale"}});
    const auto& unknown=dropdown->options().front();
    require(unknown.content.kind==ui::UiTextContentKind::RawText&&unknown.content.value=="unknown-locale"
        &&!unknown.font_language_override&&!unknown.font_source_override,"Unknown language must retain its identifier without an invalid font mapping");
    ui::UiButton button;button.set_font_language_override("ja");button.reset();
    require(!button.font_language_override(),"Button reset must clear font language overrides");
    std::cout<<"PASS: five UI languages, native glyphs, popup/trigger fonts, selection lifecycle and cache isolation\n";
}
}

int main() {
    try{Fixture fixture;check_languages(fixture);return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
