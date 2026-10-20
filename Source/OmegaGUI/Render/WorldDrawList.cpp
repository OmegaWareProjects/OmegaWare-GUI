#include "OmegaGUI/Render/WorldDrawList.hpp"

namespace OmegaGUI
{
	namespace
	{
		constexpr float flNearW = 1e-4f;

		[[nodiscard]] vec4 LerpClip(const vec4& vecA, const vec4& vecB, float flT)
		{
			return { Lerp(vecA.x, vecB.x, flT), Lerp(vecA.y, vecB.y, flT), Lerp(vecA.z, vecB.z, flT), Lerp(vecA.w, vecB.w, flT) };
		}
	}

	void clWorldDrawList::Reset(const clFontAtlas* pAtlas, const clFont* pFont, float flFontSize)
	{
		for (stWorldBatch& rBatch : m_aBatches)
		{
			rBatch.aVertices.clear();
			rBatch.aIndices.clear();
		}
		m_pAtlas = pAtlas;
		m_pFont = pFont;
		m_flFontSize = flFontSize;
	}

	void clWorldDrawList::SetCamera(const mat4& matViewProjection, const vec2& vecViewportSize, bool bClipSpaceYDown)
	{
		m_matViewProjection = matViewProjection;
		m_vecViewportSize = Max(vecViewportSize, { 1.f, 1.f });
		m_bClipSpaceYDown = bClipSpaceYDown;
	}

	bool clWorldDrawList::IsEmpty() const
	{
		return std::ranges::all_of(m_aBatches, [](const stWorldBatch& rBatch) { return rBatch.aIndices.empty(); });
	}

	std::optional<vec4> clWorldDrawList::WorldToClip(const vec3& vecWorld) const
	{
		const vec4 vecClip = m_matViewProjection.TransformRowVector(vecWorld);
		if (vecClip.w <= flNearW)
			return std::nullopt;
		return vecClip;
	}

	std::optional<vec2> clWorldDrawList::WorldToScreen(const vec3& vecWorld) const
	{
		const auto optClip = WorldToClip(vecWorld);
		if (!optClip)
			return std::nullopt;
		const float flX = optClip->x / optClip->w;
		const float flY = optClip->y / optClip->w;
		return vec2{ (flX * .5f + .5f) * m_vecViewportSize.x, (m_bClipSpaceYDown ? (flY * .5f + .5f) : (.5f - flY * .5f)) * m_vecViewportSize.y };
	}

	vec4 clWorldDrawList::OffsetClip(const vec4& vecClip, const vec2& vecPixels) const
	{
		const float flX = vecPixels.x * 2.f / m_vecViewportSize.x * vecClip.w;
		const float flY = vecPixels.y * 2.f / m_vecViewportSize.y * vecClip.w * (m_bClipSpaceYDown ? 1.f : -1.f);
		return { vecClip.x + flX, vecClip.y + flY, vecClip.z, vecClip.w };
	}

	template <typename Fn>
	void clWorldDrawList::ForEachBatch(EDepthMode eMode, color col, const stWorldStyle& stStyle, Fn&& fnEmit)
	{
		switch (eMode)
		{
		case EDepthMode::Visible:
			fnEmit(m_aBatches[static_cast<std::size_t>(EDepthBatch::Tested)], col);
			break;
		case EDepthMode::XRay:
			fnEmit(m_aBatches[static_cast<std::size_t>(EDepthBatch::Behind)], stStyle.colOccluded);
			fnEmit(m_aBatches[static_cast<std::size_t>(EDepthBatch::Tested)], col);
			break;
		case EDepthMode::Always:
			fnEmit(m_aBatches[static_cast<std::size_t>(EDepthBatch::Always)], col);
			break;
		case EDepthMode::WriteDepth:
			fnEmit(m_aBatches[static_cast<std::size_t>(EDepthBatch::WriteDepth)], col);
			break;
		}
	}

	void clWorldDrawList::EmitClipQuad(stWorldBatch& rBatch, const std::array<vec4, 4>& aCorners, const std::array<vec2, 4>& aUVs, color col)
	{
		const auto uBase = static_cast<std::uint32_t>(rBatch.aVertices.size());
		for (std::size_t i = 0; i < 4; ++i)
			rBatch.aVertices.push_back({ aCorners[i], aUVs[i], col.uPacked });
		rBatch.aIndices.insert(rBatch.aIndices.end(), { uBase, uBase + 1, uBase + 2, uBase, uBase + 2, uBase + 3 });
	}

