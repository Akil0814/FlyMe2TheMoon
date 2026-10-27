#include "engine/input/touch_pointer.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace elysia::input;
int main() {
    TouchPointer pointer;
    const auto sample = [&](int id, TouchPhase phase, float x=640, float y=360) {
        return pointer.process(1,id,phase,x,y,1280,720);
    };
    assert(sample(1,TouchPhase::Down)->action == PointerAction::Press);
    assert(!sample(2,TouchPhase::Down));
    assert(!sample(2,TouchPhase::Up));
    assert(sample(1,TouchPhase::Move,1200)->x == 1200);
    assert(sample(1,TouchPhase::Up)->action == PointerAction::Release);
    assert(!pointer.active());
    assert(!sample(1,TouchPhase::Down,-1));
    assert(!sample(1,TouchPhase::Move));
    assert(!sample(1,TouchPhase::Up));
    sample(1,TouchPhase::Down);
    assert(sample(1,TouchPhase::Move,1280)->action == PointerAction::Cancel);
    assert(!sample(1,TouchPhase::Move));
    assert(!sample(1,TouchPhase::Up));
    sample(1,TouchPhase::Down);
    assert(sample(1,TouchPhase::Cancel)->action == PointerAction::Cancel);
    sample(1,TouchPhase::Down);
    pointer.reset();
    assert(!sample(1,TouchPhase::Move));
    assert(!sample(1,TouchPhase::Up));
    assert(sample(2,TouchPhase::Down)->action == PointerAction::Press);
    assert(sample(2,TouchPhase::Move,std::numeric_limits<float>::quiet_NaN())->action == PointerAction::Cancel);
    std::cout << "TouchPointer: ownership, viewport, release and cancellation passed\n";
}
