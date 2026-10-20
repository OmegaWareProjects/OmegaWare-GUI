#include "OmegaGUI/Render/DrawList.hpp"

namespace OmegaGUI
{
	namespace
	{
		constexpr float flPi = std::numbers::pi_v<float>;

		[[nodiscard]] vec2 EdgeNormal(const vec2& vecA, const vec2& vecB)
		{
			const vec2 vecDelta = (vecB - vecA).Normalized();
			return { vecDelta.y, -vecDelta.x };
		}
	}

	void clDrawList::Reset(const stDrawListShared* pShared)
	{
		m_pShared = pShared;
		m_aVertices.clear();
		m_aIndices.clear();
		m_aCommands.clear();
		m_aClipStack.clear();
		m_aTextureStack.clear();
		m_aPath.clear();
		m_aClipStack.push_back(pShared ? pShared->rcFullClip : rect{});
		m_aTextureStack.push_back(pShared && pShared->pAtlas ? pShared->pAtlas->GetTexture() : 0);
		UpdateCommand();
	}

	void clDrawList::UpdateCommand()
	{
		const rect& rcClip = m_aClipStack.back();
		const TextureHandle hTexture = m_aTextureStack.back();
		if (!m_aCommands.empty())
		{
			stDrawCommand& rLast = m_aCommands.back();
			if (rLast.rcClip == rcClip && rLast.hTexture == hTexture)
				return;
			if (rLast.uIndexCount == 0)
			{
				rLast.rcClip = rcClip;
				rLast.hTexture = hTexture;
				if (m_aCommands.size() >= 2)
				{
					const stDrawCommand& rPrevious = m_aCommands[m_aCommands.size() - 2];
					if (rPrevious.rcClip == rcClip && rPrevious.hTexture == hTexture)
						m_aCommands.pop_back();
				}
				return;
			}
		}
		m_aCommands.push_back({ rcClip, hTexture, static_cast<std::uint32_t>(m_aIndices.size()), 0 });
	}

	void clDrawList::PushClipRect(const rect& rcClip, bool bIntersectWithCurrent)
	{
		rect rcNew = rcClip;
		if (bIntersectWithCurrent && !m_aClipStack.empty())
			rcNew = rcNew.Clipped(m_aClipStack.back());
		rcNew.max = Max(rcNew.max, rcNew.min);
		m_aClipStack.push_back(rcNew);
		UpdateCommand();
	}

	void clDrawList::PushClipRectFullScreen()
	{
		m_aClipStack.push_back(m_pShared->rcFullClip);
		UpdateCommand();
	}

	void clDrawList::PopClipRect()
	{
		if (m_aClipStack.size() > 1)
			m_aClipStack.pop_back();
		UpdateCommand();
	}

	void clDrawList::PushTexture(TextureHandle hTexture)
	{
		m_aTextureStack.push_back(hTexture);
		UpdateCommand();
	}

	void clDrawList::PopTexture()
	{
		if (m_aTextureStack.size() > 1)
			m_aTextureStack.pop_back();
		UpdateCommand();
	}

	void clDrawList::PrimitiveReserve(std::size_t uIndexCount, std::size_t uVertexCount)
	{
		m_aVertices.reserve(m_aVertices.size() + uVertexCount);
		m_aIndices.reserve(m_aIndices.size() + uIndexCount);
		m_aCommands.back().uIndexCount += static_cast<std::uint32_t>(uIndexCount);
	}

	void clDrawList::PrimitiveRectUV(const vec2& vecMin, const vec2& vecMax, const vec2& vecUVMin, const vec2& vecUVMax, color col)
	{
		const auto uBase = static_cast<std::uint32_t>(m_aVertices.size());
		m_aVertices.push_back({ vecMin, vecUVMin, col.uPacked });
		m_aVertices.push_back({ { vecMax.x, vecMin.y }, { vecUVMax.x, vecUVMin.y }, col.uPacked });
		m_aVertices.push_back({ vecMax, vecUVMax, col.uPacked });
		m_aVertices.push_back({ { vecMin.x, vecMax.y }, { vecUVMin.x, vecUVMax.y }, col.uPacked });
		m_aIndices.insert(m_aIndices.end(), { uBase, uBase + 1, uBase + 2, uBase, uBase + 2, uBase + 3 });
	}

