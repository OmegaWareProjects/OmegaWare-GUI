#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace OmegaGUI
{
	struct vec2
	{
		float x = 0.f;
		float y = 0.f;

		constexpr vec2() = default;
		constexpr vec2(float flX, float flY) : x(flX), y(flY) {}

		constexpr vec2 operator+(const vec2& rOther) const { return { x + rOther.x, y + rOther.y }; }
		constexpr vec2 operator-(const vec2& rOther) const { return { x - rOther.x, y - rOther.y }; }
		constexpr vec2 operator*(const vec2& rOther) const { return { x * rOther.x, y * rOther.y }; }
		constexpr vec2 operator*(float flScale) const { return { x * flScale, y * flScale }; }
		constexpr vec2 operator/(float flScale) const { return { x / flScale, y / flScale }; }
		constexpr vec2 operator-() const { return { -x, -y }; }
		constexpr vec2& operator+=(const vec2& rOther) { x += rOther.x; y += rOther.y; return *this; }
		constexpr vec2& operator-=(const vec2& rOther) { x -= rOther.x; y -= rOther.y; return *this; }
		constexpr vec2& operator*=(float flScale) { x *= flScale; y *= flScale; return *this; }
		constexpr bool operator==(const vec2&) const = default;

		[[nodiscard]] float Length() const { return std::sqrt(x * x + y * y); }
		[[nodiscard]] constexpr float LengthSqr() const { return x * x + y * y; }
		[[nodiscard]] vec2 Normalized() const
		{
			const float flLength = Length();
			return flLength > 0.f ? vec2{ x / flLength, y / flLength } : vec2{};
		}
		[[nodiscard]] constexpr float Dot(const vec2& rOther) const { return x * rOther.x + y * rOther.y; }
		[[nodiscard]] vec2 Floor() const { return { std::floor(x), std::floor(y) }; }
	};

	struct vec3
	{
		float x = 0.f;
		float y = 0.f;
		float z = 0.f;

		constexpr vec3() = default;
		constexpr vec3(float flX, float flY, float flZ) : x(flX), y(flY), z(flZ) {}

		constexpr vec3 operator+(const vec3& rOther) const { return { x + rOther.x, y + rOther.y, z + rOther.z }; }
		constexpr vec3 operator-(const vec3& rOther) const { return { x - rOther.x, y - rOther.y, z - rOther.z }; }
		constexpr vec3 operator*(float flScale) const { return { x * flScale, y * flScale, z * flScale }; }
		constexpr vec3 operator-() const { return { -x, -y, -z }; }
		constexpr bool operator==(const vec3&) const = default;

		[[nodiscard]] float Length() const { return std::sqrt(x * x + y * y + z * z); }
		[[nodiscard]] vec3 Normalized() const
		{
			const float flLength = Length();
			return flLength > 0.f ? vec3{ x / flLength, y / flLength, z / flLength } : vec3{};
		}
		[[nodiscard]] constexpr float Dot(const vec3& rOther) const { return x * rOther.x + y * rOther.y + z * rOther.z; }
		[[nodiscard]] constexpr vec3 Cross(const vec3& rOther) const
		{
			return { y * rOther.z - z * rOther.y, z * rOther.x - x * rOther.z, x * rOther.y - y * rOther.x };
		}
	};

	struct vec4
	{
		float x = 0.f;
		float y = 0.f;
		float z = 0.f;
		float w = 0.f;

		constexpr vec4() = default;
		constexpr vec4(float flX, float flY, float flZ, float flW) : x(flX), y(flY), z(flZ), w(flW) {}
		constexpr bool operator==(const vec4&) const = default;
	};

	struct rect
	{
		vec2 min;
		vec2 max;

		constexpr rect() = default;
		constexpr rect(const vec2& rMin, const vec2& rMax) : min(rMin), max(rMax) {}
		constexpr rect(float flX0, float flY0, float flX1, float flY1) : min(flX0, flY0), max(flX1, flY1) {}

		[[nodiscard]] constexpr float Width() const { return max.x - min.x; }
		[[nodiscard]] constexpr float Height() const { return max.y - min.y; }
		[[nodiscard]] constexpr vec2 Size() const { return max - min; }
		[[nodiscard]] constexpr vec2 Center() const { return { (min.x + max.x) * .5f, (min.y + max.y) * .5f }; }
		[[nodiscard]] constexpr bool Contains(const vec2& rPoint) const
		{
			return rPoint.x >= min.x && rPoint.y >= min.y && rPoint.x < max.x && rPoint.y < max.y;
		}
		[[nodiscard]] constexpr bool Overlaps(const rect& rOther) const
		{
			return rOther.min.y < max.y && rOther.max.y > min.y && rOther.min.x < max.x && rOther.max.x > min.x;
		}
		[[nodiscard]] constexpr bool IsEmpty() const { return max.x <= min.x || max.y <= min.y; }
		[[nodiscard]] constexpr rect Expanded(float flAmount) const
		{
			return { min.x - flAmount, min.y - flAmount, max.x + flAmount, max.y + flAmount };
		}
		[[nodiscard]] constexpr rect Clipped(const rect& rClip) const
		{
			return { std::max(min.x, rClip.min.x), std::max(min.y, rClip.min.y), std::min(max.x, rClip.max.x), std::min(max.y, rClip.max.y) };
		}
		constexpr bool operator==(const rect&) const = default;
	};

	struct mat4
	{
		std::array<float, 16> m{ 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f };

		[[nodiscard]] constexpr float& At(int iRow, int iColumn) { return m[iRow * 4 + iColumn]; }
		[[nodiscard]] constexpr float At(int iRow, int iColumn) const { return m[iRow * 4 + iColumn]; }

		[[nodiscard]] constexpr mat4 operator*(const mat4& rOther) const
		{
			mat4 matResult;
			for (int iRow = 0; iRow < 4; ++iRow)
				for (int iColumn = 0; iColumn < 4; ++iColumn)
				{
					float flSum = 0.f;
					for (int i = 0; i < 4; ++i)
						flSum += At(iRow, i) * rOther.At(i, iColumn);
					matResult.At(iRow, iColumn) = flSum;
				}
			return matResult;
		}

		[[nodiscard]] constexpr vec4 TransformRowVector(const vec3& rPoint) const
		{
			return {
				rPoint.x * At(0, 0) + rPoint.y * At(1, 0) + rPoint.z * At(2, 0) + At(3, 0),
				rPoint.x * At(0, 1) + rPoint.y * At(1, 1) + rPoint.z * At(2, 1) + At(3, 1),
				rPoint.x * At(0, 2) + rPoint.y * At(1, 2) + rPoint.z * At(2, 2) + At(3, 2),
				rPoint.x * At(0, 3) + rPoint.y * At(1, 3) + rPoint.z * At(2, 3) + At(3, 3),
			};
		}

		[[nodiscard]] constexpr mat4 Transposed() const
		{
			mat4 matResult;
			for (int iRow = 0; iRow < 4; ++iRow)
				for (int iColumn = 0; iColumn < 4; ++iColumn)
					matResult.At(iRow, iColumn) = At(iColumn, iRow);
			return matResult;
		}

		[[nodiscard]] static mat4 LookAtLH(const vec3& rEye, const vec3& rTarget, const vec3& rUp)
		{
			const vec3 vecZ = (rTarget - rEye).Normalized();
			const vec3 vecX = rUp.Cross(vecZ).Normalized();
			const vec3 vecY = vecZ.Cross(vecX);
			mat4 matResult;
			matResult.m = {
				vecX.x, vecY.x, vecZ.x, 0.f,
				vecX.y, vecY.y, vecZ.y, 0.f,
				vecX.z, vecY.z, vecZ.z, 0.f,
				-vecX.Dot(rEye), -vecY.Dot(rEye), -vecZ.Dot(rEye), 1.f,
			};
			return matResult;
		}

		[[nodiscard]] static mat4 PerspectiveFovLH(float flFovY, float flAspect, float flNear, float flFar)
		{
			const float flYScale = 1.f / std::tan(flFovY * .5f);
			const float flXScale = flYScale / flAspect;
			const float flRange = flFar / (flFar - flNear);
			mat4 matResult;
			matResult.m = {
				flXScale, 0.f, 0.f, 0.f,
				0.f, flYScale, 0.f, 0.f,
				0.f, 0.f, flRange, 1.f,
				0.f, 0.f, -flNear * flRange, 0.f,
			};
			return matResult;
		}

		[[nodiscard]] static mat4 RotationY(float flRadians)
		{
			const float flCos = std::cos(flRadians);
			const float flSin = std::sin(flRadians);
			mat4 matResult;
			matResult.m = {
				flCos, 0.f, -flSin, 0.f,
				0.f, 1.f, 0.f, 0.f,
				flSin, 0.f, flCos, 0.f,
				0.f, 0.f, 0.f, 1.f,
			};
			return matResult;
		}
	};

	[[nodiscard]] constexpr float Lerp(float flA, float flB, float flT) { return flA + (flB - flA) * flT; }
	[[nodiscard]] constexpr vec2 Lerp(const vec2& rA, const vec2& rB, float flT) { return { Lerp(rA.x, rB.x, flT), Lerp(rA.y, rB.y, flT) }; }
	[[nodiscard]] constexpr vec2 Min(const vec2& rA, const vec2& rB) { return { std::min(rA.x, rB.x), std::min(rA.y, rB.y) }; }
	[[nodiscard]] constexpr vec2 Max(const vec2& rA, const vec2& rB) { return { std::max(rA.x, rB.x), std::max(rA.y, rB.y) }; }
	[[nodiscard]] constexpr vec2 Clamp(const vec2& rValue, const vec2& rMin, const vec2& rMax) { return Min(Max(rValue, rMin), rMax); }

	[[nodiscard]] inline float ApproachExp(float flCurrent, float flTarget, float flSpeed, float flDeltaTime)
	{
		return flTarget + (flCurrent - flTarget) * std::exp(-flSpeed * flDeltaTime);
	}
}
