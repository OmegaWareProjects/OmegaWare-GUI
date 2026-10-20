#include "OmegaGUI/Render/Font.hpp"

#include <Windows.h>

#include <fstream>
#include <string>

#define STB_RECT_PACK_IMPLEMENTATION
#define STBRP_STATIC
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace OmegaGUI
{
	char32_t DecodeUTF8(std::string_view strText, std::size_t& rIndex)
	{
		const auto byLead = static_cast<unsigned char>(strText[rIndex]);
		int iLength = 1;
		char32_t uCodepoint = byLead;
		if (byLead >= 0xF0) { iLength = 4; uCodepoint = byLead & 0x07; }
		else if (byLead >= 0xE0) { iLength = 3; uCodepoint = byLead & 0x0F; }
		else if (byLead >= 0xC0) { iLength = 2; uCodepoint = byLead & 0x1F; }
		else if (byLead >= 0x80) { ++rIndex; return 0xFFFD; }

		if (rIndex + iLength > strText.size())
		{
			rIndex = strText.size();
			return 0xFFFD;
		}
		for (int i = 1; i < iLength; ++i)
		{
			const auto byNext = static_cast<unsigned char>(strText[rIndex + i]);
			if ((byNext & 0xC0) != 0x80)
			{
				rIndex += i;
				return 0xFFFD;
			}
			uCodepoint = (uCodepoint << 6) | (byNext & 0x3F);
		}
		rIndex += iLength;
		return uCodepoint;
	}

	void AppendUTF8(std::string& rOutput, char32_t uCodepoint)
	{
		if (uCodepoint < 0x80)
			rOutput.push_back(static_cast<char>(uCodepoint));
		else if (uCodepoint < 0x800)
		{
			rOutput.push_back(static_cast<char>(0xC0 | (uCodepoint >> 6)));
			rOutput.push_back(static_cast<char>(0x80 | (uCodepoint & 0x3F)));
		}
		else if (uCodepoint < 0x10000)
		{
			rOutput.push_back(static_cast<char>(0xE0 | (uCodepoint >> 12)));
			rOutput.push_back(static_cast<char>(0x80 | ((uCodepoint >> 6) & 0x3F)));
			rOutput.push_back(static_cast<char>(0x80 | (uCodepoint & 0x3F)));
		}
		else
		{
			rOutput.push_back(static_cast<char>(0xF0 | (uCodepoint >> 18)));
			rOutput.push_back(static_cast<char>(0x80 | ((uCodepoint >> 12) & 0x3F)));
			rOutput.push_back(static_cast<char>(0x80 | ((uCodepoint >> 6) & 0x3F)));
			rOutput.push_back(static_cast<char>(0x80 | (uCodepoint & 0x3F)));
		}
	}

	const stGlyph* clFont::FindGlyph(char32_t uCodepoint) const
	{
		if (uCodepoint < m_aLookup.size())
		{
			const std::int32_t iIndex = m_aLookup[uCodepoint];
			if (iIndex >= 0)
				return &m_aGlyphs[iIndex];
		}
		return m_pFallback;
	}

	std::size_t clFont::FindWrapPosition(std::string_view strText, float flSize, float flWrapWidth) const
	{
		const float flScale = flSize / m_flSize;
		float flLineWidth = 0.f;
		std::size_t uLastBreak = std::string_view::npos;
		std::size_t uIndex = 0;
		while (uIndex < strText.size())
		{
			const std::size_t uStart = uIndex;
			const char32_t uCodepoint = DecodeUTF8(strText, uIndex);
			if (uCodepoint == '\n')
				return uStart;
			if (uCodepoint == ' ' || uCodepoint == '\t')
				uLastBreak = uStart;
			const stGlyph* pGlyph = FindGlyph(uCodepoint);
			const float flAdvance = pGlyph ? pGlyph->flAdvance * flScale : 0.f;
			if (flLineWidth + flAdvance > flWrapWidth && uStart > 0)
				return uLastBreak != std::string_view::npos ? uLastBreak : uStart;
			flLineWidth += flAdvance;
		}
		return strText.size();
	}

	vec2 clFont::MeasureText(std::string_view strText, float flSize, float flWrapWidth) const
	{
		const float flScale = flSize / m_flSize;
		const float flLineHeight = m_flLineHeight * flScale;
		vec2 vecSize{ 0.f, flLineHeight };
		float flLineWidth = 0.f;
		std::size_t uIndex = 0;
		std::size_t uWrapAt = flWrapWidth > 0.f ? FindWrapPosition(strText, flSize, flWrapWidth) : std::string_view::npos;
		while (uIndex < strText.size())
		{
			if (uIndex == uWrapAt && strText[uIndex] != '\n')
			{
				vecSize.x = std::max(vecSize.x, flLineWidth);
				flLineWidth = 0.f;
				vecSize.y += flLineHeight;
				while (uIndex < strText.size() && strText[uIndex] == ' ')
					++uIndex;
				uWrapAt = uIndex + FindWrapPosition(strText.substr(uIndex), flSize, flWrapWidth);
				continue;
			}
			const char32_t uCodepoint = DecodeUTF8(strText, uIndex);
			if (uCodepoint == '\n')
			{
				vecSize.x = std::max(vecSize.x, flLineWidth);
				flLineWidth = 0.f;
				vecSize.y += flLineHeight;
				if (flWrapWidth > 0.f)
					uWrapAt = uIndex + FindWrapPosition(strText.substr(uIndex), flSize, flWrapWidth);
				continue;
			}
			if (const stGlyph* pGlyph = FindGlyph(uCodepoint))
				flLineWidth += pGlyph->flAdvance * flScale;
		}
		vecSize.x = std::max(vecSize.x, flLineWidth);
		return vecSize;
	}

	clFontAtlas::clFontAtlas() = default;
	clFontAtlas::~clFontAtlas() = default;

	std::span<const stGlyphRange> clFontAtlas::DefaultRanges()
	{
		static constexpr stGlyphRange s_aRanges[] = {
			{ 0x0020, 0x007E },
			{ 0x00A0, 0x00FF },
			{ 0x2010, 0x205E },
			{ 0x2190, 0x21FF },
			{ 0x2500, 0x259F },
			{ 0x25A0, 0x25FF },
			{ 0xFFFD, 0xFFFD },
		};
		return s_aRanges;
	}

	clFont* clFontAtlas::AddFontFromMemory(std::span<const std::uint8_t> aData, float flSizePixels, std::span<const stGlyphRange> aRanges)
	{
		if (aData.empty() || flSizePixels <= 0.f)
			return nullptr;
		if (aRanges.empty())
			aRanges = DefaultRanges();

		auto pFont = std::make_unique<clFont>();
		clFont* pRaw = pFont.get();
		m_aFonts.push_back(std::move(pFont));
		m_aSources.push_back({ std::vector<std::uint8_t>(aData.begin(), aData.end()), flSizePixels, std::vector<stGlyphRange>(aRanges.begin(), aRanges.end()), pRaw });
		m_bBuilt = false;
		return pRaw;
	}

	clFont* clFontAtlas::AddFontFromFile(const std::filesystem::path& pathFont, float flSizePixels, std::span<const stGlyphRange> aRanges)
	{
		std::ifstream fileFont(pathFont, std::ios::binary);
		if (!fileFont)
			return nullptr;
		const std::vector<std::uint8_t> aData{ std::istreambuf_iterator<char>(fileFont), std::istreambuf_iterator<char>() };
		return AddFontFromMemory(aData, flSizePixels, aRanges);
	}

	clFont* clFontAtlas::AddDefaultMonospace(float flSizePixels, std::span<const stGlyphRange> aRanges)
	{
		wchar_t szWindows[MAX_PATH]{};
		if (GetWindowsDirectoryW(szWindows, MAX_PATH) == 0)
			return nullptr;
		const std::filesystem::path pathFonts = std::filesystem::path(szWindows) / L"Fonts";
		for (const wchar_t* szCandidate : { L"CascadiaMono.ttf", L"consola.ttf", L"lucon.ttf", L"cour.ttf" })
			if (clFont* pFont = AddFontFromFile(pathFonts / szCandidate, flSizePixels, aRanges))
				return pFont;
		return nullptr;
	}

	void clFontAtlas::Clear()
	{
		m_aFonts.clear();
		m_aSources.clear();
		m_aPixelsRGBA.clear();
		m_iWidth = m_iHeight = 0;
		m_bBuilt = false;
	}

	bool clFontAtlas::Build()
	{
		if (m_aSources.empty())
			return false;

		constexpr int iAtlasWidth = 1024;
		constexpr int iMaxHeight = 8192;
		std::vector<std::uint8_t> aAlpha(static_cast<std::size_t>(iAtlasWidth) * iMaxHeight, 0);

		stbtt_pack_context stPack{};
		if (!stbtt_PackBegin(&stPack, aAlpha.data(), iAtlasWidth, iMaxHeight, iAtlasWidth, 1, nullptr))
			return false;
		stbtt_PackSetOversampling(&stPack, 2, 1);
		stbtt_PackSetSkipMissingCodepoints(&stPack, 1);

		struct stPackedRange
		{
			stGlyphRange stRange;
			std::vector<stbtt_packedchar> aChars;
		};
		std::vector<std::vector<stPackedRange>> aPacked(m_aSources.size());
		std::vector<stbtt_fontinfo> aInfos(m_aSources.size());

		for (std::size_t uSource = 0; uSource < m_aSources.size(); ++uSource)
		{
			stSource& rSource = m_aSources[uSource];
			if (!stbtt_InitFont(&aInfos[uSource], rSource.aData.data(), stbtt_GetFontOffsetForIndex(rSource.aData.data(), 0)))
			{
				stbtt_PackEnd(&stPack);
				return false;
			}

			std::vector<stbtt_pack_range> aRanges;
			aPacked[uSource].reserve(rSource.aRanges.size());
			for (const stGlyphRange& rRange : rSource.aRanges)
			{
				stPackedRange& rPacked = aPacked[uSource].emplace_back();
				rPacked.stRange = rRange;
				rPacked.aChars.resize(rRange.uLast - rRange.uFirst + 1);
			}
			for (stPackedRange& rPacked : aPacked[uSource])
			{
				stbtt_pack_range& rRange = aRanges.emplace_back();
				rRange.font_size = rSource.flSizePixels;
				rRange.first_unicode_codepoint_in_range = static_cast<int>(rPacked.stRange.uFirst);
				rRange.num_chars = static_cast<int>(rPacked.aChars.size());
				rRange.chardata_for_range = rPacked.aChars.data();
			}
			stbtt_PackFontRanges(&stPack, rSource.aData.data(), 0, aRanges.data(), static_cast<int>(aRanges.size()));
		}
		stbtt_PackEnd(&stPack);

		int iUsedHeight = 0;
		for (const auto& rRanges : aPacked)
			for (const stPackedRange& rPacked : rRanges)
				for (const stbtt_packedchar& rChar : rPacked.aChars)
					iUsedHeight = std::max<int>(iUsedHeight, rChar.y1);

		const int iWhiteY = iUsedHeight + 2;
		int iHeight = 1;
		while (iHeight < iWhiteY + 4)
			iHeight <<= 1;
		if (iHeight > iMaxHeight)
			return false;

		for (int y = 0; y < 3; ++y)
			for (int x = 0; x < 3; ++x)
				aAlpha[static_cast<std::size_t>(iWhiteY + y) * iAtlasWidth + x + 1] = 0xFF;

		m_iWidth = iAtlasWidth;
		m_iHeight = iHeight;
		m_vecWhiteUV = { 2.5f / iAtlasWidth, (iWhiteY + 1.5f) / iHeight };
		m_aPixelsRGBA.assign(static_cast<std::size_t>(m_iWidth) * m_iHeight * 4, 0xFF);
		for (std::size_t i = 0; i < static_cast<std::size_t>(m_iWidth) * m_iHeight; ++i)
			m_aPixelsRGBA[i * 4 + 3] = aAlpha[i];

		for (std::size_t uSource = 0; uSource < m_aSources.size(); ++uSource)
		{
			stSource& rSource = m_aSources[uSource];
			clFont& rFont = *rSource.pFont;
			const stbtt_fontinfo& rInfo = aInfos[uSource];

			int iAscent = 0, iDescent = 0, iLineGap = 0;
			stbtt_GetFontVMetrics(&rInfo, &iAscent, &iDescent, &iLineGap);
			const float flScale = stbtt_ScaleForPixelHeight(&rInfo, rSource.flSizePixels);
			rFont.m_flSize = rSource.flSizePixels;
			rFont.m_flAscent = std::round(iAscent * flScale);
			rFont.m_flLineHeight = std::round((iAscent - iDescent + iLineGap) * flScale);
			rFont.m_aGlyphs.clear();
			rFont.m_aLookup.clear();

			char32_t uMaxCodepoint = 0;
			for (const stPackedRange& rPacked : aPacked[uSource])
				uMaxCodepoint = std::max(uMaxCodepoint, rPacked.stRange.uLast);
			rFont.m_aLookup.assign(uMaxCodepoint + 1, -1);

			for (const stPackedRange& rPacked : aPacked[uSource])
				for (std::size_t i = 0; i < rPacked.aChars.size(); ++i)
				{
					const char32_t uCodepoint = rPacked.stRange.uFirst + static_cast<char32_t>(i);
					if (uCodepoint != ' ' && stbtt_FindGlyphIndex(&rInfo, static_cast<int>(uCodepoint)) == 0)
						continue;
					const stbtt_packedchar& rChar = rPacked.aChars[i];
					stGlyph stNew;
					stNew.uCodepoint = uCodepoint;
					stNew.flAdvance = std::round(rChar.xadvance);
					stNew.vecOffsetMin = { rChar.xoff, rChar.yoff + rFont.m_flAscent };
					stNew.vecOffsetMax = { rChar.xoff2, rChar.yoff2 + rFont.m_flAscent };
					stNew.vecUVMin = { static_cast<float>(rChar.x0) / m_iWidth, static_cast<float>(rChar.y0) / m_iHeight };
					stNew.vecUVMax = { static_cast<float>(rChar.x1) / m_iWidth, static_cast<float>(rChar.y1) / m_iHeight };
					stNew.bVisible = rChar.x1 > rChar.x0 && rChar.y1 > rChar.y0;
					rFont.m_aLookup[uCodepoint] = static_cast<std::int32_t>(rFont.m_aGlyphs.size());
					rFont.m_aGlyphs.push_back(stNew);
				}

			rFont.m_pFallback = nullptr;
			for (const char32_t uFallback : { char32_t(0xFFFD), char32_t('?'), char32_t(' ') })
				if (uFallback < rFont.m_aLookup.size() && rFont.m_aLookup[uFallback] >= 0)
				{
					rFont.m_pFallback = &rFont.m_aGlyphs[rFont.m_aLookup[uFallback]];
					break;
				}
		}

		m_bBuilt = true;
		return true;
	}
}
