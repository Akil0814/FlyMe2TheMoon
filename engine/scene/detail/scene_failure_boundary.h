#pragma once

#include "../scene_boundary_failure.h"
#include "../../core/render/render_failure.h"
#include <exception>
#include <expected>
#include <array>
#include <new>

namespace elysia::scene::detail
{
inline void append_scene_failure_context(elysia::core::FailureDiagnostic& diagnostic,
    const SceneBoundaryFailure& failure,std::string_view stage)
{
    diagnostic.entries.push_back(elysia::core::make_failure_diagnostic_entry(
        "scene-cleanup",std::to_string(failure.scene),{},{},{},
        std::string(stage) + " (" + std::string(scene_boundary_name(failure.boundary))
            + "): " + failure.diagnostic.message,failure.diagnostic.origin));
    diagnostic.entries.insert(diagnostic.entries.end(),
        failure.diagnostic.entries.begin(),failure.diagnostic.entries.end());
}

struct CapturedSceneFailure
{
    SceneKey scene;
    SceneBoundary boundary;
    std::string_view stage;
    std::source_location origin;
    std::exception_ptr exception;
    bool cleanup = false;
};
inline constexpr std::size_t kMaxCapturedSceneFailures = 16;
using CapturedSceneFailures = std::array<CapturedSceneFailure,kMaxCapturedSceneFailures>;

// Private transport between nested cleanup helpers. Formatting happens only after cleanup.
class SceneFailureTransport
{
public:
    SceneFailureTransport(CapturedSceneFailures failures,std::size_t count,std::size_t overflow,
        bool cleanup_overflow)
        : failures(std::move(failures)), count(count), overflow(overflow), cleanup_overflow(cleanup_overflow) {}
    virtual ~SceneFailureTransport() = default;
    CapturedSceneFailures failures;
    std::size_t count = 0;
    std::size_t overflow = 0;
    bool cleanup_overflow = false;
};

template<typename Error>
class SceneFailureException final : public Error,public SceneBoundaryTagged,public SceneFailureTransport
{
public:
    SceneFailureException(CapturedSceneFailures failures,std::size_t count,std::size_t overflow,
        bool cleanup_overflow)
        : Error(primary_message(failures.front().exception)),
          SceneBoundaryTagged(primary_boundary(failures.front()),primary_origin(failures.front())),
          SceneFailureTransport(std::move(failures),count,overflow,cleanup_overflow) {}
private:
    static std::string primary_message(const std::exception_ptr& exception)
    {
        try { std::rethrow_exception(exception); }
        catch (const std::exception& error) { return error.what(); }
        catch (...) { return "Unknown scene boundary exception."; }
    }
    static SceneBoundary primary_boundary(const CapturedSceneFailure& failure)
    {
        try { std::rethrow_exception(failure.exception); }
        catch (const SceneBoundaryTagged& error) { return error.scene_boundary(); }
        catch (...) { return failure.boundary; }
    }
    static std::source_location primary_origin(const CapturedSceneFailure& failure)
    {
        try { std::rethrow_exception(failure.exception); }
        catch (const SceneBoundaryTagged& error) { return error.origin(); }
        catch (...) { return failure.origin; }
    }
};

class SceneFailureCollector
{
public:
    void capture(SceneKey scene,SceneBoundary boundary,std::string_view stage,
        std::exception_ptr exception = std::current_exception(),
        std::source_location origin = std::source_location::current(),bool cleanup = false)
    {
        try { if (exception) std::rethrow_exception(exception); }
        catch (const SceneFailureTransport& error)
        {
            for (std::size_t index = 0; index < error.count; ++index)
            {
                auto captured = error.failures[index];
                if (captured.scene == SceneKeys::Invalid) captured.scene = scene;
                captured.cleanup = captured.cleanup || cleanup;
                record(std::move(captured));
            }
            _overflow += error.overflow;
            _cleanup_overflow = _cleanup_overflow || error.cleanup_overflow || (cleanup && error.overflow > 0);
            return;
        }
        catch (...) {}
        if (exception) record({scene,boundary,stage,origin,std::move(exception),cleanup});
    }

    void capture_cleanup(SceneKey scene,SceneBoundary boundary,std::string_view stage,
        std::exception_ptr exception = std::current_exception(),
        std::source_location origin = std::source_location::current())
    {
        capture(scene,boundary,stage,std::move(exception),origin,true);
    }

    template<typename Callable>
    void attempt(SceneKey scene,SceneBoundary boundary,std::string_view stage,Callable&& callable,
        std::source_location origin = std::source_location::current())
    {
        try { std::forward<Callable>(callable)(); }
        catch (...) { capture(scene,boundary,stage,std::current_exception(),origin); }
    }

