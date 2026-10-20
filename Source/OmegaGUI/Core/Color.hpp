#pragma once

#include "OmegaGUI/Core/Math.hpp"

namespace OmegaGUI
{
	struct color
	{
		std::uint32_t uPacked = 0;

		constexpr color() = default;
		constexpr explicit color(std::uint32_t uRGBA) : uPacked(uRGBA) {}
		constexpr color(std::uint8_t byR, std::uint8_t byG, std::uint8_t byB, std::uint8_t byA = 255)
			: uPacked(static_cast<std::uint32_t>(byR) | (static_cast<std::uint32_t>(byG) << 8) | (static_cast<std::uint32_t>(byB) << 16) | (static_cast<std::uint32_t>(byA) << 24))
		{
		}

		[[nodiscard]] static constexpr color Hex(std::uint32_t uRGB, std::uint8_t byA = 255)
		{
			return { static_cast<std::uint8_t>((uRGB >> 16) & 0xFF), static_cast<std::uint8_t>((uRGB >> 8) & 0xFF), static_cast<std::uint8_t>(uRGB & 0xFF), byA };
		}

		[[nodiscard]] static constexpr color FromFloat(float flR, float flG, float flB, float flA = 1.f)
		{
			auto Quantize = [](float flValue) { return static_cast<std::uint8_t>(std::clamp(flValue, 0.f, 1.f) * 255.f + .5f); };
			return { Quantize(flR), Quantize(flG), Quantize(flB), Quantize(flA) };
		}

		[[nodiscard]] static color FromHSV(float flHue, float flSaturation, float flValue, float flAlpha = 1.f)
		{
			if (flSaturation <= 0.f)
				return FromFloat(flValue, flValue, flValue, flAlpha);
			flHue = std::fmod(flHue, 1.f) * 6.f;
			const int iSector = static_cast<int>(flHue);
			const float flFraction = flHue - static_cast<float>(iSector);
			const float flP = flValue * (1.f - flSaturation);
			const float flQ = flValue * (1.f - flSaturation * flFraction);
			const float flT = flValue * (1.f - flSaturation * (1.f - flFraction));
			switch (iSector)
			{
			case 0: return FromFloat(flValue, flT, flP, flAlpha);
			case 1: return FromFloat(flQ, flValue, flP, flAlpha);
			case 2: return FromFloat(flP, flValue, flT, flAlpha);
			case 3: return FromFloat(flP, flQ, flValue, flAlpha);
			case 4: return FromFloat(flT, flP, flValue, flAlpha);
			default: return FromFloat(flValue, flP, flQ, flAlpha);
			}
		}

		[[nodiscard]] constexpr std::uint8_t R() const { return static_cast<std::uint8_t>(uPacked & 0xFF); }
		[[nodiscard]] constexpr std::uint8_t G() const { return static_cast<std::uint8_t>((uPacked >> 8) & 0xFF); }
		[[nodiscard]] constexpr std::uint8_t B() const { return static_cast<std::uint8_t>((uPacked >> 16) & 0xFF); }
		[[nodiscard]] constexpr std::uint8_t A() const { return static_cast<std::uint8_t>((uPacked >> 24) & 0xFF); }

		[[nodiscard]] constexpr vec4 ToFloat() const
		{
			return { R() / 255.f, G() / 255.f, B() / 255.f, A() / 255.f };
		}

		void ToHSV(float& rHue, float& rSaturation, float& rValue) const
		{
			const vec4 vecColor = ToFloat();
			const float flMax = std::max({ vecColor.x, vecColor.y, vecColor.z });
			const float flMin = std::min({ vecColor.x, vecColor.y, vecColor.z });
			const float flDelta = flMax - flMin;
			rValue = flMax;
			rSaturation = flMax > 0.f ? flDelta / flMax : 0.f;
			if (flDelta <= 0.f)
			{
				rHue = 0.f;
				return;
			}
			if (flMax == vecColor.x)
				rHue = (vecColor.y - vecColor.z) / flDelta;
			else if (flMax == vecColor.y)
				rHue = 2.f + (vecColor.z - vecColor.x) / flDelta;
			else
				rHue = 4.f + (vecColor.x - vecColor.y) / flDelta;
			rHue /= 6.f;
			if (rHue < 0.f)
				rHue += 1.f;
		}

		[[nodiscard]] constexpr color WithAlpha(std::uint8_t byA) const { return color{ (uPacked & 0x00FFFFFFu) | (static_cast<std::uint32_t>(byA) << 24) }; }
		[[nodiscard]] constexpr color MultiplyAlpha(float flFactor) const
		{
			return WithAlpha(static_cast<std::uint8_t>(std::clamp(A() * flFactor, 0.f, 255.f)));
		}
		[[nodiscard]] constexpr color Lerp(const color& rOther, float flT) const
		{
			auto Mix = [flT](std::uint8_t byA, std::uint8_t byB) { return static_cast<std::uint8_t>(byA + (static_cast<int>(byB) - static_cast<int>(byA)) * flT); };
			return { Mix(R(), rOther.R()), Mix(G(), rOther.G()), Mix(B(), rOther.B()), Mix(A(), rOther.A()) };
		}
		[[nodiscard]] constexpr std::uint32_t ToARGB() const
		{
			return (static_cast<std::uint32_t>(A()) << 24) | (static_cast<std::uint32_t>(R()) << 16) | (static_cast<std::uint32_t>(G()) << 8) | B();
		}

		constexpr bool operator==(const color&) const = default;
	};

	namespace Colors
	{
		inline constexpr color White{ 255, 255, 255, 255 };
		inline constexpr color Black{ 0, 0, 0, 255 };
		inline constexpr color Transparent{ 0, 0, 0, 0 };
		inline constexpr color Red = color::Hex(0xFF7B7B);
		inline constexpr color Yellow = color::Hex(0xFFD479);
		inline constexpr color Green = color::Hex(0x7EE2A8);
		inline constexpr color Blue = color::Hex(0x85CFFF);
	}
}
