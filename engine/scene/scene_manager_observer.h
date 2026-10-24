#pragma once

#include "scene_boundary_failure.h"

namespace elysia::scene
{
class SceneManagerObserver
{
public:
    virtual ~SceneManagerObserver() = default;

    virtual void on_scene_manager_quit_requested() = 0;
    virtual void on_scene_manager_fault(const SceneBoundaryFailure& failure) = 0;
};

}
