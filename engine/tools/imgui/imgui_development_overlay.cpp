#include "imgui_development_overlay.h"

#include "../logger.h"
#include "../debug_draw.h"
#include "../../core/time.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace elysia::tools
{
namespace
{
struct SdlRendererState
{
    SDL_Texture* target = nullptr;
    int logical_width = 0;
    int logical_height = 0;
    SDL_RendererLogicalPresentation presentation = SDL_LOGICAL_PRESENTATION_DISABLED;
    SDL_Rect viewport{};
    bool viewport_enabled = false;
    SDL_Rect clip{};
    bool clip_enabled = false;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    Uint8 red = 0;
    Uint8 green = 0;
    Uint8 blue = 0;
    Uint8 alpha = 0;
    SDL_BlendMode blend_mode = SDL_BLENDMODE_NONE;
};

[[nodiscard]] SdlRendererState capture_renderer_state(
    SDL_Renderer& renderer) noexcept
{
    SdlRendererState state;
    state.target = SDL_GetRenderTarget(&renderer);
    SDL_GetRenderLogicalPresentation(&renderer,&state.logical_width,&state.logical_height,&state.presentation);
    SDL_GetRenderViewport(&renderer, &state.viewport);
    state.viewport_enabled = SDL_RenderViewportSet(&renderer);
    state.clip_enabled = SDL_RenderClipEnabled(&renderer) == true;
    if (state.clip_enabled)
        SDL_GetRenderClipRect(&renderer, &state.clip);
    SDL_GetRenderScale(&renderer, &state.scale_x, &state.scale_y);
    SDL_GetRenderDrawColor(
        &renderer,
        &state.red,
        &state.green,
        &state.blue,
        &state.alpha);
    SDL_GetRenderDrawBlendMode(&renderer, &state.blend_mode);
    return state;
}

void restore_renderer_state(
    SDL_Renderer& renderer,
    const SdlRendererState& state) noexcept
{
    (void)SDL_SetRenderTarget(&renderer, state.target);
    (void)SDL_SetRenderLogicalPresentation(&renderer, state.logical_width, state.logical_height, state.presentation);
    (void)SDL_SetRenderScale(&renderer, state.scale_x, state.scale_y);
    SDL_SetRenderViewport(&renderer, state.viewport_enabled ? &state.viewport : nullptr);
    if (state.clip_enabled)
        SDL_SetRenderClipRect(&renderer, &state.clip);
    else
        SDL_SetRenderClipRect(&renderer, nullptr);
    (void)SDL_SetRenderDrawColor(
        &renderer,
        state.red,
        state.green,
        state.blue,
        state.alpha);
    (void)SDL_SetRenderDrawBlendMode(&renderer, state.blend_mode);
}
}

ImGuiDevelopmentOverlay::~ImGuiDevelopmentOverlay()
{
    shutdown();
}

std::expected<void, std::string> ImGuiDevelopmentOverlay::initialize(
    SDL_Window& window,
    SDL_Renderer& renderer)
{
    if (_context)
        return {};

    IMGUI_CHECKVERSION();
    _context = ImGui::CreateContext();
    if (!_context)
        return std::unexpected("Dear ImGui context creation failed.");
    set_current_context();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForSDLRenderer(&window, &renderer))
    {
        shutdown();
        return std::unexpected(
            "Dear ImGui SDL3 platform backend initialization failed.");
    }
    _sdl_platform_initialized = true;

    if (!ImGui_ImplSDLRenderer3_Init(&renderer))
    {
        shutdown();
        return std::unexpected(
            "Dear ImGui SDLRenderer3 backend initialization failed.");
    }
    _sdl_renderer_initialized = true;
    return {};
}

void ImGuiDevelopmentOverlay::process_event(const SDL_Event& event)
{
    if (!_context || !_sdl_platform_initialized)
        return;
    set_current_context();
    (void)ImGui_ImplSDL3_ProcessEvent(&event);
}

void ImGuiDevelopmentOverlay::begin_frame(double delta_seconds)
{
    if (!_context || !_sdl_platform_initialized
        || !_sdl_renderer_initialized || _frame_started)
    {
        return;
    }

    set_current_context();
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    if (std::isfinite(delta_seconds) && delta_seconds > 0.0)
    {
        ImGui::GetIO().DeltaTime = static_cast<float>(delta_seconds);
        _frame_times[_frame_time_offset] = static_cast<float>(delta_seconds * 1000.0);
        _frame_time_offset = (_frame_time_offset + 1) % static_cast<int>(_frame_times.size());
        _frame_time_count = std::min(_frame_time_count + 1, static_cast<int>(_frame_times.size()));
    }
    ImGui::NewFrame();
    _frame_started = true;
}

