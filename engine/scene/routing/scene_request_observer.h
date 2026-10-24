#pragma once
#include "scene_request.h"

namespace elysia::scene
{
class SceneRequestObserver
{
public:
    virtual ~SceneRequestObserver() = default;

    virtual void on_scene_request(const SceneRequest& request) = 0;
};
}
