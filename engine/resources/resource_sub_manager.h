#pragma once

#include <cstddef>

namespace elysia::resources
{
class ResourceSubManager
{
public:
	virtual ~ResourceSubManager() = default;

	virtual void clear() = 0;
	virtual size_t resource_count() const = 0;
};

}
