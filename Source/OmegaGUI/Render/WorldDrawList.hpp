#pragma once

#include "OmegaGUI/Core/Color.hpp"
#include "OmegaGUI/Render/Font.hpp"

#include <array>
#include <optional>
#include <string_view>
#include <vector>

namespace OmegaGUI
{
	enum class EDepthMode : std::uint8_t
	{
		Visible,
		XRay,
		Always,
		WriteDepth,
	};

	enum class EDepthBatch : std::uint8_t
	{
		WriteDepth,
		Behind,
		Tested,
		Always,
		Count,
	};

	struct stWorldVertex
	{
		vec4 vecClip;
		vec2 vecUV;
		std::uint32_t uColor = 0;
	};

	struct stWorldBatch
	{
		std::vector<stWorldVertex> aVertices;
		std::vector<std::uint32_t> aIndices;
	};

	struct stWorldPass
	{
		bool bReversedZ = false;
	};

	struct stWorldStyle
	{
		color colOccluded = color(255, 255, 255, 70);
		float flThickness = 1.5f;
	};

	class clWorldDrawList
	{
	public:
		void Reset(const clFontAtlas* pAtlas, const clFont* pFont, float flFontSize);
		void SetCamera(const mat4& matViewProjection, const vec2& vecViewportSize, bool bClipSpaceYDown = false);

		[[nodiscard]] std::optional<vec2> WorldToScreen(const vec3& vecWorld) const;
		[[nodiscard]] std::optional<vec4> WorldToClip(const vec3& vecWorld) const;

		void AddLine(const vec3& vecA, const vec3& vecB, color col, EDepthMode eMode = EDepthMode::Visible, const stWorldStyle& stStyle = {});
		void AddPolyline(std::span<const vec3> aPoints, color col, bool bClosed, EDepthMode eMode = EDepthMode::Visible, const stWorldStyle& stStyle = {});
		void AddTriangleFilled(const vec3& vecA, const vec3& vecB, const vec3& vecC, color col, EDepthMode eMode = EDepthMode::Visible, const stWorldStyle& stStyle = {});
		void AddQuadFilled(const vec3& vecA, const vec3& vecB, const vec3& vecC, const vec3& vecD, color col, EDepthMode eMode = EDepthMode::Visible, const stWorldStyle& stStyle = {});
		void AddBox(const vec3& vecMin, const vec3& vecMax, color col, EDepthMode eMode = EDepthMode::Visible, const stWorldStyle& stStyle = {});
		void AddBoxFilled(const vec3& vecMin, const vec3& vecMax, color col, EDepthMode eMode = EDepthMode::Visible, const stWorldStyle& stStyle = {});
		void AddOrientedBox(const std::array<vec3, 8>& aCorners, color col, EDepthMode eMode = EDepthMode::Visible, const stWorldStyle& stStyle = {});
		void AddCircle(const vec3& vecCenter, const vec3& vecNormal, float flRadius, color col, int iSegments = 32, EDepthMode eMode = EDepthMode::Visible, const stWorldStyle& stStyle = {});
		void AddSphere(const vec3& vecCenter, float flRadius, color col, int iSegments = 24, EDepthMode eMode = EDepthMode::Visible, const stWorldStyle& stStyle = {});
		void AddGrid(const vec3& vecCenter, float flSize, int iCells, color col, EDepthMode eMode = EDepthMode::Visible, const stWorldStyle& stStyle = {});
		void AddAxes(const vec3& vecOrigin, float flLength, EDepthMode eMode = EDepthMode::Always);

		void AddScreenRect(const vec3& vecAnchor, const vec2& vecPixelMin, const vec2& vecPixelMax, color col, bool bFilled, EDepthMode eMode = EDepthMode::Visible, const stWorldStyle& stStyle = {});
		void AddText(const vec3& vecAnchor, std::string_view strText, color col, bool bCentered = true, const vec2& vecPixelOffset = {}, EDepthMode eMode = EDepthMode::Visible, const stWorldStyle& stStyle = {});

		[[nodiscard]] const stWorldBatch& GetBatch(EDepthBatch eBatch) const { return m_aBatches[static_cast<std::size_t>(eBatch)]; }
		[[nodiscard]] bool IsEmpty() const;
		[[nodiscard]] TextureHandle GetTexture() const { return m_pAtlas ? m_pAtlas->GetTexture() : 0; }

	private:
		void EmitClipLine(stWorldBatch& rBatch, vec4 vecA, vec4 vecB, color col, float flThickness);
		void EmitClipTriangle(stWorldBatch& rBatch, const vec4& vecA, const vec4& vecB, const vec4& vecC, color col);
		void EmitClipQuad(stWorldBatch& rBatch, const std::array<vec4, 4>& aCorners, const std::array<vec2, 4>& aUVs, color col);
		[[nodiscard]] vec4 OffsetClip(const vec4& vecClip, const vec2& vecPixels) const;

		template <typename Fn>
		void ForEachBatch(EDepthMode eMode, color col, const stWorldStyle& stStyle, Fn&& fnEmit);

		std::array<stWorldBatch, static_cast<std::size_t>(EDepthBatch::Count)> m_aBatches;
		mat4 m_matViewProjection;
		vec2 m_vecViewportSize{ 1.f, 1.f };
		bool m_bClipSpaceYDown = false;
		const clFontAtlas* m_pAtlas = nullptr;
		const clFont* m_pFont = nullptr;
		float m_flFontSize = 15.f;
	};
}