	int clDrawList::AutoSegments(float flRadius) const
	{
		const float flMaxError = m_pShared ? m_pShared->flCurveTessellation : 1.25f;
		if (flRadius <= flMaxError)
			return 8;
		const float flSegments = flPi / std::acos(1.f - std::min(flMaxError, flRadius) / flRadius);
		return std::clamp(static_cast<int>(std::ceil(flSegments)), 8, 128);
	}

	void clDrawList::AddPolyline(std::span<const vec2> aPoints, color col, bool bClosed, float flThickness)
	{
		const std::size_t uCount = aPoints.size();
		if (uCount < 2 || col.A() == 0)
			return;

		const bool bAntiAliased = m_pShared && m_pShared->bAntiAliased;
		const float flFringe = bAntiAliased ? m_pShared->flFringe : 0.f;
		const std::size_t uSegments = bClosed ? uCount : uCount - 1;

		if (flThickness < 1.f && bAntiAliased)
		{
			col = col.MultiplyAlpha(flThickness);
			flThickness = 1.f;
		}

		const float flHalfInner = std::max(0.f, (flThickness - flFringe) * .5f);
		const float flHalfOuter = flHalfInner + flFringe;

		m_aScratchNormals.resize(uCount);
		for (std::size_t i = 0; i < uSegments; ++i)
			m_aScratchNormals[i] = EdgeNormal(aPoints[i], aPoints[(i + 1) % uCount]);
		if (!bClosed)
			m_aScratchNormals[uCount - 1] = m_aScratchNormals[uCount - 2];

		const vec2 vecWhiteUV = m_pShared && m_pShared->pAtlas ? m_pShared->pAtlas->GetWhiteUV() : vec2{};
		const color colFaded = col.WithAlpha(0);
		const auto uBase = static_cast<std::uint32_t>(m_aVertices.size());
		const std::size_t uVerticesPerPoint = bAntiAliased ? 4 : 2;
		PrimitiveReserve(uSegments * (bAntiAliased ? 18 : 6), uCount * uVerticesPerPoint);

		for (std::size_t i = 0; i < uCount; ++i)
		{
			vec2 vecNormal = m_aScratchNormals[i];
			if (bClosed || (i > 0 && i < uCount - 1))
			{
				const vec2 vecPrevious = m_aScratchNormals[(i + uCount - 1) % uCount];
				vec2 vecAverage = (vecPrevious + m_aScratchNormals[i]) * .5f;
				const float flLengthSqr = vecAverage.LengthSqr();
				if (flLengthSqr > 1e-6f)
					vecAverage *= std::min(1.f / flLengthSqr, 4.f);
				vecNormal = vecAverage;
			}

			const vec2& vecPoint = aPoints[i];
			if (bAntiAliased)
			{
				m_aVertices.push_back({ vecPoint + vecNormal * flHalfOuter, vecWhiteUV, colFaded.uPacked });
				m_aVertices.push_back({ vecPoint + vecNormal * flHalfInner, vecWhiteUV, col.uPacked });
				m_aVertices.push_back({ vecPoint - vecNormal * flHalfInner, vecWhiteUV, col.uPacked });
				m_aVertices.push_back({ vecPoint - vecNormal * flHalfOuter, vecWhiteUV, colFaded.uPacked });
			}
			else
			{
				m_aVertices.push_back({ vecPoint + vecNormal * (flThickness * .5f), vecWhiteUV, col.uPacked });
				m_aVertices.push_back({ vecPoint - vecNormal * (flThickness * .5f), vecWhiteUV, col.uPacked });
			}
		}

		for (std::size_t i = 0; i < uSegments; ++i)
		{
			const std::size_t j = (i + 1) % uCount;
			const auto uA = static_cast<std::uint32_t>(uBase + i * uVerticesPerPoint);
			const auto uB = static_cast<std::uint32_t>(uBase + j * uVerticesPerPoint);
			const std::size_t uStrips = uVerticesPerPoint - 1;
			for (std::uint32_t uStrip = 0; uStrip < uStrips; ++uStrip)
				m_aIndices.insert(m_aIndices.end(), { uA + uStrip, uA + uStrip + 1, uB + uStrip + 1, uA + uStrip, uB + uStrip + 1, uB + uStrip });
		}
	}

