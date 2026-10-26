#pragma once

namespace elysia::scene { class SceneCameraRuntime; }

namespace game::development {
// Appends controls to the current ImGui window; only used in ImGui builds.
void draw_camera_controls(elysia::scene::SceneCameraRuntime& runtime, float default_zoom);
}
