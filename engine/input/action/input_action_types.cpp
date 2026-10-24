#include "input_action_types.h"

#include "../../core/validation/dotted_key_validator.h"

#include <stdexcept>

namespace elysia::input
{
namespace
{
std::string validate_action_id(std::string_view value)
{
    if (auto result = elysia::core::DottedKeyValidator::validate_key(value); !result)
    {
        throw std::invalid_argument("Invalid input action id '" + std::string(value)
            + "': " + result.error().message);
    }
    return std::string(value);
}
}

InputActionId::InputActionId(std::string_view value)
    : _value(validate_action_id(value))
{
}
}