	void clDrawList::AddFilledInternal(std::span<const vec2> aPoints, color colInner, color colOuter, float flInnerInset, float flOuterExtent)
	{
		const std::size_t uCount = aPoints.size();
		if (uCount < 3)
			return;

		float flArea = 0.f;
		for (std::size_t i = 0; i < uCount; ++i)
		{
			const vec2& vecA = aPoints[i];
			const vec2& vecB = aPoints[(i + 1) % uCount];
			flArea += vecA.x * vecB.y - vecB.x * vecA.y;
		}
		const float flWinding = flArea < 0.f ? -1.f : 1.f;

		const vec2 vecWhiteUV = m_pShared && m_pShared->pAtlas ? m_pShared->pAtlas->GetWhiteUV() : vec2{};
		const bool bFringe = flOuterExtent > 0.f;
		const auto uBase = static_cast<std::uint32_t>(m_aVertices.size());

		if (!bFringe)
		{
			PrimitiveReserve((uCount - 2) * 3, uCount);
			for (const vec2& vecPoint : aPoints)
				m_aVertices.push_back({ vecPoint, vecWhiteUV, colInner.uPacked });
			for (std::uint32_t i = 2; i < uCount; ++i)
				m_aIndices.insert(m_aIndices.end(), { uBase, uBase + i - 1, uBase + i });
			return;
		}

		m_aScratchNormals.resize(uCount);
		for (std::size_t i = 0; i < uCount; ++i)
			m_aScratchNormals[i] = EdgeNormal(aPoints[i], aPoints[(i + 1) % uCount]) * flWinding;

		PrimitiveReserve((uCount - 2) * 3 + uCount * 6, uCount * 2);
		for (std::size_t i = 0; i < uCount; ++i)
		{
			vec2 vecAverage = (m_aScratchNormals[(i + uCount - 1) % uCount] + m_aScratchNormals[i]) * .5f;
			const float flLengthSqr = vecAverage.LengthSqr();
			if (flLengthSqr > 1e-6f)
				vecAverage *= std::min(1.f / flLengthSqr, 4.f);
			m_aVertices.push_back({ aPoints[i] - vecAverage * flInnerInset, vecWhiteUV, colInner.uPacked });
			m_aVertices.push_back({ aPoints[i] + vecAverage * flOuterExtent, vecWhiteUV, colOuter.uPacked });
		}
		for (std::uint32_t i = 2; i < uCount; ++i)
			m_aIndices.insert(m_aIndices.end(), { uBase, uBase + (i - 1) * 2, uBase + i * 2 });
		for (std::uint32_t i = 0; i < uCount; ++i)
		{
			const std::uint32_t j = (i + 1) % static_cast<std::uint32_t>(uCount);
			const std::uint32_t uInnerA = uBase + i * 2, uOuterA = uInnerA + 1;
			const std::uint32_t uInnerB = uBase + j * 2, uOuterB = uInnerB + 1;
			m_aIndices.insert(m_aIndices.end(), { uInnerA, uInnerB, uOuterB, uInnerA, uOuterB, uOuterA });
		}
	}

	void clDrawList::AddConvexPolyFilled(std::span<const vec2> aPoints, color col)
	{
		if (col.A() == 0)
			return;
		const bool bAntiAliased = m_pShared && m_pShared->bAntiAliased;
		const float flFringe = bAntiAliased ? m_pShared->flFringe * .5f : 0.f;
		AddFilledInternal(aPoints, col, col.WithAlpha(0), flFringe, flFringe);
	}

	void clDrawList::PathArcTo(const vec2& vecCenter, float flRadius, float flAngleMin, float flAngleMax, int iSegments)
	{
		if (flRadius <= 0.f)
		{
			m_aPath.push_back(vecCenter);
			return;
		}
		if (iSegments <= 0)
			iSegments = std::max(2, static_cast<int>(AutoSegments(flRadius) * std::abs(flAngleMax - flAngleMin) / (2.f * flPi) + .5f));
		for (int i = 0; i <= iSegments; ++i)
		{
			const float flAngle = flAngleMin + (flAngleMax - flAngleMin) * (static_cast<float>(i) / iSegments);
			m_aPath.push_back({ vecCenter.x + std::cos(flAngle) * flRadius, vecCenter.y + std::sin(flAngle) * flRadius });
		}
	}