	void clWorldDrawList::EmitClipTriangle(stWorldBatch& rBatch, const vec4& vecA, const vec4& vecB, const vec4& vecC, color col)
	{
		const vec2 vecWhite = m_pAtlas ? m_pAtlas->GetWhiteUV() : vec2{};
		const auto uBase = static_cast<std::uint32_t>(rBatch.aVertices.size());
		rBatch.aVertices.push_back({ vecA, vecWhite, col.uPacked });
		rBatch.aVertices.push_back({ vecB, vecWhite, col.uPacked });
		rBatch.aVertices.push_back({ vecC, vecWhite, col.uPacked });
		rBatch.aIndices.insert(rBatch.aIndices.end(), { uBase, uBase + 1, uBase + 2 });
	}

	void clWorldDrawList::EmitClipLine(stWorldBatch& rBatch, vec4 vecA, vec4 vecB, color col, float flThickness)
	{
		if (vecA.w <= flNearW && vecB.w <= flNearW)
			return;
		if (vecA.w <= flNearW)
			vecA = LerpClip(vecA, vecB, (flNearW - vecA.w) / (vecB.w - vecA.w));
		else if (vecB.w <= flNearW)
			vecB = LerpClip(vecB, vecA, (flNearW - vecB.w) / (vecA.w - vecB.w));

		const vec2 vecScreenA{ vecA.x / vecA.w * m_vecViewportSize.x * .5f, vecA.y / vecA.w * m_vecViewportSize.y * .5f * (m_bClipSpaceYDown ? 1.f : -1.f) };
		const vec2 vecScreenB{ vecB.x / vecB.w * m_vecViewportSize.x * .5f, vecB.y / vecB.w * m_vecViewportSize.y * .5f * (m_bClipSpaceYDown ? 1.f : -1.f) };
		vec2 vecDirection = (vecScreenB - vecScreenA).Normalized();
		if (vecDirection.LengthSqr() == 0.f)
			vecDirection = { 1.f, 0.f };
		const vec2 vecNormal = vec2{ -vecDirection.y, vecDirection.x } * (flThickness * .5f);
		const vec2 vecWhite = m_pAtlas ? m_pAtlas->GetWhiteUV() : vec2{};
		EmitClipQuad(rBatch, { OffsetClip(vecA, vecNormal), OffsetClip(vecB, vecNormal), OffsetClip(vecB, -vecNormal), OffsetClip(vecA, -vecNormal) }, { vecWhite, vecWhite, vecWhite, vecWhite }, col);
	}

	void clWorldDrawList::AddLine(const vec3& vecA, const vec3& vecB, color col, EDepthMode eMode, const stWorldStyle& stStyle)
	{
		const vec4 vecClipA = m_matViewProjection.TransformRowVector(vecA);
		const vec4 vecClipB = m_matViewProjection.TransformRowVector(vecB);
		ForEachBatch(eMode, col, stStyle, [&](stWorldBatch& rBatch, color colBatch) { EmitClipLine(rBatch, vecClipA, vecClipB, colBatch, stStyle.flThickness); });
	}

	void clWorldDrawList::AddPolyline(std::span<const vec3> aPoints, color col, bool bClosed, EDepthMode eMode, const stWorldStyle& stStyle)
	{
		if (aPoints.size() < 2)
			return;
		const std::size_t uSegments = bClosed ? aPoints.size() : aPoints.size() - 1;
		for (std::size_t i = 0; i < uSegments; ++i)
			AddLine(aPoints[i], aPoints[(i + 1) % aPoints.size()], col, eMode, stStyle);
	}

	void clWorldDrawList::AddTriangleFilled(const vec3& vecA, const vec3& vecB, const vec3& vecC, color col, EDepthMode eMode, const stWorldStyle& stStyle)
	{
		const vec4 vecClipA = m_matViewProjection.TransformRowVector(vecA);
		const vec4 vecClipB = m_matViewProjection.TransformRowVector(vecB);
		const vec4 vecClipC = m_matViewProjection.TransformRowVector(vecC);
		ForEachBatch(eMode, col, stStyle, [&](stWorldBatch& rBatch, color colBatch) { EmitClipTriangle(rBatch, vecClipA, vecClipB, vecClipC, colBatch); });
	}

	void clWorldDrawList::AddQuadFilled(const vec3& vecA, const vec3& vecB, const vec3& vecC, const vec3& vecD, color col, EDepthMode eMode, const stWorldStyle& stStyle)
	{
		AddTriangleFilled(vecA, vecB, vecC, col, eMode, stStyle);
		AddTriangleFilled(vecA, vecC, vecD, col, eMode, stStyle);
	}

