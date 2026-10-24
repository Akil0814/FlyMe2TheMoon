#pragma once

#include "../diagnostics/failure_diagnostic.h"
#include <expected>
#include <stdexcept>

namespace elysia::core
{
struct RenderFailure
{
    std::string operation;
    FailureDiagnostic diagnostic;
};
using RenderResult = std::expected<void,RenderFailure>;

class RenderBackendError final : public std::runtime_error
{
public:
    explicit RenderBackendError(RenderFailure failure)
        : std::runtime_error(failure.diagnostic.message),_failure(std::move(failure)) {}
    [[nodiscard]] const RenderFailure& failure() const noexcept { return _failure; }
private:
    RenderFailure _failure;
};

inline void require_render_success(RenderResult result)
{
    if (!result) throw RenderBackendError(std::move(result.error()));
}
}
