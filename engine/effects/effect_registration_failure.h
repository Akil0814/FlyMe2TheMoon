#pragma once

#include "../core/diagnostics/failure_diagnostic.h"

namespace elysia::effects
{
enum class EffectRegistrationError
{
    InvalidKey,
    InvalidAnimationKey,
    InvalidSize,
    MissingAnimation
};

struct EffectRegistrationFailure
{
    EffectRegistrationError code = EffectRegistrationError::InvalidKey;
    elysia::core::FailureDiagnostic diagnostic;
};
}