    template<typename Callable>
    void attempt_cleanup(SceneKey scene,SceneBoundary boundary,std::string_view stage,Callable&& callable,
        std::source_location origin = std::source_location::current())
    {
        try { std::forward<Callable>(callable)(); }
        catch (...) { capture_cleanup(scene,boundary,stage,std::current_exception(),origin); }
    }

    [[nodiscard]] bool empty() const noexcept { return _count == 0; }

    void rethrow_if_failed()
    {
        if (empty()) return;
        const auto primary = _failures.front().exception;
        try { std::rethrow_exception(primary); }
        catch (const std::logic_error&)
        {
            try { throw SceneFailureException<std::logic_error>(std::move(_failures),_count,_overflow,_cleanup_overflow); }
            catch (const std::bad_alloc&) { std::rethrow_exception(primary); }
        }
        catch (...)
        {
            try { throw SceneFailureException<std::runtime_error>(std::move(_failures),_count,_overflow,_cleanup_overflow); }
            catch (const std::bad_alloc&) { std::rethrow_exception(primary); }
        }
    }

    [[nodiscard]] std::expected<void,SceneBoundaryFailure> finish() const
    {
        if (empty()) return {};
        try
        {
        for (std::size_t index = 0; index < _count; ++index)
        {
            try { if (_failures[index].exception) std::rethrow_exception(_failures[index].exception); }
            catch (const elysia::core::RenderBackendError& error)
            {
                auto backend = error.failure();
                backend.diagnostic = to_failure_diagnostic({
                    _failures[index].scene,_failures[index].boundary,std::move(backend.diagnostic)});
                for (std::size_t other = 0; other < _count; ++other)
                    if (other != index) append(backend.diagnostic,_failures[other]);
                append_overflow(backend.diagnostic);
                throw elysia::core::RenderBackendError(std::move(backend));
            }
            catch (...) {}
        }
        auto primary = describe(_failures.front());
        primary.cleanup_failed = _cleanup_overflow;
        // A primary Exit/Detach/ObjectRemoval failure can still be isolated.
        // Only additional cleanup failures make recovery unsafe.
        for (std::size_t index = 1; index < _count; ++index)
        {
            primary.cleanup_failed = primary.cleanup_failed || _failures[index].cleanup;
            append(primary.diagnostic,_failures[index]);
        }
        append_overflow(primary.diagnostic);
        return std::unexpected(std::move(primary));
        }
        catch (const std::bad_alloc&)
        {
            std::rethrow_exception(_failures.front().exception);
        }
    }

private:
    static SceneBoundaryFailure describe(const CapturedSceneFailure& captured)
    {
        auto boundary = captured.boundary;
        auto origin = captured.origin;
        std::string message = "Unknown scene boundary exception.";
        try { if (captured.exception) std::rethrow_exception(captured.exception); }
        catch (const elysia::core::RenderBackendError& error)
        {
            auto diagnostic = error.failure().diagnostic;
            diagnostic.message = error.failure().operation + ": " + diagnostic.message;
            return {captured.scene,boundary,std::move(diagnostic)};
        }
        catch (const SceneBoundaryTagged& error)
        {
            boundary = error.scene_boundary();
            origin = error.origin();
            if (const auto* standard = dynamic_cast<const std::exception*>(&error)) message = standard->what();
        }
        catch (const std::exception& error) { message = error.what(); }
        catch (...) {}
        return make_scene_boundary_failure(captured.scene,boundary,std::move(message),origin);
    }

    static void append(elysia::core::FailureDiagnostic& diagnostic,const CapturedSceneFailure& captured)
    {
        append_scene_failure_context(diagnostic,describe(captured),captured.stage);
    }

    void append_overflow(elysia::core::FailureDiagnostic& diagnostic) const
    {
        if (!_overflow) return;
        diagnostic.entries.push_back(elysia::core::make_failure_diagnostic_entry(
            "scene-cleanup",{},{},{},{},
            "Additional cleanup failures omitted: " + std::to_string(_overflow),
            diagnostic.origin));
    }

    void record(CapturedSceneFailure failure) noexcept
    {
        if (_count < _failures.size())
            _failures[_count++] = std::move(failure);
        else
        {
            ++_overflow;
            _cleanup_overflow = _cleanup_overflow || failure.cleanup;
        }
    }
    CapturedSceneFailures _failures{};
    std::size_t _count = 0;
    std::size_t _overflow = 0;
    bool _cleanup_overflow = false;
};
}
