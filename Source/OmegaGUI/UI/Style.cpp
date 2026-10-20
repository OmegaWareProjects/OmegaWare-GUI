#include "OmegaGUI/UI/Style.hpp"

namespace OmegaGUI
{
	void stStyle::ScaleAllSizes(float flScale)
	{
		flFontSize *= flScale;
		flRounding *= flScale;
		flRoundingSmall *= flScale;
		flWindowRounding *= flScale;
		vecWindowPadding *= flScale;
		vecFramePadding *= flScale;
		vecItemSpacing *= flScale;
		vecItemInnerSpacing *= flScale;
		vecWindowMinSize *= flScale;
		flTitleBarHeight *= flScale;
		flTabHeight *= flScale;
		flSidebarWidth *= flScale;
		flScrollbarSize *= flScale;
		flResizeBorder *= flScale;
		flIndent *= flScale;
		flSliderHeight *= flScale;
		flShadowSize *= flScale;
		flSnapDistance *= flScale;
		flDotSpacing *= flScale;
	}

	void stStyle::SetAccent(color colAccent)
	{
		Set(EColor::Accent, colAccent);
		Set(EColor::AccentActive, colAccent.Lerp(Colors::White, .55f));
		Set(EColor::AccentSoft, colAccent.WithAlpha(26));
		Set(EColor::AccentGlow, colAccent.WithAlpha(64));
		Set(EColor::Selection, colAccent.WithAlpha(90));
	}

	stStyle stStyle::Console()
	{
		stStyle stResult;
		stResult.Set(EColor::Background, color::Hex(0x1A1A1A));
		stResult.Set(EColor::Surface, color::Hex(0x202020));
		stResult.Set(EColor::Surface2, color::Hex(0x292929));
		stResult.Set(EColor::Hover, color::Hex(0x2E2E2E));
		stResult.Set(EColor::Text, color::Hex(0xCCCCD4));
		stResult.Set(EColor::TextMuted, color::Hex(0x9A9AA2));
		stResult.Set(EColor::TextDisabled, color::Hex(0x696969));
		stResult.Set(EColor::TextHeading, color::Hex(0xE4E4EA));
		stResult.Set(EColor::Border, color::Hex(0x323232));
		stResult.Set(EColor::BorderHover, color::Hex(0x3D3D3D));
		stResult.Set(EColor::AccentText, color::Hex(0x1A1A1A));
		stResult.SetAccent(color::Hex(0x85CFFF));
		stResult.Set(EColor::AccentActive, color::Hex(0xC7E5FF));
		stResult.Set(EColor::Danger, color::Hex(0xFF7B7B));
		stResult.Set(EColor::DangerSoft, color::Hex(0xFF7B7B, 26));
		stResult.Set(EColor::Success, color::Hex(0x7EE2A8));
		stResult.Set(EColor::SuccessSoft, color::Hex(0x7EE2A8, 26));
		stResult.Set(EColor::Warning, color::Hex(0xFFD479));
		stResult.Set(EColor::DotGrid, color(255, 255, 255, 9));
		stResult.Set(EColor::Shadow, color(0, 0, 0, 90));
		stResult.Set(EColor::ModalDim, color(0, 0, 0, 130));
		stResult.Set(EColor::ControlClose, color::Hex(0xFF7B7B));
		stResult.Set(EColor::ControlMinimize, color::Hex(0xFFD479));
		stResult.Set(EColor::ControlMaximize, color::Hex(0x7EE2A8));
		return stResult;
	}

	stStyle stStyle::ConsoleAmber()
	{
		stStyle stResult = Console();
		stResult.SetAccent(color::Hex(0xFFB454));
		stResult.Set(EColor::Text, color::Hex(0xE8D9C0));
		return stResult;
	}

	stStyle stStyle::ConsoleGreen()
	{
		stStyle stResult = Console();
		stResult.SetAccent(color::Hex(0x7EE2A8));
		stResult.Set(EColor::Text, color::Hex(0xC8E6D2));
		return stResult;
	}
}
