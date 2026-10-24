#pragma once

#include <cmath>

namespace elysia::core
{
class Vector2
{
public:
	static constexpr float kEpsilon = 1e-6f;

	float x = 0.0f;
	float y = 0.0f;

public:
	constexpr Vector2() noexcept = default;
	constexpr Vector2(float x, float y) noexcept
		: x(x), y(y) { }

	~Vector2() = default;

	[[nodiscard]] static constexpr Vector2 zero() noexcept
	{
		return Vector2(0.0f, 0.0f);
	}

	[[nodiscard]] static constexpr Vector2 one() noexcept
	{
		return Vector2(1.0f, 1.0f);
	}

	[[nodiscard]] constexpr Vector2 operator+() const noexcept
	{
		return *this;
	}

	[[nodiscard]] constexpr Vector2 operator-() const noexcept
	{
		return Vector2(-x, -y);
	}

	[[nodiscard]] constexpr Vector2 operator+(const Vector2& vec) const noexcept
	{
		return Vector2(x + vec.x, y + vec.y);
	}

	Vector2& operator+=(const Vector2& vec) noexcept
	{
		x += vec.x;
		y += vec.y;
		return *this;
	}

	[[nodiscard]] constexpr Vector2 operator-(const Vector2& vec) const noexcept
	{
		return Vector2(x - vec.x, y - vec.y);
	}

	Vector2& operator-=(const Vector2& vec) noexcept
	{
		x -= vec.x;
		y -= vec.y;
		return *this;
	}

	[[nodiscard]] constexpr float operator*(const Vector2& vec) const noexcept
	{
		return dot(vec);
	}

	[[nodiscard]] constexpr Vector2 operator*(float val) const noexcept
	{
		return Vector2(x * val, y * val);
	}

	Vector2& operator*=(float val) noexcept
	{
		x *= val;
		y *= val;
		return *this;
	}

	[[nodiscard]] Vector2 operator/(float val) const noexcept
	{
		return Vector2(x / val, y / val);
	}

	Vector2& operator/=(float val) noexcept
	{
		x /= val;
		y /= val;
		return *this;
	}

	[[nodiscard]] constexpr bool operator==(const Vector2& vec) const noexcept
	{
		return x == vec.x && y == vec.y;
	}

	[[nodiscard]] constexpr bool operator!=(const Vector2& vec) const noexcept
	{
		return !(*this == vec);
	}

	[[nodiscard]] constexpr float dot(const Vector2& vec) const noexcept
	{
		return x * vec.x + y * vec.y;
	}

	[[nodiscard]] constexpr float cross(const Vector2& vec) const noexcept
	{
		return x * vec.y - y * vec.x;
	}

	[[nodiscard]] constexpr float length_squared() const noexcept
	{
		return x * x + y * y;
	}

	[[nodiscard]] float length() const noexcept
	{
		return std::sqrt(length_squared());
	}

	[[nodiscard]] constexpr float distance_squared_to(const Vector2& vec) const noexcept
	{
		return (*this - vec).length_squared();
	}

	[[nodiscard]] float distance_to(const Vector2& vec) const noexcept
	{
		return (*this - vec).length();
	}

	[[nodiscard]] bool is_zero(float epsilon = kEpsilon) const noexcept
	{
		return std::fabs(x) <= epsilon && std::fabs(y) <= epsilon;
	}

	[[nodiscard]] bool nearly_equals(const Vector2& vec, float epsilon = kEpsilon) const noexcept
	{
		return std::fabs(x - vec.x) <= epsilon && std::fabs(y - vec.y) <= epsilon;
	}

	[[nodiscard]] Vector2 normalized(float epsilon = kEpsilon) const noexcept
	{
		float len = length();
		if (len <= epsilon)
		{
			return zero();
		}

		return *this / len;
	}

	[[nodiscard]] Vector2 normalize(float epsilon = kEpsilon) const noexcept
	{
		return normalized(epsilon);
	}

	bool normalize_in_place(float epsilon = kEpsilon) noexcept
	{
		float len = length();
		if (len <= epsilon)
		{
			x = 0.0f;
			y = 0.0f;
			return false;
		}

		*this /= len;
		return true;
	}

	[[nodiscard]] Vector2 direction_to(const Vector2& vec, float epsilon = kEpsilon) const noexcept
	{
		return (vec - *this).normalized(epsilon);
	}

	friend constexpr Vector2 operator*(float val, const Vector2& vec) noexcept
	{
		return vec * val;
	}
};

}
