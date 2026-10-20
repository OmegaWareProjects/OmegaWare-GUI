#pragma once

#include "OmegaGUI/Core/Color.hpp"

#include <array>

namespace OmegaGUI
{
	enum class EColor : std::uint8_t
	{
		Background,
		Surface,
		Surface2,
		Hover,
		Text,
		TextMuted,
		TextDisabled,
		TextHeading,
		Border,
		BorderHover,
		Accent,
		AccentActive,
		AccentText,
		AccentSoft,
		AccentGlow,
		Danger,
		DangerSoft,
		Success,
		SuccessSoft,
		Warning,
		DotGrid,
		Shadow,
		ModalDim,
		Selection,
		ControlClose,
		ControlMinimize,
		ControlMaximize,
		Count,
	};

	enum class EWindowControls : std::uint8_t
	{
		Dots,
		Brackets,
	};

	struct stStyle
	{
		std::array<color, static_cast<std::size_t>(EColor::Count)> aColors{};

		float flFontSize = 15.f;
		float flRounding = 6.f;
		float flRoundingSmall = 3.f;
		float flWindowRounding = 10.f;
		float flBorderSize = 1.f;
		vec2 vecWindowPadding{ 16.f, 14.f };
		vec2 vecFramePadding{ 9.f, 5.f };
		vec2 vecItemSpacing{ 8.f, 7.f };
		vec2 vecItemInnerSpacing{ 6.f, 4.f };
		vec2 vecWindowMinSize{ 160.f, 80.f };
		float flTitleBarHeight = 32.f;
		float flTabHeight = 32.f;
		float flSidebarWidth = 170.f;
		float flScrollbarSize = 8.f;
		float flResizeBorder = 6.f;
		float flIndent = 18.f;
		float flSliderHeight = 6.f;
		float flAnimationSpeed = 16.f;
		float flTooltipDelay = .45f;
		float flShadowSize = 28.f;
		float flSnapDistance = 10.f;
		float flDotSpacing = 22.f;
		float flCursorBlinkRate = 1.1f;
		bool bDotGrid = true;
		bool bConsolePrefixes = true;
		bool bAnimations = true;
		EWindowControls eWindowControls = EWindowControls::Dots;

		[[nodiscard]] color Get(EColor eColor) const { return aColors[static_cast<std::size_t>(eColor)]; }
		void Set(EColor eColor, color col) { aColors[static_cast<std::size_t>(eColor)] = col; }

		void ScaleAllSizes(float flScale);
		void SetAccent(color colAccent);

		[[nodiscard]] static stStyle Console();
		[[nodiscard]] static stStyle ConsoleAmber();
		[[nodiscard]] static stStyle ConsoleGreen();
	};
}
