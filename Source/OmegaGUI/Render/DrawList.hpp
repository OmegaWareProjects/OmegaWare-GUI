#pragma once

#include "OmegaGUI/Core/Color.hpp"
#include "OmegaGUI/Render/Font.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace OmegaGUI
{
	struct stVertex
	{
		vec2 vecPosition;
		vec2 vecUV;
		std::uint32_t uColor = 0;
	};

	struct stDrawCommand
	{
		rect rcClip;
		TextureHandle hTexture = 0;
		std::uint32_t uIndexOffset = 0;
		std::uint32_t uIndexCount = 0;
	};

	enum class ECorner : std::uint8_t
	{
		None = 0,
		TopLeft = 1 << 0,
		TopRight = 1 << 1,
		BottomRight = 1 << 2,
		BottomLeft = 1 << 3,
		Top = TopLeft | TopRight,
		Bottom = BottomLeft | BottomRight,
		Left = TopLeft | BottomLeft,
		Right = TopRight | BottomRight,
		All = 0xF,
	};

	[[nodiscard]] constexpr ECorner operator|(ECorner eA, ECorner eB) { return static_cast<ECorner>(static_cast<std::uint8_t>(eA) | static_cast<std::uint8_t>(eB)); }
	[[nodiscard]] constexpr bool HasCorner(ECorner eSet, ECorner eTest) { return (static_cast<std::uint8_t>(eSet) & static_cast<std::uint8_t>(eTest)) != 0; }

	struct stDrawListShared
	{
		const clFontAtlas* pAtlas = nullptr;
		const clFont* pFont = nullptr;
		float flFontSize = 15.f;
		float flCurveTessellation = 1.25f;
		float flFringe = 1.f;
		bool bAntiAliased = true;
		rect rcFullClip{ -8192.f, -8192.f, 8192.f, 8192.f };
	};

	class clDrawList
	{
	public:
		void Reset(const stDrawListShared* pShared);

		void PushClipRect(const rect& rcClip, bool bIntersectWithCurrent = true);
		void PushClipRectFullScreen();
		void PopClipRect();
		[[nodiscard]] const rect& GetClipRect() const { return m_aClipStack.back(); }

		void PushTexture(TextureHandle hTexture);
		void PopTexture();

		void AddLine(const vec2& vecA, const vec2& vecB, color col, float flThickness = 1.f);
		void AddDashedLine(const vec2& vecA, const vec2& vecB, color col, float flDash = 4.f, float flGap = 3.f, float flThickness = 1.f);
		void AddRect(const vec2& vecMin, const vec2& vecMax, color col, float flRounding = 0.f, float flThickness = 1.f, ECorner eCorners = ECorner::All);
		void AddDashedRect(const vec2& vecMin, const vec2& vecMax, color col, float flDash = 4.f, float flGap = 3.f, float flThickness = 1.f);
		void AddRectFilled(const vec2& vecMin, const vec2& vecMax, color col, float flRounding = 0.f, ECorner eCorners = ECorner::All);
		void AddRectFilledMultiColor(const vec2& vecMin, const vec2& vecMax, color colTopLeft, color colTopRight, color colBottomRight, color colBottomLeft);
		void AddShadowRect(const vec2& vecMin, const vec2& vecMax, color col, float flSize, float flRounding = 0.f, const vec2& vecOffset = {});
		void AddGlowRect(const vec2& vecMin, const vec2& vecMax, color col, float flSize, float flRounding = 0.f);
		void AddCornerBox(const vec2& vecMin, const vec2& vecMax, color col, float flCornerFraction = .25f, float flThickness = 1.f);
		void AddCircle(const vec2& vecCenter, float flRadius, color col, int iSegments = 0, float flThickness = 1.f);
		void AddCircleFilled(const vec2& vecCenter, float flRadius, color col, int iSegments = 0);
		void AddArc(const vec2& vecCenter, float flRadius, float flAngleMin, float flAngleMax, color col, float flThickness = 1.f, int iSegments = 0);
		void AddTriangle(const vec2& vecA, const vec2& vecB, const vec2& vecC, color col, float flThickness = 1.f);
		void AddTriangleFilled(const vec2& vecA, const vec2& vecB, const vec2& vecC, color col);
		void AddQuad(const vec2& vecA, const vec2& vecB, const vec2& vecC, const vec2& vecD, color col, float flThickness = 1.f);
		void AddQuadFilled(const vec2& vecA, const vec2& vecB, const vec2& vecC, const vec2& vecD, color col);
		void AddPolyline(std::span<const vec2> aPoints, color col, bool bClosed, float flThickness);
		void AddConvexPolyFilled(std::span<const vec2> aPoints, color col);
		void AddBezierCubic(const vec2& vecP0, const vec2& vecP1, const vec2& vecP2, const vec2& vecP3, color col, float flThickness = 1.f, int iSegments = 0);
		void AddImage(TextureHandle hTexture, const vec2& vecMin, const vec2& vecMax, const vec2& vecUVMin = { 0.f, 0.f }, const vec2& vecUVMax = { 1.f, 1.f }, color col = Colors::White);
		void AddText(const vec2& vecPosition, color col, std::string_view strText, float flSize = 0.f, float flWrapWidth = 0.f, const clFont* pFont = nullptr);
		void AddTextShadowed(const vec2& vecPosition, color col, std::string_view strText, color colShadow = color(0, 0, 0, 200), float flSize = 0.f);
		void AddTextOutlined(const vec2& vecPosition, color col, std::string_view strText, color colOutline = color(0, 0, 0, 255), float flSize = 0.f);

		void PathClear() { m_aPath.clear(); }
		void PathLineTo(const vec2& vecPoint) { m_aPath.push_back(vecPoint); }
		void PathArcTo(const vec2& vecCenter, float flRadius, float flAngleMin, float flAngleMax, int iSegments = 0);
		void PathRect(const vec2& vecMin, const vec2& vecMax, float flRounding = 0.f, ECorner eCorners = ECorner::All);
		void PathStroke(color col, bool bClosed, float flThickness = 1.f);
		void PathFillConvex(color col);

		void PrimitiveReserve(std::size_t uIndexCount, std::size_t uVertexCount);
		void PrimitiveRectUV(const vec2& vecMin, const vec2& vecMax, const vec2& vecUVMin, const vec2& vecUVMax, color col);

		[[nodiscard]] const std::vector<stVertex>& GetVertices() const { return m_aVertices; }
		[[nodiscard]] const std::vector<std::uint32_t>& GetIndices() const { return m_aIndices; }
		[[nodiscard]] const std::vector<stDrawCommand>& GetCommands() const { return m_aCommands; }
		[[nodiscard]] bool IsEmpty() const { return m_aIndices.empty(); }
		[[nodiscard]] const stDrawListShared* GetShared() const { return m_pShared; }

	private:
		void UpdateCommand();
		void AddFilledInternal(std::span<const vec2> aPoints, color colInner, color colOuter, float flInnerInset, float flOuterExtent);
		[[nodiscard]] int AutoSegments(float flRadius) const;

		const stDrawListShared* m_pShared = nullptr;
		std::vector<stVertex> m_aVertices;
		std::vector<std::uint32_t> m_aIndices;
		std::vector<stDrawCommand> m_aCommands;
		std::vector<rect> m_aClipStack;
		std::vector<TextureHandle> m_aTextureStack;
		std::vector<vec2> m_aPath;
		std::vector<vec2> m_aScratchNormals;
	};

	struct stDrawData
	{
		std::vector<const clDrawList*> aLists;
		vec2 vecDisplayPosition;
		vec2 vecDisplaySize;
		vec2 vecFramebufferScale{ 1.f, 1.f };
		std::size_t uTotalVertices = 0;
		std::size_t uTotalIndices = 0;

		void Clear()
		{
			aLists.clear();
			uTotalVertices = uTotalIndices = 0;
		}

		void Add(const clDrawList* pList)
		{
			if (!pList || pList->IsEmpty())
				return;
			aLists.push_back(pList);
			uTotalVertices += pList->GetVertices().size();
			uTotalIndices += pList->GetIndices().size();
		}

		[[nodiscard]] std::array<float, 16> OrthoProjection() const
		{
			const float flL = vecDisplayPosition.x;
			const float flR = vecDisplayPosition.x + vecDisplaySize.x;
			const float flT = vecDisplayPosition.y;
			const float flB = vecDisplayPosition.y + vecDisplaySize.y;
			return {
				2.f / (flR - flL), 0.f, 0.f, 0.f,
				0.f, 2.f / (flT - flB), 0.f, 0.f,
				0.f, 0.f, .5f, 0.f,
				(flR + flL) / (flL - flR), (flT + flB) / (flB - flT), .5f, 1.f,
			};
		}
	};
}
