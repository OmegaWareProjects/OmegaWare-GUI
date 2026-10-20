#pragma once

#include <type_traits>

namespace OmegaGUI
{
	template <typename T>
	struct stEnableFlags : std::false_type
	{
	};

	template <typename T>
	concept FlagEnum = std::is_enum_v<T> && stEnableFlags<T>::value;

	template <FlagEnum T>
	[[nodiscard]] constexpr T operator|(T eA, T eB)
	{
		return static_cast<T>(static_cast<std::underlying_type_t<T>>(eA) | static_cast<std::underlying_type_t<T>>(eB));
	}

	template <FlagEnum T>
	[[nodiscard]] constexpr T operator&(T eA, T eB)
	{
		return static_cast<T>(static_cast<std::underlying_type_t<T>>(eA) & static_cast<std::underlying_type_t<T>>(eB));
	}

	template <FlagEnum T>
	[[nodiscard]] constexpr T operator~(T eA)
	{
		return static_cast<T>(~static_cast<std::underlying_type_t<T>>(eA));
	}

	template <FlagEnum T>
	constexpr T& operator|=(T& rA, T eB)
	{
		rA = rA | eB;
		return rA;
	}

	template <FlagEnum T>
	constexpr T& operator&=(T& rA, T eB)
	{
		rA = rA & eB;
		return rA;
	}

	template <FlagEnum T>
	[[nodiscard]] constexpr bool HasFlag(T eSet, T eFlag)
	{
		return (static_cast<std::underlying_type_t<T>>(eSet) & static_cast<std::underlying_type_t<T>>(eFlag)) != 0;
	}
}
