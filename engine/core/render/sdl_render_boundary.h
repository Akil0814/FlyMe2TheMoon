#pragma once

#include "render_failure.h"
#include <SDL3/SDL.h>
#include <exception>
#include <optional>
#include <type_traits>

namespace elysia::core::detail
{
// Per-thread internal seam. Normal execution always calls the SDL operation.
inline thread_local bool (*render_operation_probe)(std::string_view) = nullptr;

template<typename Callable>
void checked_render_operation(const char* operation,Callable&& callable,
    std::source_location origin = std::source_location::current())
{
    bool success = false;
    if (!render_operation_probe || render_operation_probe(operation))
    {
        const auto result = callable();
        if constexpr (std::is_same_v<std::remove_cv_t<decltype(result)>,bool>) success = result;
        else success = result >= 0; // SDL_gfx uses 0/-1.
    }
    if (!success)
        throw RenderBackendError(RenderFailure{operation,make_failure_diagnostic(
            std::string(operation) + ": " + SDL_GetError(),{},{},origin)});
}

template<typename Callable>
[[nodiscard]] RenderResult render_boundary(SDL_Renderer* renderer,Callable&& callable)
{
    try
    {
        if (!renderer)
            throw RenderBackendError({"renderer",make_failure_diagnostic("Renderer is unavailable.")});
        callable();
        return {};
    }
    catch (const RenderBackendError& error) { return std::unexpected(error.failure()); }
}

inline void append_render_failure(std::optional<RenderFailure>& first,const RenderFailure& failure)
{
    if (!first) first = failure;
    else
    {
        first->diagnostic.entries.push_back(make_failure_diagnostic_entry(
            "render-restore",failure.operation,{},{},{},failure.diagnostic.message,failure.diagnostic.origin));
        first->diagnostic.entries.insert(first->diagnostic.entries.end(),
            failure.diagnostic.entries.begin(),failure.diagnostic.entries.end());
    }
}

inline void append_render_exception(RenderFailure& failure,const std::exception_ptr& exception,
    const char* stage,std::source_location origin = std::source_location::current())
{
    if (!exception) return;
    std::string reason = "Unknown non-standard exception.";
    try { std::rethrow_exception(exception); }
    catch (const std::exception& error) { reason = error.what(); }
    catch (...) {}
    failure.diagnostic.entries.push_back(make_failure_diagnostic_entry(
        stage,{},{},{},{},std::move(reason),origin));
}

template<typename State,typename Callable>
void with_render_state(State& state,Callable&& callable)
{
    std::optional<RenderFailure> first;
    std::exception_ptr command_exception;
    std::exception_ptr restore_exception;
    std::source_location command_origin;
    std::source_location restore_origin;
    try { callable(); }
    catch (const RenderBackendError& error) { first = error.failure(); }
    catch (...) { command_exception = std::current_exception(); command_origin = std::source_location::current(); }
    const auto restore = [&](auto&& operation)
    {
        try { operation(); }
        catch (const RenderBackendError& error) { append_render_failure(first,error.failure()); }
        catch (...)
        {
            if (!restore_exception)
            {
                restore_exception = std::current_exception();
                restore_origin = std::source_location::current();
            }
        }
    };
    state.restore(restore);
    // Once any backend operation fails, ordinary exceptions must not route
    // execution back into a scene using that renderer.
    if (first)
    {
        append_render_exception(*first,command_exception,"render-command",command_origin);
        append_render_exception(*first,restore_exception,"render-restore",restore_origin);
        throw RenderBackendError(std::move(*first));
    }
    if (command_exception) std::rethrow_exception(command_exception);
    if (restore_exception) std::rethrow_exception(restore_exception);
}

class RendererState
{
public:
    explicit RendererState(SDL_Renderer* renderer) : _renderer(renderer)
    {
        _clip_enabled = SDL_RenderClipEnabled(renderer);
        if (_clip_enabled)
            checked_render_operation("SDL_GetRenderClipRect",[&] { return SDL_GetRenderClipRect(renderer,&_clip); });
        checked_render_operation("SDL_GetRenderDrawBlendMode",[&] { return SDL_GetRenderDrawBlendMode(renderer,&_blend); });
        checked_render_operation("SDL_GetRenderDrawColor",[&] { return SDL_GetRenderDrawColor(renderer,&_r,&_g,&_b,&_a); });
    }
    void set_clip(const SDL_Rect* clip)
    {
        checked_render_operation("SDL_SetRenderClipRect",[&] { return SDL_SetRenderClipRect(_renderer,clip); });
        _clip_changed = _clip_changed || (clip != nullptr) != _clip_enabled
            || (clip && (clip->x != _clip.x || clip->y != _clip.y || clip->w != _clip.w || clip->h != _clip.h));
    }
    void set_blend(SDL_BlendMode blend)
    {
        checked_render_operation("SDL_SetRenderDrawBlendMode",[&] { return SDL_SetRenderDrawBlendMode(_renderer,blend); });
        _blend_changed = _blend_changed || blend != _blend;
    }
    void set_color(SDL_Color color)
    {
        checked_render_operation("SDL_SetRenderDrawColor",[&] { return SDL_SetRenderDrawColor(_renderer,color.r,color.g,color.b,color.a); });
        _color_changed = _color_changed || color.r != _r || color.g != _g || color.b != _b || color.a != _a;
    }
    // SDL_gfx performs its own color/blend writes, including on partial failure.
    void mark_gfx_changes() noexcept { _color_changed = true; _blend_changed = true; }
    template<typename Restore>
    void restore(Restore&& restore)
    {
        if (_color_changed) restore([&] { checked_render_operation("restore.SDL_SetRenderDrawColor",[&] { return SDL_SetRenderDrawColor(_renderer,_r,_g,_b,_a); }); });
        if (_blend_changed) restore([&] { checked_render_operation("restore.SDL_SetRenderDrawBlendMode",[&] { return SDL_SetRenderDrawBlendMode(_renderer,_blend); }); });
        if (_clip_changed) restore([&] { checked_render_operation("restore.SDL_SetRenderClipRect",[&] { return SDL_SetRenderClipRect(_renderer,_clip_enabled ? &_clip : nullptr); }); });
    }
private:
    SDL_Renderer* _renderer;
    SDL_Rect _clip{};
    SDL_BlendMode _blend{};
    Uint8 _r{},_g{},_b{},_a{};
    bool _clip_enabled = false;
    bool _clip_changed = false,_blend_changed = false,_color_changed = false;
};

class TextureState
{
public:
    TextureState(SDL_Texture* texture,bool color) : _texture(texture),_has_color(color)
    {
        checked_render_operation("SDL_GetTextureAlphaMod",[&] { return SDL_GetTextureAlphaMod(texture,&_a); });
        if (color) checked_render_operation("SDL_GetTextureColorMod",[&] { return SDL_GetTextureColorMod(texture,&_r,&_g,&_b); });
    }
    void set_alpha(Uint8 alpha)
    {
        checked_render_operation("SDL_SetTextureAlphaMod",[&] { return SDL_SetTextureAlphaMod(_texture,alpha); });
        _alpha_changed = alpha != _a;
    }
    void set_color(Uint8 r,Uint8 g,Uint8 b)
    {
        checked_render_operation("SDL_SetTextureColorMod",[&] { return SDL_SetTextureColorMod(_texture,r,g,b); });
        _color_changed = _has_color && (r != _r || g != _g || b != _b);
    }
    template<typename Restore>
    void restore(Restore&& restore)
    {
        if (_color_changed) restore([&] { checked_render_operation("restore.SDL_SetTextureColorMod",[&] { return SDL_SetTextureColorMod(_texture,_r,_g,_b); }); });
        if (_alpha_changed) restore([&] { checked_render_operation("restore.SDL_SetTextureAlphaMod",[&] { return SDL_SetTextureAlphaMod(_texture,_a); }); });
    }
private:
    SDL_Texture* _texture;
    Uint8 _r{},_g{},_b{},_a{};
    bool _has_color,_alpha_changed = false,_color_changed = false;
};
}

namespace elysia::core
{
[[nodiscard]] inline RenderResult begin_render_frame(SDL_Renderer* renderer)
{
    return detail::render_boundary(renderer,[&] {
        detail::checked_render_operation("SDL_SetRenderDrawColor",[&] { return SDL_SetRenderDrawColor(renderer,0,0,0,255); });
        detail::checked_render_operation("SDL_RenderClear",[&] { return SDL_RenderClear(renderer); });
    });
}
[[nodiscard]] inline RenderResult present_render_frame(SDL_Renderer* renderer)
{
    return detail::render_boundary(renderer,[&] {
        detail::checked_render_operation("SDL_RenderPresent",[&] { return SDL_RenderPresent(renderer); });
    });
}
}
