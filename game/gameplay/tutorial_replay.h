#pragma once
#include "game/content/content.h"
#include <optional>
#include "engine/core/geometry/vector2.h"

namespace game::play {
// Record timing is inferred as 60 Hz from 322 held frames and the 5.3 s script.
// The original 0.166667 header is retained in data; its meaning is unconfirmed.
class TutorialReplay {
public:
    void load(const content::Json& record) {_frames=record.at("frames");_cursor=0;}
    void skip_idle(){while(_cursor<_frames.size()&&_frames[_cursor].is_null())++_cursor;}
    std::optional<elysia::core::Vector2> next(float world_height) {
        if(_cursor>=_frames.size())return std::nullopt;
        const auto& frame=_frames[_cursor++];
        if(frame.is_null())return std::nullopt;
        return elysia::core::Vector2{frame[0],world_height-frame[1].get<float>()};
    }
    size_t cursor()const{return _cursor;}
    bool ended()const{return _cursor>=_frames.size();}
private:
    content::Json _frames=content::Json::array();
    size_t _cursor=0;
};
}