	void clWorldDrawList::AddOrientedBox(const std::array<vec3, 8>& aCorners, color col, EDepthMode eMode, const stWorldStyle& stStyle)
	{
		static constexpr std::array<std::pair<int, int>, 12> s_aEdges{ {
			{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
			{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
			{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
		} };
		for (const auto& [iA, iB] : s_aEdges)
			AddLine(aCorners[iA], aCorners[iB], col, eMode, stStyle);
	}

	void clWorldDrawList::AddBox(const vec3& vecMin, const vec3& vecMax, color col, EDepthMode eMode, const stWorldStyle& stStyle)
	{
		AddOrientedBox({
			vec3{ vecMin.x, vecMin.y, vecMin.z }, vec3{ vecMax.x, vecMin.y, vecMin.z }, vec3{ vecMax.x, vecMin.y, vecMax.z }, vec3{ vecMin.x, vecMin.y, vecMax.z },
			vec3{ vecMin.x, vecMax.y, vecMin.z }, vec3{ vecMax.x, vecMax.y, vecMin.z }, vec3{ vecMax.x, vecMax.y, vecMax.z }, vec3{ vecMin.x, vecMax.y, vecMax.z },
		}, col, eMode, stStyle);
	}

	void clWorldDrawList::AddBoxFilled(const vec3& vecMin, const vec3& vecMax, color col, EDepthMode eMode, const stWorldStyle& stStyle)
	{
		const vec3 a[8] = {
			{ vecMin.x, vecMin.y, vecMin.z }, { vecMax.x, vecMin.y, vecMin.z }, { vecMax.x, vecMin.y, vecMax.z }, { vecMin.x, vecMin.y, vecMax.z },
			{ vecMin.x, vecMax.y, vecMin.z }, { vecMax.x, vecMax.y, vecMin.z }, { vecMax.x, vecMax.y, vecMax.z }, { vecMin.x, vecMax.y, vecMax.z },
		};
		static constexpr int s_aFaces[6][4] = { { 0, 1, 2, 3 }, { 4, 7, 6, 5 }, { 0, 4, 5, 1 }, { 1, 5, 6, 2 }, { 2, 6, 7, 3 }, { 3, 7, 4, 0 } };
		for (const auto& aFace : s_aFaces)
			AddQuadFilled(a[aFace[0]], a[aFace[1]], a[aFace[2]], a[aFace[3]], col, eMode, stStyle);
	}

	void clWorldDrawList::AddCircle(const vec3& vecCenter, const vec3& vecNormal, float flRadius, color col, int iSegments, EDepthMode eMode, const stWorldStyle& stStyle)
	{
		const vec3 vecN = vecNormal.Normalized();
		const vec3 vecHelper = std::abs(vecN.y) < .99f ? vec3{ 0.f, 1.f, 0.f } : vec3{ 1.f, 0.f, 0.f };
		const vec3 vecU = vecN.Cross(vecHelper).Normalized();
		const vec3 vecV = vecN.Cross(vecU);
		std::vector<vec3> aPoints(static_cast<std::size_t>(std::max(iSegments, 3)));
		for (std::size_t i = 0; i < aPoints.size(); ++i)
		{
			const float flAngle = 2.f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(aPoints.size());
			aPoints[i] = vecCenter + vecU * (std::cos(flAngle) * flRadius) + vecV * (std::sin(flAngle) * flRadius);
		}
		AddPolyline(aPoints, col, true, eMode, stStyle);
	}

	void clWorldDrawList::AddSphere(const vec3& vecCenter, float flRadius, color col, int iSegments, EDepthMode eMode, const stWorldStyle& stStyle)
	{
		AddCircle(vecCenter, { 1.f, 0.f, 0.f }, flRadius, col, iSegments, eMode, stStyle);
		AddCircle(vecCenter, { 0.f, 1.f, 0.f }, flRadius, col, iSegments, eMode, stStyle);
		AddCircle(vecCenter, { 0.f, 0.f, 1.f }, flRadius, col, iSegments, eMode, stStyle);
	}

	void clWorldDrawList::AddGrid(const vec3& vecCenter, float flSize, int iCells, color col, EDepthMode eMode, const stWorldStyle& stStyle)
	{
		const float flHalf = flSize * .5f;
		const float flStep = flSize / static_cast<float>(std::max(iCells, 1));
		for (int i = 0; i <= iCells; ++i)
		{
			const float flOffset = -flHalf + flStep * static_cast<float>(i);
			AddLine(vecCenter + vec3{ flOffset, 0.f, -flHalf }, vecCenter + vec3{ flOffset, 0.f, flHalf }, col, eMode, stStyle);
			AddLine(vecCenter + vec3{ -flHalf, 0.f, flOffset }, vecCenter + vec3{ flHalf, 0.f, flOffset }, col, eMode, stStyle);
		}
	}

	void clWorldDrawList::AddAxes(const vec3& vecOrigin, float flLength, EDepthMode eMode)
	{
		AddLine(vecOrigin, vecOrigin + vec3{ flLength, 0.f, 0.f }, Colors::Red, eMode);
		AddLine(vecOrigin, vecOrigin + vec3{ 0.f, flLength, 0.f }, Colors::Green, eMode);
		AddLine(vecOrigin, vecOrigin + vec3{ 0.f, 0.f, flLength }, Colors::Blue, eMode);
	}

	void clWorldDrawList::AddScreenRect(const vec3& vecAnchor, const vec2& vecPixelMin, const vec2& vecPixelMax, color col, bool bFilled, EDepthMode eMode, const stWorldStyle& stStyle)
	{
		const auto optClip = WorldToClip(vecAnchor);
		if (!optClip)
			return;
		const vec4 vecClip = *optClip;
		const vec2 vecWhite = m_pAtlas ? m_pAtlas->GetWhiteUV() : vec2{};
		ForEachBatch(eMode, col, stStyle, [&](stWorldBatch& rBatch, color colBatch) {
			if (bFilled)
			{
				EmitClipQuad(rBatch, { OffsetClip(vecClip, vecPixelMin), OffsetClip(vecClip, { vecPixelMax.x, vecPixelMin.y }), OffsetClip(vecClip, vecPixelMax), OffsetClip(vecClip, { vecPixelMin.x, vecPixelMax.y }) }, { vecWhite, vecWhite, vecWhite, vecWhite }, colBatch);
				return;
			}
			const float flT = stStyle.flThickness;
			const std::array<std::pair<vec2, vec2>, 4> aEdges{ {
				{ vecPixelMin, { vecPixelMax.x, vecPixelMin.y + flT } },
				{ { vecPixelMin.x, vecPixelMax.y - flT }, vecPixelMax },
				{ { vecPixelMin.x, vecPixelMin.y + flT }, { vecPixelMin.x + flT, vecPixelMax.y - flT } },
				{ { vecPixelMax.x - flT, vecPixelMin.y + flT }, { vecPixelMax.x, vecPixelMax.y - flT } },
			} };
			for (const auto& [vecMin, vecMax] : aEdges)
				EmitClipQuad(rBatch, { OffsetClip(vecClip, vecMin), OffsetClip(vecClip, { vecMax.x, vecMin.y }), OffsetClip(vecClip, vecMax), OffsetClip(vecClip, { vecMin.x, vecMax.y }) }, { vecWhite, vecWhite, vecWhite, vecWhite }, colBatch);
		});
	}

	void clWorldDrawList::AddText(const vec3& vecAnchor, std::string_view strText, color col, bool bCentered, const vec2& vecPixelOffset, EDepthMode eMode, const stWorldStyle& stStyle)
	{
		if (!m_pFont || strText.empty())
			return;
		const auto optClip = WorldToClip(vecAnchor);
		if (!optClip)
			return;
		const vec4 vecClip = *optClip;

		const float flScale = m_flFontSize / m_pFont->GetSize();
		vec2 vecOrigin = vecPixelOffset;
		if (bCentered)
			vecOrigin -= m_pFont->MeasureText(strText, m_flFontSize) * .5f;
		const vec2 vecAnchorPixels{ (vecClip.x / vecClip.w * .5f + .5f) * m_vecViewportSize.x, (.5f - vecClip.y / vecClip.w * .5f) * m_vecViewportSize.y };
		vecOrigin += (vecAnchorPixels + vecOrigin).Floor() - (vecAnchorPixels + vecOrigin);

		ForEachBatch(eMode, col, stStyle, [&](stWorldBatch& rBatch, color colBatch) {
			vec2 vecCursor = vecOrigin;
			std::size_t uIndex = 0;
			while (uIndex < strText.size())
			{
				const char32_t uCodepoint = DecodeUTF8(strText, uIndex);
				if (uCodepoint == '\n')
				{
					vecCursor = { vecOrigin.x, vecCursor.y + m_pFont->GetLineHeight() * flScale };
					continue;
				}
				const stGlyph* pGlyph = m_pFont->FindGlyph(uCodepoint);
				if (!pGlyph)
					continue;
				if (pGlyph->bVisible)
				{
					const vec2 vecMin = vecCursor + pGlyph->vecOffsetMin * flScale;
					const vec2 vecMax = vecCursor + pGlyph->vecOffsetMax * flScale;
					EmitClipQuad(rBatch,
						{ OffsetClip(vecClip, vecMin), OffsetClip(vecClip, { vecMax.x, vecMin.y }), OffsetClip(vecClip, vecMax), OffsetClip(vecClip, { vecMin.x, vecMax.y }) },
						{ pGlyph->vecUVMin, vec2{ pGlyph->vecUVMax.x, pGlyph->vecUVMin.y }, pGlyph->vecUVMax, vec2{ pGlyph->vecUVMin.x, pGlyph->vecUVMax.y } },
						colBatch);
				}
				vecCursor.x += pGlyph->flAdvance * flScale;
			}
		});
	}
}
