#pragma once
#include "engine/ui/widgets/text/ui_text_block.h"
#include <algorithm>

namespace game::frontend {
// Color-code parsing and reveal state belong to the game, not the Lua scheduler.
class StoryText final:public elysia::ui::UiTextBlock {
public:
    using UiTextBlock::UiTextBlock;
    void set_story_text(const std::string& source);
    void advance(double delta){_time+=std::max(0.,delta);}
    bool complete()const{return _revealed||visible_count()>=_glyphs.size();}
    void reveal(){_revealed=true;}
    std::size_t visible_count()const{return _revealed?_glyphs.size():std::min(_glyphs.size(),static_cast<std::size_t>(_time*60+1e-7));}
    std::size_t glyph_count()const{return _glyphs.size();}
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
private:
    struct Glyph {std::string text;elysia::core::Color color;};
    std::vector<Glyph> _glyphs;
    double _time=0;
    bool _revealed=false;
};
}