	void clDrawList::PathRect(const vec2& vecMin, const vec2& vecMax, float flRounding, ECorner eCorners)
	{
		flRounding = std::min({ flRounding, (vecMax.x - vecMin.x) * .5f, (vecMax.y - vecMin.y) * .5f });
		if (flRounding < .5f || eCorners == ECorner::None)
		{
			PathLineTo(vecMin);
			PathLineTo({ vecMax.x, vecMin.y });
			PathLineTo(vecMax);
			PathLineTo({ vecMin.x, vecMax.y });
			return;
		}
		const float flTL = HasCorner(eCorners, ECorner::TopLeft) ? flRounding : 0.f;
		const float flTR = HasCorner(eCorners, ECorner::TopRight) ? flRounding : 0.f;
		const float flBR = HasCorner(eCorners, ECorner::BottomRight) ? flRounding : 0.f;
		const float flBL = HasCorner(eCorners, ECorner::BottomLeft) ? flRounding : 0.f;
		PathArcTo({ vecMin.x + flTL, vecMin.y + flTL }, flTL, flPi, flPi * 1.5f);
		PathArcTo({ vecMax.x - flTR, vecMin.y + flTR }, flTR, flPi * 1.5f, flPi * 2.f);
		PathArcTo({ vecMax.x - flBR, vecMax.y - flBR }, flBR, 0.f, flPi * .5f);
		PathArcTo({ vecMin.x + flBL, vecMax.y - flBL }, flBL, flPi * .5f, flPi);
	}

	void clDrawList::PathStroke(color col, bool bClosed, float flThickness)
	{
		AddPolyline(m_aPath, col, bClosed, flThickness);
		m_aPath.clear();
	}

	void clDrawList::PathFillConvex(color col)
	{
		AddConvexPolyFilled(m_aPath, col);
		m_aPath.clear();
	}

	void clDrawList::AddLine(const vec2& vecA, const vec2& vecB, color col, float flThickness)
	{
		const vec2 aPoints[] = { vecA + vec2{ .5f, .5f }, vecB + vec2{ .5f, .5f } };
		AddPolyline(aPoints, col, false, flThickness);
	}

	void clDrawList::AddDashedLine(const vec2& vecA, const vec2& vecB, color col, float flDash, float flGap, float flThickness)
	{
		const vec2 vecDelta = vecB - vecA;
		const float flLength = vecDelta.Length();
		if (flLength <= 0.f)
			return;
		const vec2 vecDirection = vecDelta / flLength;
		const bool bHorizontal = std::abs(vecDirection.x) > .999f;
		const bool bVertical = std::abs(vecDirection.y) > .999f;
		if (bHorizontal || bVertical)
		{
			const vec2 vecPerpendicular = bHorizontal ? vec2{ 0.f, flThickness } : vec2{ flThickness, 0.f };
			for (float flStart = 0.f; flStart < flLength; flStart += flDash + flGap)
			{
				const vec2 vecFrom = (vecA + vecDirection * flStart).Floor();
				const vec2 vecTo = (vecA + vecDirection * std::min(flStart + flDash, flLength)).Floor();
				AddRectFilled(Min(vecFrom, vecTo), Max(vecFrom, vecTo) + vecPerpendicular, col);
			}
			return;
		}
		for (float flStart = 0.f; flStart < flLength; flStart += flDash + flGap)
			AddLine(vecA + vecDirection * flStart, vecA + vecDirection * std::min(flStart + flDash, flLength), col, flThickness);
	}

	void clDrawList::AddRect(const vec2& vecMin, const vec2& vecMax, color col, float flRounding, float flThickness, ECorner eCorners)
	{
		if (col.A() == 0)
			return;
		const float flHalf = flThickness * .5f;
		PathRect(vecMin + vec2{ flHalf, flHalf }, vecMax - vec2{ flHalf, flHalf }, std::max(0.f, flRounding - flHalf), eCorners);
		PathStroke(col, true, flThickness);
	}

	void clDrawList::AddDashedRect(const vec2& vecMin, const vec2& vecMax, color col, float flDash, float flGap, float flThickness)
	{
		AddDashedLine(vecMin, { vecMax.x, vecMin.y }, col, flDash, flGap, flThickness);
		AddDashedLine({ vecMin.x, vecMax.y - flThickness }, { vecMax.x, vecMax.y - flThickness }, col, flDash, flGap, flThickness);
		AddDashedLine(vecMin, { vecMin.x, vecMax.y }, col, flDash, flGap, flThickness);
		AddDashedLine({ vecMax.x - flThickness, vecMin.y }, { vecMax.x - flThickness, vecMax.y }, col, flDash, flGap, flThickness);
	}

