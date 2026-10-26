#include "engine/tools/development_overlay_host.h"
#include "engine/tools/imgui/imgui_development_overlay.h"
#include "engine/tools/debug_draw.h"
#include "engine/core/time.h"
#include "engine/scene/runtime/scene_camera_runtime.h"
#include "game/development/camera_controls.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void check_overlay(SDL_Window& window, SDL_Renderer& renderer)
{
    using namespace elysia::tools;
    DevelopmentOverlayHost host;
    host.set_overlay(std::make_unique<ImGuiDevelopmentOverlay>());
    const auto initialized = host.initialize(window, renderer);
    require(initialized.has_value(), "Overlay initialization failed");
    using elysia::camera::CameraSlot;
    elysia::scene::SceneCameraRuntime camera_runtime({});
    elysia::camera::CameraManager::instance()->set_viewport_size(CameraSlot::Main, {1280, 720});
    camera_runtime.set_zoom(CameraSlot::Main, 1.5f);
    camera_runtime.set_follow_strategy(CameraSlot::Main,
        std::make_unique<elysia::camera::HardFollowStrategy>());
    camera_runtime.set_focus(CameraSlot::Main,
        elysia::camera::CameraFocus{{2000, 2000, 32, 32}, {2000, 2000, 32, 32}});
    int callback_count = 0;
    const auto panel = host.panel_registry()->register_panel("test.lifecycle", [&] {
        ++callback_count;
        if (ImGui::Begin(ImGuiDevelopmentOverlay::MainPanelTitle))
            game::development::draw_camera_controls(camera_runtime, 1.5f);
        ImGui::End();
    });
    require(panel.is_valid(), "Panel registration failed");

    auto frame = [&] {
        elysia::core::Time::instance()->begin_frame(1.0 / 60.0);
        (void)camera_runtime.advance(elysia::core::Time::instance()->delta());
        require(SDL_SetRenderDrawColor(&renderer, 15, 23, 36, 255)
            && SDL_RenderClear(&renderer), SDL_GetError());
        host.begin_frame(elysia::core::Time::instance()->raw_delta());
        host.render(renderer);
    };
    auto toggle = [&](bool repeat = false, Uint32 type = SDL_EVENT_KEY_DOWN) {
        SDL_Event event{};
        event.type = type;
        event.key.key = SDLK_F2;
        event.key.repeat = repeat;
        require(host.process_event(event), "F2 must be consumed by the host");
    };
    auto activate = [&](const char* label) {
        auto* panel_window = ImGui::FindWindowByName("Development Panel (F2)");
        require(panel_window && panel_window->Active, "Development panel is missing");
        ImGui::ActivateItemByID(panel_window->GetID(label));
        frame();
    };

    const int initial_frame = ImGui::GetFrameCount();
    frame();
    require(!host.visible() && callback_count == 0
        && ImGui::GetFrameCount() == initial_frame, "Hidden overlay should be idle by default");
    toggle(true);
    toggle(false, SDL_EVENT_KEY_UP);
    require(!host.visible(), "Key repeats and releases must not toggle the panel");
    toggle();
    frame();
    frame();
    require(host.visible() && callback_count == 2, "F2 must show the panel and registered callbacks");
    activate("2x");
    require(camera_runtime.presented_camera().zoom() == 2.0f
        && camera_runtime.presented_camera().world_viewport_size().x == 640.0f,
        "Zoom must update the live camera and its world viewport");
    frame();
    require(camera_runtime.presented_camera().zoom() == 2.0f,
        "Camera follow must preserve the developer zoom");
    activate("0.5x");
    require(camera_runtime.presented_camera().zoom() == 0.5f, "Zoom-out preset failed");
    activate("Reset zoom");
    require(camera_runtime.presented_camera().zoom() == 1.5f, "Reset must restore the level's 1.5x zoom");

    activate("Enable debug draw");
    require(ELYSIA_DEBUG_DRAW->enabled(), "Debug checkbox did not enable DebugDraw");
    activate("None");
    require(ELYSIA_DEBUG_DRAW->enabled_categories() == DebugDrawCategory::None, "None must disable all categories");
    activate("All");
    require(ELYSIA_DEBUG_DRAW->enabled_categories() == DebugDrawCategory::All, "All must enable every category");
    activate("Defaults");
    require(ELYSIA_DEBUG_DRAW->enabled_categories() == DebugDrawCategory::Default, "Defaults must restore category defaults");
    activate("Show FPS overlay");
    activate("Pause");
    require(ELYSIA_TIME->time_scale() == 0.0, "Pause must stop simulation time");
    frame();
    require(ELYSIA_TIME->delta() == 0.0 && ImGui::GetIO().DeltaTime > 0.0f,
        "ImGui must keep using real frame time while simulation is paused");

    // Exercise the same scaled/letterboxed renderer configuration as the game.
    require(SDL_SetRenderLogicalPresentation(&renderer, 640, 360, SDL_LOGICAL_PRESENTATION_LETTERBOX)
        && SDL_SetRenderScale(&renderer, 1.25f, 1.5f), SDL_GetError());
    const SDL_Rect clip{3, 4, 110, 120};
    require(SDL_SetRenderClipRect(&renderer, &clip), SDL_GetError());
    frame();
    float scale_x = 0.0f, scale_y = 0.0f;
    int logical_width = 0, logical_height = 0;
    SDL_RendererLogicalPresentation mode{};
    SDL_Rect restored_clip{};
    require(SDL_GetRenderScale(&renderer, &scale_x, &scale_y)
        && SDL_GetRenderLogicalPresentation(&renderer, &logical_width, &logical_height, &mode)
        && SDL_GetRenderClipRect(&renderer, &restored_clip), SDL_GetError());
    require(scale_x == 1.25f && scale_y == 1.5f && logical_width == 640 && logical_height == 360
        && mode == SDL_LOGICAL_PRESENTATION_LETTERBOX && SDL_RenderClipEnabled(&renderer)
        && restored_clip.x == clip.x && restored_clip.y == clip.y
        && restored_clip.w == clip.w && restored_clip.h == clip.h, "Overlay leaked renderer state");
    require(SDL_SetRenderLogicalPresentation(&renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED)
        && SDL_SetRenderScale(&renderer, 1.0f, 1.0f) && SDL_SetRenderClipRect(&renderer, nullptr), SDL_GetError());

    std::filesystem::create_directories("out/qa/development-panel");
    auto save_preview = [&](const char* path) {
        auto* surface = SDL_RenderReadPixels(&renderer, nullptr);
        require(surface != nullptr, SDL_GetError());
        int hud_pixels = 0;
        for (int y = 10; y < 50; ++y)
            for (int x = 1100; x < 1270; ++x)
            {
                Uint8 red = 0, green = 0, blue = 0, alpha = 0;
                require(SDL_ReadSurfacePixel(surface, x, y, &red, &green, &blue, &alpha), SDL_GetError());
                if (red != 15 || green != 23 || blue != 36)
                    ++hud_pixels;
            }
        require(hud_pixels > 100, "FPS HUD produced no visible pixels");
        const bool saved = SDL_SaveBMP(surface, path);
        SDL_DestroySurface(surface);
        require(saved, SDL_GetError());
    };
    frame();
    save_preview("out/qa/development-panel/panel.bmp");
    const int callbacks_before_hiding = callback_count;
    toggle();
    frame();
    frame();
    require(!host.visible() && callback_count == callbacks_before_hiding,
        "Hidden panels must not invoke scene callbacks");
    require(!ImGui::FindWindowByName("Development Panel (F2)")->Active
        && ImGui::FindWindowByName("FPS##development-hud")->Active
        && ImGui::GetDrawData()->TotalVtxCount > 0, "FPS HUD must remain visible after hiding panels");
    require(host.captured_input() == elysia::input::InputCapture::None,
        "Background FPS HUD must not capture gameplay input");
    save_preview("out/qa/development-panel/fps-only.bmp");

    toggle();
    frame();
    activate("Normal speed");
    require(ELYSIA_TIME->time_scale() == 1.0, "Simulation must resume from the panel");
    activate("Show FPS overlay");
    toggle();
    const int last_frame = ImGui::GetFrameCount();
    frame();
    require(ImGui::GetFrameCount() == last_frame, "Hiding with FPS disabled must stop ImGui frames");
    require(host.panel_registry()->unregister_panel(panel), "Panel cleanup failed");
    host.shutdown();
    ELYSIA_DEBUG_DRAW->set_enabled(false);
    ELYSIA_DEBUG_DRAW->set_enabled_categories(DebugDrawCategory::Default);
    ELYSIA_TIME->reset();
}
}

int main()
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    int result = 0;
    try
    {
        require(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
        window = SDL_CreateWindow("Overlay integration test", 1280, 720, SDL_WINDOW_HIDDEN);
        require(window != nullptr, SDL_GetError());
        renderer = SDL_CreateRenderer(window, "software");
        require(renderer != nullptr, SDL_GetError());
        check_overlay(*window, *renderer);
        std::cout << "Development overlay integration PASS\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
