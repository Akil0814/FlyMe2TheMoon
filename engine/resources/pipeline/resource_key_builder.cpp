#include "resource_key_builder.h"
#include "../../core/validation/dotted_key_validator.h"

#include <utility>

namespace elysia::resources
{
std::expected<void,elysia::core::KeyValidationFailure>
ResourceKeyBuilder::validate_component(std::string_view component)
{
	return elysia::core::DottedKeyValidator::validate_component(component);
}

std::expected<void,elysia::core::KeyValidationFailure>
ResourceKeyBuilder::validate_key(std::string_view key)
{
	return elysia::core::DottedKeyValidator::validate_key(key);
}

std::expected<std::string,elysia::core::KeyValidationFailure> ResourceKeyBuilder::build(
	std::string_view entity_id,
	std::string_view key_namespace,
	const std::vector<std::string>& logical_components,
	std::optional<size_t> segment_index)
{
	if (auto result = validate_component(entity_id); !result)
		return std::unexpected(std::move(result.error()));
	if (!key_namespace.empty())
		if (auto result = validate_component(key_namespace); !result)
			return std::unexpected(std::move(result.error()));
	if (logical_components.empty())
		return std::unexpected(elysia::core::KeyValidationFailure{
			elysia::core::KeyValidationError::MissingLogicalComponent,{},
			"resource key requires at least one logical component",
			std::source_location::current()});
	std::string key(entity_id);
	if (!key_namespace.empty()) key += "." + std::string(key_namespace);
	for (const std::string& component : logical_components)
	{
		if (auto result = validate_component(component); !result)
			return std::unexpected(std::move(result.error()));
		key += "." + component;
	}
	if (segment_index)
	{
		if (*segment_index > 99)
			return std::unexpected(elysia::core::KeyValidationFailure{
				elysia::core::KeyValidationError::SegmentOutOfRange,
				std::to_string(*segment_index),"resource segment index exceeds 99",
				std::source_location::current()});
		key += "." + std::to_string(*segment_index);
	}
	return key;
}

std::expected<std::string,elysia::core::KeyValidationFailure>
ResourceKeyBuilder::append_component(
	std::string_view base_key,
	std::string_view component)
{
	if (auto result = validate_key(base_key); !result)
		return std::unexpected(std::move(result.error()));
	if (auto result = validate_component(component); !result)
		return std::unexpected(std::move(result.error()));
	return std::string(base_key) + "." + std::string(component);
}
}