	void clDrawList::AddRectFilled(const vec2& vecMin, const vec2& vecMax, color col, float flRounding, ECorner eCorners)
	{
		if (col.A() == 0 || vecMax.x <= vecMin.x || vecMax.y <= vecMin.y)
			return;
		if (flRounding < .5f || eCorners == ECorner::None)
		{
			const vec2 vecWhiteUV = m_pShared && m_pShared->pAtlas ? m_pShared->pAtlas->GetWhiteUV() : vec2{};
			PrimitiveReserve(6, 4);
			PrimitiveRectUV(vecMin, vecMax, vecWhiteUV, vecWhiteUV, col);
			return;
		}
		PathRect(vecMin, vecMax, flRounding, eCorners);
		PathFillConvex(col);
	}

	void clDrawList::AddRectFilledMultiColor(const vec2& vecMin, const vec2& vecMax, color colTopLeft, color colTopRight, color colBottomRight, color colBottomLeft)
	{
		const vec2 vecWhiteUV = m_pShared && m_pShared->pAtlas ? m_pShared->pAtlas->GetWhiteUV() : vec2{};
		const auto uBase = static_cast<std::uint32_t>(m_aVertices.size());
		PrimitiveReserve(6, 4);
		m_aVertices.push_back({ vecMin, vecWhiteUV, colTopLeft.uPacked });
		m_aVertices.push_back({ { vecMax.x, vecMin.y }, vecWhiteUV, colTopRight.uPacked });
		m_aVertices.push_back({ vecMax, vecWhiteUV, colBottomRight.uPacked });
		m_aVertices.push_back({ { vecMin.x, vecMax.y }, vecWhiteUV, colBottomLeft.uPacked });
		m_aIndices.insert(m_aIndices.end(), { uBase, uBase + 1, uBase + 2, uBase, uBase + 2, uBase + 3 });
	}

	void clDrawList::AddShadowRect(const vec2& vecMin, const vec2& vecMax, color col, float flSize, float flRounding, const vec2& vecOffset)
	{
		if (col.A() == 0 || flSize <= 0.f)
			return;
		PathRect(vecMin + vecOffset, vecMax + vecOffset, flRounding);
		AddFilledInternal(m_aPath, col, col.WithAlpha(0), 0.f, flSize);
		m_aPath.clear();
	}

	void clDrawList::AddGlowRect(const vec2& vecMin, const vec2& vecMax, color col, float flSize, float flRounding)
	{
		if (col.A() == 0 || flSize <= 0.f)
			return;
		PathRect(vecMin, vecMax, flRounding);
		const std::size_t uCount = m_aPath.size();
		std::vector<vec2> aInner(m_aPath.begin(), m_aPath.end());
		m_aPath.clear();
		float flArea = 0.f;
		for (std::size_t i = 0; i < uCount; ++i)
			flArea += aInner[i].x * aInner[(i + 1) % uCount].y - aInner[(i + 1) % uCount].x * aInner[i].y;
		const float flWinding = flArea < 0.f ? -1.f : 1.f;

		const vec2 vecWhiteUV = m_pShared && m_pShared->pAtlas ? m_pShared->pAtlas->GetWhiteUV() : vec2{};
		m_aScratchNormals.resize(uCount);
		for (std::size_t i = 0; i < uCount; ++i)
			m_aScratchNormals[i] = EdgeNormal(aInner[i], aInner[(i + 1) % uCount]) * flWinding;

		const auto uBase = static_cast<std::uint32_t>(m_aVertices.size());
		PrimitiveReserve(uCount * 6, uCount * 2);
		for (std::size_t i = 0; i < uCount; ++i)
		{
			vec2 vecAverage = (m_aScratchNormals[(i + uCount - 1) % uCount] + m_aScratchNormals[i]) * .5f;
			const float flLengthSqr = vecAverage.LengthSqr();
			if (flLengthSqr > 1e-6f)
				vecAverage *= std::min(1.f / flLengthSqr, 4.f);
			m_aVertices.push_back({ aInner[i], vecWhiteUV, col.uPacked });
			m_aVertices.push_back({ aInner[i] + vecAverage * flSize, vecWhiteUV, col.WithAlpha(0).uPacked });
		}
		for (std::uint32_t i = 0; i < uCount; ++i)
		{
			const std::uint32_t j = (i + 1) % static_cast<std::uint32_t>(uCount);
			const std::uint32_t uInnerA = uBase + i * 2, uOuterA = uInnerA + 1;
			const std::uint32_t uInnerB = uBase + j * 2, uOuterB = uInnerB + 1;
			m_aIndices.insert(m_aIndices.end(), { uInnerA, uInnerB, uOuterB, uInnerA, uOuterB, uOuterA });
		}
	}

