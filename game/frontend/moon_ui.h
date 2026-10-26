#pragma once
#include "engine/ui/core/ui_element.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/widgets/text/ui_text_block.h"
#include "engine/ui/containers/ui_grid_container.h"
#include <memory>
#include <algorithm>
#include <optional>

namespace game::frontend {
class StarRating final:public elysia::ui::UiElement {
public:
    explicit StarRating(elysia::core::Rect rect,int order=0):UiElement(rect,order){}
    void set_collected(int count){_collected=std::clamp(count,0,3);}
    int collected()const{return _collected;}
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
private:
    int _collected=0;
};
class MoonPanel final:public elysia::ui::UiElement {
public:
    explicit MoonPanel(elysia::core::Rect rect,int order=0):UiElement(rect,order){}
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
};
class PauseIconButton final:public elysia::ui::UiButton {
public:
    explicit PauseIconButton(elysia::core::Rect rect,std::function<void()> action);
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
};
class MenuArtwork final:public elysia::ui::UiElement {
public:
    MenuArtwork();
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
private:
    SDL_Texture* _background=nullptr;
    SDL_Texture* _title=nullptr;
};
// Artwork is supplied by the project; all interactive controls remain engine UI.
class MoonBackdrop final:public elysia::ui::UiElement {
public:
    MoonBackdrop(const char* name,int shade=0);
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
class LevelNode final:public elysia::ui::UiButton {
public:
    LevelNode(int id,int chapter,int stars,bool open,std::function<void()> action,
        std::optional<int> best_score=std::nullopt);
    [[nodiscard]] std::optional<int> best_score()const{return _best_score;}
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
private:
    int _chapter=0,_stars=-1;
    bool _open=false;
    std::optional<int> _best_score;
    mutable elysia::ui::UiLabel _score_label;
};
class ConstellationGrid final:public elysia::ui::UiGridContainer {
public:
    explicit ConstellationGrid(const elysia::core::Rect& rect);
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
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