void ImGuiDevelopmentOverlay::render(SDL_Renderer& renderer)
{
    if (!_context || !_frame_started)
        return;
    set_current_context();

    const std::vector<PanelEntry> panel_snapshot = _panels_visible
        ? _panels : std::vector<PanelEntry>{};
    _drawing_panels = true;
    try
    {
        if (_panels_visible)
            draw_development_panel();
        for (const PanelEntry& panel : panel_snapshot)
        {
            if (panel.draw)
                panel.draw();
        }
        _drawing_panels = false;
        apply_pending_operations();
    }
    catch (...)
    {
        _drawing_panels = false;
        apply_pending_operations();
        _frame_started = false;
        throw;
    }

    if (_show_fps)
        draw_fps_hud();

    ImGui::Render();
    const ImGuiIO& io = ImGui::GetIO();
    _captured_input = elysia::input::InputCapture::None;
    if (_panels_visible && (io.WantCaptureKeyboard || io.WantTextInput))
    {
        _captured_input = _captured_input
            | elysia::input::InputCapture::Keyboard;
    }
    if (_panels_visible && io.WantCaptureMouse)
    {
        _captured_input = _captured_input
            | elysia::input::InputCapture::Pointer;
    }

    const SdlRendererState renderer_state = capture_renderer_state(renderer);
    // ImGui uses window coordinates and applies its own framebuffer scale.
    // Game logical presentation would scale and letterbox the overlay a second time.
    (void)SDL_SetRenderLogicalPresentation(&renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    (void)SDL_SetRenderScale(&renderer, 1.0f, 1.0f);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), &renderer);
    restore_renderer_state(renderer, renderer_state);
    _frame_started = false;
}

void ImGuiDevelopmentOverlay::shutdown() noexcept
{
    if (_context)
        set_current_context();
    if (_sdl_renderer_initialized)
        ImGui_ImplSDLRenderer3_Shutdown();
    if (_sdl_platform_initialized)
        ImGui_ImplSDL3_Shutdown();
    if (_context)
        ImGui::DestroyContext(_context);

    _context = nullptr;
    _panels.clear();
    _pending_operations.clear();
    _next_panel_handle = 1;
    _captured_input = elysia::input::InputCapture::None;
    _sdl_platform_initialized = false;
    _sdl_renderer_initialized = false;
    _frame_started = false;
    _drawing_panels = false;
    _panels_visible = true;
    _show_fps = false;
    _frame_times.fill(0.0f);
    _frame_time_count = 0;
    _frame_time_offset = 0;
}

void ImGuiDevelopmentOverlay::set_panels_visible(bool visible) noexcept
{
    _panels_visible = visible;
    _captured_input = elysia::input::InputCapture::None;
    // Hidden panels do not receive key-up events. Release held input before hiding.
    if (_context && !visible)
    {
        set_current_context();
        ImGui::GetIO().ClearEventsQueue();
        ImGui::GetIO().ClearInputKeys();
        ImGui::GetIO().ClearInputMouse();
    }
}

bool ImGuiDevelopmentOverlay::has_background_content() const noexcept
{
    return _show_fps;
}