	void clDrawList::AddCornerBox(const vec2& vecMin, const vec2& vecMax, color col, float flCornerFraction, float flThickness)
	{
		const float flWidth = (vecMax.x - vecMin.x) * flCornerFraction;
		const float flHeight = (vecMax.y - vecMin.y) * flCornerFraction;
		AddLine(vecMin, { vecMin.x + flWidth, vecMin.y }, col, flThickness);
		AddLine(vecMin, { vecMin.x, vecMin.y + flHeight }, col, flThickness);
		AddLine({ vecMax.x, vecMin.y }, { vecMax.x - flWidth, vecMin.y }, col, flThickness);
		AddLine({ vecMax.x, vecMin.y }, { vecMax.x, vecMin.y + flHeight }, col, flThickness);
		AddLine({ vecMin.x, vecMax.y }, { vecMin.x + flWidth, vecMax.y }, col, flThickness);
		AddLine({ vecMin.x, vecMax.y }, { vecMin.x, vecMax.y - flHeight }, col, flThickness);
		AddLine(vecMax, { vecMax.x - flWidth, vecMax.y }, col, flThickness);
		AddLine(vecMax, { vecMax.x, vecMax.y - flHeight }, col, flThickness);
	}

	void clDrawList::AddCircle(const vec2& vecCenter, float flRadius, color col, int iSegments, float flThickness)
	{
		if (col.A() == 0 || flRadius <= 0.f)
			return;
		if (iSegments <= 0)
			iSegments = AutoSegments(flRadius);
		PathArcTo(vecCenter, flRadius, 0.f, 2.f * flPi * (iSegments - 1) / iSegments, iSegments - 1);
		PathStroke(col, true, flThickness);
	}

	void clDrawList::AddCircleFilled(const vec2& vecCenter, float flRadius, color col, int iSegments)
	{
		if (col.A() == 0 || flRadius <= 0.f)
			return;
		if (iSegments <= 0)
			iSegments = AutoSegments(flRadius);
		PathArcTo(vecCenter, flRadius, 0.f, 2.f * flPi * (iSegments - 1) / iSegments, iSegments - 1);
		PathFillConvex(col);
	}

	void clDrawList::AddArc(const vec2& vecCenter, float flRadius, float flAngleMin, float flAngleMax, color col, float flThickness, int iSegments)
	{
		PathArcTo(vecCenter, flRadius, flAngleMin, flAngleMax, iSegments);
		PathStroke(col, false, flThickness);
	}

	void clDrawList::AddTriangle(const vec2& vecA, const vec2& vecB, const vec2& vecC, color col, float flThickness)
	{
		const vec2 aPoints[] = { vecA, vecB, vecC };
		AddPolyline(aPoints, col, true, flThickness);
	}

	void clDrawList::AddTriangleFilled(const vec2& vecA, const vec2& vecB, const vec2& vecC, color col)
	{
		const vec2 aPoints[] = { vecA, vecB, vecC };
		AddConvexPolyFilled(aPoints, col);
	}

	void clDrawList::AddQuad(const vec2& vecA, const vec2& vecB, const vec2& vecC, const vec2& vecD, color col, float flThickness)
	{
		const vec2 aPoints[] = { vecA, vecB, vecC, vecD };
		AddPolyline(aPoints, col, true, flThickness);
	}

	void clDrawList::AddQuadFilled(const vec2& vecA, const vec2& vecB, const vec2& vecC, const vec2& vecD, color col)
	{
		const vec2 aPoints[] = { vecA, vecB, vecC, vecD };
		AddConvexPolyFilled(aPoints, col);
	}

	void clDrawList::AddBezierCubic(const vec2& vecP0, const vec2& vecP1, const vec2& vecP2, const vec2& vecP3, color col, float flThickness, int iSegments)
	{
		if (iSegments <= 0)
			iSegments = std::clamp(static_cast<int>(((vecP1 - vecP0).Length() + (vecP2 - vecP1).Length() + (vecP3 - vecP2).Length()) / 6.f), 8, 64);
		m_aPath.push_back(vecP0);
		for (int i = 1; i <= iSegments; ++i)
		{
			const float flT = static_cast<float>(i) / iSegments;
			const float flU = 1.f - flT;
			const float flW0 = flU * flU * flU, flW1 = 3.f * flU * flU * flT, flW2 = 3.f * flU * flT * flT, flW3 = flT * flT * flT;
			m_aPath.push_back(vecP0 * flW0 + vecP1 * flW1 + vecP2 * flW2 + vecP3 * flW3);
		}
		PathStroke(col, false, flThickness);
	}

