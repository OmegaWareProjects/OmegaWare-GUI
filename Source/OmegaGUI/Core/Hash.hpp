#pragma once

#include <cstdint>
#include <string_view>

namespace OmegaGUI
{
	using ID = std::uint32_t;

	[[nodiscard]] constexpr ID HashBytes(const void* pData, std::size_t uSize, ID uSeed = 0x811C9DC5u)
	{
		const auto* pBytes = static_cast<const unsigned char*>(pData);
		ID uHash = uSeed;
		for (std::size_t i = 0; i < uSize; ++i)
		{
			uHash ^= pBytes[i];
			uHash *= 0x01000193u;
		}
		return uHash;
	}

	[[nodiscard]] constexpr ID HashString(std::string_view strText, ID uSeed = 0x811C9DC5u)
	{
		if (const std::size_t uOverride = strText.find("###"); uOverride != std::string_view::npos)
			strText = strText.substr(uOverride);
		ID uHash = uSeed;
		for (const char chLetter : strText)
		{
			uHash ^= static_cast<unsigned char>(chLetter);
			uHash *= 0x01000193u;
		}
		return uHash;
	}

	[[nodiscard]] constexpr std::string_view VisibleLabel(std::string_view strLabel)
	{
		const std::size_t uSplit = strLabel.find("##");
		return uSplit == std::string_view::npos ? strLabel : strLabel.substr(0, uSplit);
	}
}
