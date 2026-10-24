#include "dotted_key_validator.h"

namespace elysia::core
{
std::expected<void,KeyValidationFailure> DottedKeyValidator::validate_component(
    std::string_view component)
{
    if (component.empty())
        return std::unexpected(KeyValidationFailure{
            KeyValidationError::EmptyComponent,{},"key component is empty",
            std::source_location::current()});
    for (const unsigned char character : component)
    {
        const bool alpha = (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
        const bool digit = character >= '0' && character <= '9';
        if (!(alpha || digit || character == '_'))
            return std::unexpected(KeyValidationFailure{
                KeyValidationError::InvalidCharacter,std::string(component),
                "invalid key component: " + std::string(component),
                std::source_location::current()});
    }
    return {};
}

std::expected<void,KeyValidationFailure> DottedKeyValidator::validate_key(
    std::string_view key)
{
    if (key.empty())
        return std::unexpected(KeyValidationFailure{
            KeyValidationError::EmptyKey,{},"key is empty",
            std::source_location::current()});
    size_t begin = 0;
    while (begin <= key.size())
    {
        const size_t end = key.find('.',begin);
        const auto component = key.substr(begin,end == std::string_view::npos ? key.size() - begin : end - begin);
        if (auto result = validate_component(component); !result)
            return result;
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
    return {};
}
}