	void clDrawList::AddImage(TextureHandle hTexture, const vec2& vecMin, const vec2& vecMax, const vec2& vecUVMin, const vec2& vecUVMax, color col)
	{
		PushTexture(hTexture);
		PrimitiveReserve(6, 4);
		PrimitiveRectUV(vecMin, vecMax, vecUVMin, vecUVMax, col);
		PopTexture();
	}

	void clDrawList::AddText(const vec2& vecPosition, color col, std::string_view strText, float flSize, float flWrapWidth, const clFont* pFont)
	{
		if (col.A() == 0 || strText.empty() || !m_pShared)
			return;
		if (!pFont)
			pFont = m_pShared->pFont;
		if (!pFont)
			return;
		if (flSize <= 0.f)
			flSize = m_pShared->flFontSize;

		const float flScale = flSize / pFont->GetSize();
		const float flLineHeight = pFont->GetLineHeight() * flScale;
		const rect& rcClip = m_aClipStack.back();
		vec2 vecCursor = vecPosition.Floor();
		const float flStartX = vecCursor.x;

		std::size_t uIndex = 0;
		std::size_t uWrapAt = flWrapWidth > 0.f ? pFont->FindWrapPosition(strText, flSize, flWrapWidth) : std::string_view::npos;
		while (uIndex < strText.size())
		{
			if (uIndex == uWrapAt && strText[uIndex] != '\n')
			{
				vecCursor = { flStartX, vecCursor.y + flLineHeight };
				while (uIndex < strText.size() && strText[uIndex] == ' ')
					++uIndex;
				uWrapAt = uIndex + pFont->FindWrapPosition(strText.substr(uIndex), flSize, flWrapWidth);
				continue;
			}
			const char32_t uCodepoint = DecodeUTF8(strText, uIndex);
			if (uCodepoint == '\n')
			{
				vecCursor = { flStartX, vecCursor.y + flLineHeight };
				if (flWrapWidth > 0.f)
					uWrapAt = uIndex + pFont->FindWrapPosition(strText.substr(uIndex), flSize, flWrapWidth);
				if (vecCursor.y > rcClip.max.y)
					break;
				continue;
			}
			if (uCodepoint == '\r')
				continue;
			const stGlyph* pGlyph = pFont->FindGlyph(uCodepoint);
			if (!pGlyph)
				continue;
			if (pGlyph->bVisible)
			{
				const vec2 vecMin = vecCursor + pGlyph->vecOffsetMin * flScale;
				const vec2 vecMax = vecCursor + pGlyph->vecOffsetMax * flScale;
				if (vecMax.x >= rcClip.min.x && vecMin.x <= rcClip.max.x && vecMax.y >= rcClip.min.y && vecMin.y <= rcClip.max.y)
				{
					PrimitiveReserve(6, 4);
					PrimitiveRectUV(vecMin, vecMax, pGlyph->vecUVMin, pGlyph->vecUVMax, col);
				}
			}
			vecCursor.x += pGlyph->flAdvance * flScale;
		}
	}

	void clDrawList::AddTextShadowed(const vec2& vecPosition, color col, std::string_view strText, color colShadow, float flSize)
	{
		AddText(vecPosition + vec2{ 1.f, 1.f }, colShadow.MultiplyAlpha(col.A() / 255.f), strText, flSize);
		AddText(vecPosition, col, strText, flSize);
	}

	void clDrawList::AddTextOutlined(const vec2& vecPosition, color col, std::string_view strText, color colOutline, float flSize)
	{
		const color colFaded = colOutline.MultiplyAlpha(col.A() / 255.f);
		for (const vec2 vecOffset : { vec2{ -1.f, 0.f }, vec2{ 1.f, 0.f }, vec2{ 0.f, -1.f }, vec2{ 0.f, 1.f } })
			AddText(vecPosition + vecOffset, colFaded, strText, flSize);
		AddText(vecPosition, col, strText, flSize);
	}
}
