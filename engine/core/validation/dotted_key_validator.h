#pragma once

#include <expected>
#include <source_location>
#include <string>
#include <string_view>

namespace elysia::core
{
enum class KeyValidationError
{
    EmptyKey,
    EmptyComponent,
    InvalidCharacter,
    MissingLogicalComponent,
    SegmentOutOfRange
};

struct KeyValidationFailure
{
    KeyValidationError code = KeyValidationError::EmptyKey;
    std::string value;
    std::string message;
    std::source_location origin = std::source_location::current();
};

class DottedKeyValidator
{
public:
    [[nodiscard]] static std::expected<void,KeyValidationFailure> validate_component(
        std::string_view component);
    [[nodiscard]] static std::expected<void,KeyValidationFailure> validate_key(
        std::string_view key);
};
}
