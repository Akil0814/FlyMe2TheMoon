#pragma once

#include <cstddef>
#include <string>

namespace elysia::resources
{
class Atlas;
}

namespace elysia::animation
{
struct AnimationDefinition
{
	std::string animation_key;
	std::string atlas_key;
	double fps = 10.0;
	bool loop = true;
	std::size_t segment_index = 0;
	const elysia::resources::Atlas* atlas = nullptr;
};
}
