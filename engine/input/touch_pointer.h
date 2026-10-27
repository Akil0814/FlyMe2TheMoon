#pragma once
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

namespace elysia::input {
enum class TouchPhase { Down, Move, Up, Cancel };
enum class PointerAction { Press, Move, Release, Cancel };
struct TouchPoint { PointerAction action; float x, y; };

// Own exactly one contact. A contact that leaves the viewport cannot re-enter
// as a new press; it must lift before another down can take ownership.
class TouchPointer {
public:
    std::optional<TouchPoint> process(std::int64_t device, std::int64_t finger,
        TouchPhase phase, float x, float y, float width, float height) {
        const auto id = std::pair{device,finger};
        const bool inside = std::isfinite(x) && std::isfinite(y) && x >= 0 && y >= 0 && x < width && y < height;
        if (phase == TouchPhase::Down) {
            if (_primary) return {};
            _primary = id;
            _pressed = inside;
            return inside ? std::optional<TouchPoint>{{PointerAction::Press,x,y}} : std::nullopt;
        }
        if (!_primary || *_primary != id) return {};
        if (phase == TouchPhase::Up || phase == TouchPhase::Cancel) {
            _primary.reset();
            if (!std::exchange(_pressed,false)) return {};
            return TouchPoint{phase == TouchPhase::Up && inside ? PointerAction::Release : PointerAction::Cancel,x,y};
        }
        if (!_pressed) return {};
        if (!inside) { _pressed = false; return TouchPoint{PointerAction::Cancel,x,y}; }
        return TouchPoint{PointerAction::Move,x,y};
    }
    void reset() { _primary.reset(); _pressed = false; }
    bool active() const { return _primary.has_value(); }
private:
    std::optional<std::pair<std::int64_t,std::int64_t>> _primary;
    bool _pressed = false;
};
}
