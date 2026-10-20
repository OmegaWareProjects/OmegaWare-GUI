#pragma once

#include "OmegaGUI/Core/Math.hpp"

#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace OmegaGUI
{
	using TextureHandle = std::uint64_t;

	struct stGlyph
	{
		char32_t uCodepoint = 0;
		float flAdvance = 0.f;
		vec2 vecOffsetMin;
		vec2 vecOffsetMax;
		vec2 vecUVMin;
		vec2 vecUVMax;
		bool bVisible = false;
	};

	struct stGlyphRange
	{
		char32_t uFirst = 0;
		char32_t uLast = 0;
	};

	class clFont
	{
	public:
		[[nodiscard]] const stGlyph* FindGlyph(char32_t uCodepoint) const;
		[[nodiscard]] vec2 MeasureText(std::string_view strText, float flSize, float flWrapWidth = 0.f) const;
		[[nodiscard]] float GetSize() const { return m_flSize; }
		[[nodiscard]] float GetAscent() const { return m_flAscent; }
		[[nodiscard]] float GetLineHeight() const { return m_flLineHeight; }
		[[nodiscard]] std::size_t FindWrapPosition(std::string_view strText, float flSize, float flWrapWidth) const;

	private:
		friend class clFontAtlas;

		float m_flSize = 0.f;
		float m_flAscent = 0.f;
		float m_flLineHeight = 0.f;
		std::vector<stGlyph> m_aGlyphs;
		std::vector<std::int32_t> m_aLookup;
		const stGlyph* m_pFallback = nullptr;
	};

	class clFontAtlas
	{
	public:
		clFontAtlas();
		~clFontAtlas();
		clFontAtlas(const clFontAtlas&) = delete;
		clFontAtlas& operator=(const clFontAtlas&) = delete;

		clFont* AddFontFromMemory(std::span<const std::uint8_t> aData, float flSizePixels, std::span<const stGlyphRange> aRanges = {});
		clFont* AddFontFromFile(const std::filesystem::path& pathFont, float flSizePixels, std::span<const stGlyphRange> aRanges = {});
		clFont* AddDefaultMonospace(float flSizePixels, std::span<const stGlyphRange> aRanges = {});

		bool Build();
		void Clear();

		[[nodiscard]] bool IsBuilt() const { return m_bBuilt; }
		[[nodiscard]] const std::vector<std::uint8_t>& GetPixelsRGBA() const { return m_aPixelsRGBA; }
		[[nodiscard]] int GetWidth() const { return m_iWidth; }
		[[nodiscard]] int GetHeight() const { return m_iHeight; }
		[[nodiscard]] vec2 GetWhiteUV() const { return m_vecWhiteUV; }
		[[nodiscard]] clFont* GetFont(std::size_t uIndex = 0) const { return uIndex < m_aFonts.size() ? m_aFonts[uIndex].get() : nullptr; }
		[[nodiscard]] std::size_t GetFontCount() const { return m_aFonts.size(); }

		void SetTexture(TextureHandle hTexture) { m_hTexture = hTexture; }
		[[nodiscard]] TextureHandle GetTexture() const { return m_hTexture; }

		static std::span<const stGlyphRange> DefaultRanges();

	private:
		struct stSource
		{
			std::vector<std::uint8_t> aData;
			float flSizePixels = 0.f;
			std::vector<stGlyphRange> aRanges;
			clFont* pFont = nullptr;
		};

		std::vector<std::unique_ptr<clFont>> m_aFonts;
		std::vector<stSource> m_aSources;
		std::vector<std::uint8_t> m_aPixelsRGBA;
		int m_iWidth = 0;
		int m_iHeight = 0;
		vec2 m_vecWhiteUV;
		TextureHandle m_hTexture = 0;
		bool m_bBuilt = false;
	};

	[[nodiscard]] char32_t DecodeUTF8(std::string_view strText, std::size_t& rIndex);
	void AppendUTF8(std::string& rOutput, char32_t uCodepoint);
}
