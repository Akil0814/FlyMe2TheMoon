#if ELYSIA_ENABLE_IMGUI
#include "camera_controls.h"
#include "engine/scene/runtime/scene_camera_runtime.h"
#include <imgui.h>

namespace game::development {
void draw_camera_controls(elysia::scene::SceneCameraRuntime& runtime, float default_zoom)
{
    using elysia::camera::Camera;
    const auto slot = runtime.presented_slot();
    const auto& camera = runtime.slot_camera(slot);
    ImGui::SeparatorText("Camera");
    float zoom = camera.zoom();
    if (ImGui::SliderFloat("Zoom", &zoom, Camera::kMinZoom, Camera::kMaxZoom, "%.2fx",
            ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp))
        runtime.set_zoom(slot, zoom);
    if (ImGui::Button("0.5x"))
        runtime.set_zoom(slot, 0.5f);
    ImGui::SameLine();
    if (ImGui::Button("1x"))
        runtime.set_zoom(slot, 1.0f);
    ImGui::SameLine();
    if (ImGui::Button("2x"))
        runtime.set_zoom(slot, 2.0f);
    ImGui::SameLine();
    if (ImGui::Button("Reset zoom"))
        runtime.set_zoom(slot, default_zoom);
    const auto size = camera.world_viewport_size();
    ImGui::TextDisabled("World view: %.0f x %.0f", size.x, size.y);
}
}
#endif
