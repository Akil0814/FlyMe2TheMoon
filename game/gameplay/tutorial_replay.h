#pragma once
#include "game/content/content.h"
#include <optional>
#include "engine/core/geometry/vector2.h"

namespace game::play {
// AppDelegate sets the director to 1/60 s (double at 0x7938c), and GameRecorder
// advances once per game update. Retain the unrelated 0.166667 file header.
class TutorialReplay {
public:
    void load(const content::Json& record) {_frames=record.at("frames");_cursor=0;_point.reset();_pressed=false;}
    void skip_idle(){while(_cursor<_frames.size()&&_frames[_cursor].is_null())++_cursor;}
    std::optional<elysia::core::Vector2> next(float world_height) {
        _pressed=false;
        if(_cursor>=_frames.size())return std::nullopt;
        const auto& frame=_frames[_cursor++];
        if(frame.is_null())return std::nullopt;
        _pressed=true;_point=elysia::core::Vector2{frame[0],world_height-frame[1].get<float>()};return _point;
    }
    size_t cursor()const{return _cursor;}
    bool ended()const{return _cursor>=_frames.size();}
    bool pressed()const{return _pressed;}
    const std::optional<elysia::core::Vector2>& last_point()const{return _point;}
private:
    content::Json _frames=content::Json::array();
    size_t _cursor=0;
    bool _pressed=false;
    std::optional<elysia::core::Vector2> _point;
};
}
