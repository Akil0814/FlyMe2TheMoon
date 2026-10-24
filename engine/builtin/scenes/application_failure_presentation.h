#pragma once

#include "application_failure_scene_payload.h"

#include <string>
#include <string_view>

namespace elysia::builtin
{
struct ApplicationFailurePresentationModel
{
    std::string summary_key;
    std::string error_code;
    std::string category;
    std::string log_file_name;
    bool log_available = false;
    std::string diagnostic_details;
    std::string copy_report;
};

[[nodiscard]] std::string_view application_failure_summary_key(
    ApplicationFailureReason reason) noexcept;

[[nodiscard]] ApplicationFailurePresentationModel
build_application_failure_presentation(
    const ApplicationFailureScenePayload& payload,
    std::string log_file_name,
    bool include_diagnostics);
}
