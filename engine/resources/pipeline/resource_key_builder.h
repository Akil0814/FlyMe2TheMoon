#pragma once

#include "../../core/validation/dotted_key_validator.h"

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace elysia::resources
{
class ResourceKeyBuilder
{
public:
	[[nodiscard]] static std::expected<void,elysia::core::KeyValidationFailure>
		validate_component(std::string_view component);
	[[nodiscard]] static std::expected<void,elysia::core::KeyValidationFailure>
		validate_key(std::string_view key);
	[[nodiscard]] static std::expected<std::string,elysia::core::KeyValidationFailure> build(
		std::string_view entity_id,
		std::string_view key_namespace,
		const std::vector<std::string>& logical_components,
		std::optional<size_t> segment_index
	);
	[[nodiscard]] static std::expected<std::string,elysia::core::KeyValidationFailure> append_component(
		std::string_view base_key,
		std::string_view component
	);
};
}