void ImGuiDevelopmentOverlay::draw_development_panel()
{
    ImGui::SetNextWindowPos(ImVec2(16.0f, 88.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(390.0f, 550.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(MainPanelTitle))
    {
        const auto& io = ImGui::GetIO();
        ImGui::Text("%.1f FPS | %.2f ms / frame", io.Framerate,
            io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
        ImGui::Checkbox("Show FPS overlay", &_show_fps);
        ImGui::PlotLines("##frame-times", _frame_times.data(), _frame_time_count,
            _frame_time_count == static_cast<int>(_frame_times.size()) ? _frame_time_offset : 0,
            "Frame time (ms)", 0.0f, FLT_MAX, ImVec2(-1.0f, 50.0f));

        ImGui::SeparatorText("Debug draw");
        auto* debug = DebugDraw::instance();
        bool enabled = debug->enabled();
        if (ImGui::Checkbox("Enable debug draw", &enabled))
            debug->set_enabled(enabled);
        ImGui::TextDisabled("Changes appear on the next scene update.");
        if (ImGui::CollapsingHeader("Draw categories", ImGuiTreeNodeFlags_DefaultOpen))
        {
            struct CategoryOption { const char* label; DebugDrawCategory category; };
            static constexpr CategoryOption options[] = {
                {"Colliders", DebugDrawCategory::PhysicsCollider},
                {"Contacts", DebugDrawCategory::PhysicsContact},
                {"Contact normals", DebugDrawCategory::PhysicsContactNormal},
                {"Broad phase / AABB", DebugDrawCategory::PhysicsBroadPhase},
                {"Pose history", DebugDrawCategory::PhysicsPoseHistory},
                {"Velocities", DebugDrawCategory::PhysicsVelocity},
                {"Joints", DebugDrawCategory::PhysicsJoint},
                {"Joint details", DebugDrawCategory::PhysicsJointDetails},
                {"Chain details", DebugDrawCategory::PhysicsChainDetails},
                {"Targets", DebugDrawCategory::PhysicsTarget},
                {"Camera", DebugDrawCategory::Camera},
                {"Gameplay", DebugDrawCategory::Gameplay},
            };
            auto categories = debug->enabled_categories();
            if (ImGui::Button("All"))
                categories = DebugDrawCategory::All;
            ImGui::SameLine();
            if (ImGui::Button("None"))
                categories = DebugDrawCategory::None;
            ImGui::SameLine();
            if (ImGui::Button("Defaults"))
                categories = DebugDrawCategory::Default;
            if (ImGui::BeginTable("draw-categories", 2))
            {
                for (const auto& option : options)
                {
                    ImGui::TableNextColumn();
                    bool selected = (categories & option.category) != DebugDrawCategory::None;
                    if (ImGui::Checkbox(option.label, &selected))
                    {
                        const auto bits = static_cast<std::uint32_t>(categories);
                        const auto mask = static_cast<std::uint32_t>(option.category);
                        categories = static_cast<DebugDrawCategory>(selected ? bits | mask : bits & ~mask);
                    }
                }
                ImGui::EndTable();
            }
            debug->set_enabled_categories(categories);
        }

        ImGui::SeparatorText("Simulation");
        auto* time = elysia::core::Time::instance();
        float scale = static_cast<float>(time->time_scale());
        if (ImGui::SliderFloat("Time scale", &scale, 0.0f, 2.0f, "%.2fx"))
            time->set_time_scale(scale);
        if (ImGui::Button("Pause"))
            time->set_time_scale(0.0);
        ImGui::SameLine();
        if (ImGui::Button("Normal speed"))
            time->set_time_scale(1.0);
        ImGui::TextDisabled("F2 hides panels; FPS overlay stays visible.");
    }
    ImGui::End();
}

void ImGuiDevelopmentOverlay::draw_fps_hud()
{
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 12.0f,
        viewport->WorkPos.y + 12.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav
        | ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin("FPS##development-hud", nullptr, flags))
    {
        const float fps = ImGui::GetIO().Framerate;
        ImGui::Text("FPS %.1f | %.2f ms", fps, fps > 0.0f ? 1000.0f / fps : 0.0f);
    }
    ImGui::End();
}

elysia::input::InputCapture
ImGuiDevelopmentOverlay::captured_input() const noexcept
{
    return _captured_input;
}

DevelopmentPanelHandle ImGuiDevelopmentOverlay::register_panel(
    std::string stable_id,
    DrawCallback draw)
{
    if (stable_id.empty() || !draw || has_panel_id(stable_id)
        || _next_panel_handle == 0)
    {
        Logger::instance()->warn(
            "development_overlay",
            "Development panel registration rejected an empty, duplicate, or invalid panel.");
        return InvalidDevelopmentPanelHandle;
    }

    const DevelopmentPanelHandle handle{_next_panel_handle++};
    PanelEntry panel{handle, std::move(stable_id), std::move(draw)};
    if (_drawing_panels)
    {
        _pending_operations.push_back(PendingOperation{
            .add = true,
            .panel = std::move(panel)});
    }
    else
    {
        _panels.push_back(std::move(panel));
    }
    return handle;
}

bool ImGuiDevelopmentOverlay::unregister_panel(
    DevelopmentPanelHandle handle)
{
    if (!handle.is_valid() || !has_panel_handle(handle))
        return false;

    if (_drawing_panels)
    {
        const bool already_pending = std::any_of(
            _pending_operations.begin(),
            _pending_operations.end(),
            [handle](const PendingOperation& operation)
            {
                return !operation.add && operation.handle == handle;
            });
        if (!already_pending)
        {
            _pending_operations.push_back(PendingOperation{
                .add = false,
                .handle = handle});
        }
        return !already_pending;
    }

    const auto previous_size = _panels.size();
    std::erase_if(_panels, [handle](const PanelEntry& panel)
    {
        return panel.handle == handle;
    });
    return _panels.size() != previous_size;
}

bool ImGuiDevelopmentOverlay::has_panel_id(
    const std::string& stable_id) const
{
    const auto matches = [&stable_id](const PanelEntry& panel)
    {
        return panel.stable_id == stable_id;
    };
    return std::any_of(_panels.begin(), _panels.end(), matches)
        || std::any_of(
            _pending_operations.begin(),
            _pending_operations.end(),
            [&matches](const PendingOperation& operation)
            {
                return operation.add && matches(operation.panel);
            });
}

bool ImGuiDevelopmentOverlay::has_panel_handle(
    DevelopmentPanelHandle handle) const
{
    const auto matches = [handle](const PanelEntry& panel)
    {
        return panel.handle == handle;
    };
    return std::any_of(_panels.begin(), _panels.end(), matches)
        || std::any_of(
            _pending_operations.begin(),
            _pending_operations.end(),
            [&matches](const PendingOperation& operation)
            {
                return operation.add && matches(operation.panel);
            });
}

void ImGuiDevelopmentOverlay::apply_pending_operations()
{
    for (PendingOperation& operation : _pending_operations)
    {
        if (operation.add)
        {
            _panels.push_back(std::move(operation.panel));
        }
        else
        {
            std::erase_if(_panels, [&operation](const PanelEntry& panel)
            {
                return panel.handle == operation.handle;
            });
        }
    }
    _pending_operations.clear();
}

void ImGuiDevelopmentOverlay::set_current_context() const noexcept
{
    ImGui::SetCurrentContext(_context);
}
}
