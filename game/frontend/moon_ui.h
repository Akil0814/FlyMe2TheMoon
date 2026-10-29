#pragma once
#include "engine/core/interface/updatable.h"
#include "engine/ui/core/ui_element.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/widgets/text/ui_text_block.h"
#include "engine/ui/containers/ui_grid_container.h"
#include <memory>
#include <algorithm>
#include <optional>
#include <array>

namespace elysia::scene {class SceneRuntimeContext;}
namespace game::frontend {
class StarRating final:public elysia::ui::UiElement {
public:
    explicit StarRating(elysia::core::Rect rect,int order=0):UiElement(rect,order){}
    void set_collected(int count){_collected=std::clamp(count,0,3);_result_count=0;_result_elapsed=-1;}
    void present_result(int count,double elapsed);
    int collected()const{return _collected;}
    elysia::core::Rect slot_rect(int index)const;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
private:
    int _collected=0;
    int _result_count=0;
    double _result_elapsed=-1;
};
class HudLabel final:public elysia::ui::UiLabel {
public:
    explicit HudLabel(elysia::core::Rect rect,int order=0,elysia::ui::UiTextContent text={});
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
};
class HudPanel final:public elysia::ui::UiElement {
public:
    explicit HudPanel(elysia::core::Rect rect,int order=0):UiElement(rect,order){}
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
};
class RunStats final:public elysia::ui::UiElement {
public:
    explicit RunStats(elysia::core::Rect rect,int order=0);
    void set_values(double seconds,int large_crystals,int small_crystals);
    int large_crystals()const{return _large;}
    int small_crystals()const{return _small;}
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
private:
    int _large=0,_small=0;
    mutable std::array<elysia::ui::UiLabel,3> _labels;
};
class PauseIconButton final:public elysia::ui::UiButton {
public:
    explicit PauseIconButton(elysia::core::Rect rect,std::function<void()> action);
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
};
// Artwork is supplied by the project; all interactive controls remain engine UI.
class MoonBackdrop final:public elysia::ui::UiElement {
public:
    MoonBackdrop(const elysia::scene::SceneRuntimeContext& context,const char* name,int shade=0);
    MoonBackdrop(elysia::core::Rect viewport,const char* name,int shade=0);
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
private:
    SDL_Texture* _texture=nullptr;
    int _shade=0;
};
class GlassButton final:public elysia::ui::UiButton {
public:
    GlassButton(std::string text,float width,float height,std::function<void()> action);
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
};
std::unique_ptr<elysia::ui::UiLabel> label(std::string text,float width,float height,bool centered=false);
void mark_selected(elysia::ui::UiButton& button);
class LevelNode final:public elysia::ui::UiButton,public elysia::core::Updatable {
public:
    LevelNode(int id,int stars,bool open,std::function<void()> action,
        std::optional<int> best_score=std::nullopt);
    [[nodiscard]] std::optional<int> best_score()const{return _best_score;}
    bool unlocked()const{return _open;}
    void show_unlock(){_unlock_time=.6;}
    void update(double delta)override{_unlock_time=std::max(0.,_unlock_time-std::max(0.,delta));}
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
private:
    std::string _color;
    int _stars=-1;
    bool _open=false;
    std::optional<int> _best_score;
    mutable elysia::ui::UiLabel _score_label;
    double _unlock_time=0;
};
class ConstellationGrid final:public elysia::ui::UiGridContainer {
public:
    explicit ConstellationGrid(const elysia::core::Rect& rect);
    bool on_ui_input_event(const elysia::ui::UiInputEvent& event)override;
    void cancel_input_interaction()noexcept override{_locked_pressed=nullptr;UiGridContainer::cancel_input_interaction();}
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
private:
    LevelNode* _locked_pressed=nullptr;
};
class AchievementCard final:public elysia::ui::UiElement {
public:
    AchievementCard(int id,std::string title,std::string description,bool earned);
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
private:
    SDL_Texture* _badge=nullptr;
    bool _earned=false;
    mutable elysia::ui::UiLabel _title,_status;
    mutable elysia::ui::UiTextBlock _description;
};
}
