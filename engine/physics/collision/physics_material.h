#pragma once
namespace elysia::physics
{
	struct SurfaceDensity
	{
		float kilograms_per_square_meter = 1;
		bool operator==(const SurfaceDensity&) const noexcept = default;
	};

	struct PhysicsMaterial
	{
		float friction = 0.4f, restitution = 0;
		bool operator==(const PhysicsMaterial&) const noexcept = default;
	};
}
