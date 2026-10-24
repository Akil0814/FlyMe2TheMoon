#pragma once

#include "../core/diagnostics/failure_diagnostic.h"

namespace elysia::animation
{
enum class AnimationRegistrationError
{
    InvalidKey,
    MissingAtlas,
    InvalidFps
};

struct AnimationRegistrationFailure
{
    AnimationRegistrationError code = AnimationRegistrationError::InvalidKey;
    elysia::core::FailureDiagnostic diagnostic;
};
}
