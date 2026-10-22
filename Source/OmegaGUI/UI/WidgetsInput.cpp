#include "OmegaGUI/UI/Internal.hpp"

#include <Windows.h>

#include <array>
#include <charconv>

namespace OmegaGUI
{
	namespace
	{
		[[nodiscard]] int StepBack(const std::string& strText, int iIndex)
		{
			if (iIndex <= 0)
				return 0;
			--iIndex;
			while (iIndex > 0 && (static_cast<unsigned char>(strText[iIndex]) & 0xC0) == 0x80)
				--iIndex;
			return iIndex;
		}

		[[nodiscard]] int StepForward(const std::string& strText, int iIndex)
		{
			const int iSize = static_cast<int>(strText.size());
			if (iIndex >= iSize)
				return iSize;
			++iIndex;
			while (iIndex < iSize && (static_cast<unsigned char>(strText[iIndex]) & 0xC0) == 0x80)
				++iIndex;
			return iIndex;
		}

		[[nodiscard]] int WordBack(const std::string& strText, int iIndex)
		{
			iIndex = StepBack(strText, iIndex);
			while (iIndex > 0 && strText[iIndex] == ' ')
				iIndex = StepBack(strText, iIndex);
			while (iIndex > 0 && strText[StepBack(strText, iIndex)] != ' ')
				iIndex = StepBack(strText, iIndex);
			return iIndex;
		}

		[[nodiscard]] int WordForward(const std::string& strText, int iIndex)
		{
			const int iSize = static_cast<int>(strText.size());
			while (iIndex < iSize && strText[iIndex] != ' ')
				iIndex = StepForward(strText, iIndex);
			while (iIndex < iSize && strText[iIndex] == ' ')
				iIndex = StepForward(strText, iIndex);
			return iIndex;
		}

		[[nodiscard]] std::string GetClipboardUTF8()
		{
			std::string strResult;
			if (!OpenClipboard(nullptr))
				return strResult;
			if (HANDLE hData = GetClipboardData(CF_UNICODETEXT))
				if (const auto* szWide = static_cast<const wchar_t*>(GlobalLock(hData)))
				{
					const int iBytes = WideCharToMultiByte(CP_UTF8, 0, szWide, -1, nullptr, 0, nullptr, nullptr);
					if (iBytes > 1)
					{
						strResult.resize(static_cast<std::size_t>(iBytes) - 1);
						WideCharToMultiByte(CP_UTF8, 0, szWide, -1, strResult.data(), iBytes, nullptr, nullptr);
					}
					GlobalUnlock(hData);
				}
			CloseClipboard();
			return strResult;
		}

		void SetClipboardUTF8(std::string_view strText)
		{
			if (!OpenClipboard(nullptr))
				return;
			EmptyClipboard();
			const int iWide = MultiByteToWideChar(CP_UTF8, 0, strText.data(), static_cast<int>(strText.size()), nullptr, 0);
			if (HGLOBAL hMemory = GlobalAlloc(GMEM_MOVEABLE, (static_cast<std::size_t>(iWide) + 1) * sizeof(wchar_t)))
			{
				auto* szWide = static_cast<wchar_t*>(GlobalLock(hMemory));
				MultiByteToWideChar(CP_UTF8, 0, strText.data(), static_cast<int>(strText.size()), szWide, iWide);
				szWide[iWide] = 0;
				GlobalUnlock(hMemory);
				if (!SetClipboardData(CF_UNICODETEXT, hMemory))
					GlobalFree(hMemory);
			}
			CloseClipboard();
			if (!strText.empty())
				PushToast(EToastKind::Info, "copied to clipboard", 1.5f);
		}

		[[nodiscard]] bool CharacterAllowed(char32_t uCodepoint, EInputTextFlags eFlags)
		{
			if (uCodepoint < 32 || uCodepoint == 127)
				return false;
			if (HasFlag(eFlags, EInputTextFlags::CharsDecimal))
				return (uCodepoint >= '0' && uCodepoint <= '9') || uCodepoint == '.' || uCodepoint == '-' || uCodepoint == '+' || uCodepoint == 'e' || uCodepoint == 'E';
			if (HasFlag(eFlags, EInputTextFlags::CharsHex))
				return (uCodepoint >= '0' && uCodepoint <= '9') || (uCodepoint >= 'a' && uCodepoint <= 'f') || (uCodepoint >= 'A' && uCodepoint <= 'F');
			return true;
		}

		[[nodiscard]] std::string DisplayText(const std::string& strText, EInputTextFlags eFlags)
		{
			if (!HasFlag(eFlags, EInputTextFlags::Password))
				return strText;
			std::size_t uCount = 0;
			for (std::size_t i = 0; i < strText.size(); ++i)
				if ((static_cast<unsigned char>(strText[i]) & 0xC0) != 0x80)
					++uCount;
			return std::string(uCount, '*');
		}

		[[nodiscard]] int DisplayIndex(const std::string& strText, int iIndex, EInputTextFlags eFlags)
		{
			if (!HasFlag(eFlags, EInputTextFlags::Password))
				return iIndex;
			int iCount = 0;
			for (int i = 0; i < iIndex && i < static_cast<int>(strText.size()); ++i)
				if ((static_cast<unsigned char>(strText[i]) & 0xC0) != 0x80)
					++iCount;
			return iCount;
		}

		[[nodiscard]] int IndexFromX(const std::string& strText, float flX, EInputTextFlags eFlags)
		{
			int iIndex = 0;
			float flWidth = 0.f;
			while (iIndex < static_cast<int>(strText.size()))
			{
				const int iNext = StepForward(strText, iIndex);
				const std::string_view strChar = HasFlag(eFlags, EInputTextFlags::Password) ? std::string_view("*") : std::string_view(strText).substr(iIndex, iNext - iIndex);
				const float flAdvance = MeasureText(strChar).x;
				if (flWidth + flAdvance * .5f > flX)
					break;
				flWidth += flAdvance;
				iIndex = iNext;
			}
			return iIndex;
		}

		void DeleteSelection(stTextState& rState)
		{
			if (rState.iSelectStart == rState.iSelectEnd)
				return;
			const int iMin = std::min(rState.iSelectStart, rState.iSelectEnd);
			const int iMax = std::max(rState.iSelectStart, rState.iSelectEnd);
			rState.strBuffer.erase(iMin, iMax - iMin);
			rState.iCursor = iMin;
			rState.iSelectStart = rState.iSelectEnd = iMin;
		}

		void InsertText(stTextState& rState, std::string_view strText)
		{
			DeleteSelection(rState);
			rState.strBuffer.insert(static_cast<std::size_t>(rState.iCursor), strText);
			rState.iCursor += static_cast<int>(strText.size());
			rState.iSelectStart = rState.iSelectEnd = rState.iCursor;
			rState.flBlink = 0.f;
		}

		void ActivateText(ID uID, const std::string& strText, EInputTextFlags eFlags, bool bOnScreenKeyboard)
		{
			stContext& g = Ctx();
			SetActiveID(uID, CurrentWindow());
			g.stText = {};
			g.stText.uID = uID;
			g.stText.strBuffer = strText;
			g.stText.strInitial = strText;
			g.stText.iCursor = static_cast<int>(strText.size());
			g.stText.iSelectStart = 0;
			g.stText.iSelectEnd = static_cast<int>(strText.size());
			g.stText.eFlags = eFlags;
			g.stText.bJustActivated = true;
			g.stText.bOnScreenKeyboard = bOnScreenKeyboard;
			if (bOnScreenKeyboard)
				g.uOskReturnWindow = CurrentWindow()->uID;
		}

		void DeactivateText()
		{
			stContext& g = Ctx();
			if (g.uActiveID == g.stText.uID)
				ClearActiveID();
			g.stText = {};
		}
	}

	bool InputTextFrame(ID uID, const rect& rcFrame, std::string& rText, std::string_view strHint, EInputTextFlags eFlags, bool bNavActivated)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		const stStyle& rStyle = g.stCurrentStyle;
		clInput& rInput = g.clInputState;
		const bool bReadOnly = HasFlag(eFlags, EInputTextFlags::ReadOnly);

		const bool bHovered = ItemHoverable(rcFrame, uID, EItemFlags::AllowWhenBlocked) && (g.uActiveID == 0 || g.uActiveID == uID || g.stText.uID == g.uActiveID);
		if (bHovered)
			g.eCursor = EMouseCursor::TextInput;

		const rect rcText{ rcFrame.min + vec2{ rStyle.vecFramePadding.x, 0.f }, rcFrame.max - vec2{ rStyle.vecFramePadding.x, 0.f } };
		bool bActive = g.stText.uID == uID;
		const bool bClicked = bHovered && rInput.IsMouseClicked(EMouseButton::Left);
		if (!bActive && !bReadOnly && (bClicked || bNavActivated))
		{
			const bool bOsk = bNavActivated && rInput.WasGamepadUsedLast() && !HasFlag(eFlags, EInputTextFlags::NoOnScreenKeyboard);
			ActivateText(uID, rText, eFlags, bOsk);
			if (bClicked)
			{
				g.stText.iCursor = IndexFromX(rText, rInput.GetMousePosition().x - rcText.min.x, eFlags);
				g.stText.iSelectStart = g.stText.iSelectEnd = g.stText.iCursor;
				g.bTextDragging = true;
				g.uNavID = uID;
				g.pNavWindow = pWindow;
			}
			bActive = true;
		}

		bool bEdited = false;
		bool bSubmitted = false;
		if (bActive)
		{
			KeepAliveID(uID);
			g.rcStickyActive = rcFrame;
			if (g.uActiveID != uID)
				SetActiveID(uID, pWindow);
			stTextState& rState = g.stText;
			rState.flBlink += g.flDeltaTime;

			if (rInput.IsMouseClicked(EMouseButton::Left) && !bHovered && !rState.bOnScreenKeyboard && !rState.bJustActivated)
			{
				DeactivateText();
				bActive = false;
			}
			else
			{
				if (bHovered && rInput.IsMouseClicked(EMouseButton::Left) && !rState.bJustActivated)
				{
					rState.iCursor = IndexFromX(rState.strBuffer, rInput.GetMousePosition().x - rcText.min.x + rState.flScrollX, eFlags);
					if (!rInput.IsShiftDown())
						rState.iSelectStart = rState.iCursor;
					rState.iSelectEnd = rState.iCursor;
					g.bTextDragging = true;
					if (rInput.IsMouseDoubleClicked(EMouseButton::Left))
					{
						rState.iSelectStart = 0;
						rState.iSelectEnd = rState.iCursor = static_cast<int>(rState.strBuffer.size());
					}
				}
				if (g.bTextDragging)
				{
					if (rInput.IsMouseDown(EMouseButton::Left))
					{
						rState.iCursor = IndexFromX(rState.strBuffer, rInput.GetMousePosition().x - rcText.min.x + rState.flScrollX, eFlags);
						rState.iSelectEnd = rState.iCursor;
					}
					else
						g.bTextDragging = false;
				}

				if (!rState.bOnScreenKeyboard)
				{
					const bool bCtrl = rInput.IsCtrlDown();
					const bool bShift = rInput.IsShiftDown();
					auto MoveCursor = [&](int iTarget) {
						if (!bShift && rState.iSelectStart != rState.iSelectEnd)
						{
							const int iMin = std::min(rState.iSelectStart, rState.iSelectEnd);
							const int iMax = std::max(rState.iSelectStart, rState.iSelectEnd);
							iTarget = iTarget < rState.iCursor ? iMin : iTarget > rState.iCursor ? iMax : iTarget;
						}
						if (!bShift)
							rState.iSelectStart = iTarget;
						rState.iCursor = iTarget;
						rState.iSelectEnd = iTarget;
						rState.flBlink = 0.f;
					};

					if (!bReadOnly)
						for (const char32_t uCodepoint : rInput.GetCharacters())
						{
							if (bCtrl && !rInput.IsAltDown())
								break;
							if (!CharacterAllowed(uCodepoint, eFlags))
								continue;
							std::string strUTF8;
							AppendUTF8(strUTF8, uCodepoint);
							InsertText(rState, strUTF8);
						}

					if (rInput.IsKeyPressed(VK_LEFT, true))
						MoveCursor(bCtrl ? WordBack(rState.strBuffer, rState.iCursor) : StepBack(rState.strBuffer, rState.iCursor));
					else if (rInput.IsKeyPressed(VK_RIGHT, true))
						MoveCursor(bCtrl ? WordForward(rState.strBuffer, rState.iCursor) : StepForward(rState.strBuffer, rState.iCursor));
					else if (rInput.IsKeyPressed(VK_HOME))
						MoveCursor(0);
					else if (rInput.IsKeyPressed(VK_END))
						MoveCursor(static_cast<int>(rState.strBuffer.size()));
					else if (rInput.IsKeyPressed(VK_BACK, true) && !bReadOnly)
					{
						if (rState.iSelectStart == rState.iSelectEnd)
						{
							const int iFrom = bCtrl ? WordBack(rState.strBuffer, rState.iCursor) : StepBack(rState.strBuffer, rState.iCursor);
							rState.iSelectStart = iFrom;
							rState.iSelectEnd = rState.iCursor;
						}
						DeleteSelection(rState);
					}
					else if (rInput.IsKeyPressed(VK_DELETE, true) && !bReadOnly)
					{
						if (rState.iSelectStart == rState.iSelectEnd)
						{
							rState.iSelectStart = rState.iCursor;
							rState.iSelectEnd = bCtrl ? WordForward(rState.strBuffer, rState.iCursor) : StepForward(rState.strBuffer, rState.iCursor);
						}
						DeleteSelection(rState);
					}
					else if (bCtrl && rInput.IsKeyPressed('A'))
					{
						rState.iSelectStart = 0;
						rState.iSelectEnd = rState.iCursor = static_cast<int>(rState.strBuffer.size());
					}
					else if (bCtrl && (rInput.IsKeyPressed('C') || rInput.IsKeyPressed('X')) && rState.iSelectStart != rState.iSelectEnd && !HasFlag(eFlags, EInputTextFlags::Password))
					{
						const int iMin = std::min(rState.iSelectStart, rState.iSelectEnd);
						const int iMax = std::max(rState.iSelectStart, rState.iSelectEnd);
						SetClipboardUTF8(std::string_view(rState.strBuffer).substr(iMin, iMax - iMin));
						if (rInput.IsKeyPressed('X') && !bReadOnly)
							DeleteSelection(rState);
					}
					else if (bCtrl && rInput.IsKeyPressed('V') && !bReadOnly)
					{
						std::string strPaste;
						const std::string strClipboard = GetClipboardUTF8();
						std::size_t uIndex = 0;
						while (uIndex < strClipboard.size())
						{
							const char32_t uCodepoint = DecodeUTF8(strClipboard, uIndex);
							if (CharacterAllowed(uCodepoint, eFlags))
								AppendUTF8(strPaste, uCodepoint);
						}
						InsertText(rState, strPaste);
					}

					if (rInput.IsKeyPressed(VK_RETURN) || rInput.IsKeyPressed(VK_SEPARATOR))
					{
						bSubmitted = true;
						if (!HasFlag(eFlags, EInputTextFlags::KeepFocusOnEnter))
						{
							rText = rState.strBuffer;
							DeactivateText();
							bActive = false;
						}
					}
					else if (rInput.IsKeyPressed(VK_ESCAPE))
					{
						rText = rState.strInitial;
						bEdited = true;
						DeactivateText();
						bActive = false;
					}
				}

				if (bActive)
				{
					if (rText != g.stText.strBuffer)
					{
						rText = g.stText.strBuffer;
						bEdited = true;
					}
					g.stText.bJustActivated = false;
				}
			}
		}

		clDrawList& rList = pWindow->drawList;
		const float flHover = AnimateBool(uID, bHovered || bActive);
		RenderFrame(rcFrame, rStyle.Get(EColor::Background), true, bActive ? rStyle.Get(EColor::Accent) : rStyle.Get(EColor::Border).Lerp(rStyle.Get(EColor::BorderHover), flHover), rStyle.flRoundingSmall);
		if (bActive)
			rList.AddGlowRect(rcFrame.min, rcFrame.max, WithGlobalAlpha(rStyle.Get(EColor::AccentGlow)), 3.f, rStyle.flRoundingSmall);

		const std::string& strSource = bActive ? g.stText.strBuffer : rText;
		const std::string strDisplay = DisplayText(strSource, eFlags);
		const float flTextY = std::floor(rcFrame.Center().y - GetTextLineHeight() * .5f);
		rList.PushClipRect(rcText);
		if (strSource.empty() && !strHint.empty() && !bActive)
			rList.AddText({ rcText.min.x, flTextY }, WithGlobalAlpha(rStyle.Get(EColor::TextDisabled)), strHint);
		if (bActive)
		{
			stTextState& rState = g.stText;
			const int iCursorDisplay = DisplayIndex(strSource, rState.iCursor, eFlags);
			const float flCursorX = MeasureText(std::string_view(strDisplay).substr(0, iCursorDisplay)).x;
			const float flWidth = rcText.Width();
			const float flCellWidth = MeasureText(" ").x;
			if (flCursorX - rState.flScrollX > flWidth - flCellWidth)
				rState.flScrollX = flCursorX - flWidth + flCellWidth;
			if (flCursorX < rState.flScrollX)
				rState.flScrollX = std::max(0.f, flCursorX - flWidth * .25f);
			const float flOriginX = rcText.min.x - rState.flScrollX;

			if (rState.iSelectStart != rState.iSelectEnd)
			{
				const int iMin = DisplayIndex(strSource, std::min(rState.iSelectStart, rState.iSelectEnd), eFlags);
				const int iMax = DisplayIndex(strSource, std::max(rState.iSelectStart, rState.iSelectEnd), eFlags);
				const float flX0 = MeasureText(std::string_view(strDisplay).substr(0, iMin)).x;
				const float flX1 = MeasureText(std::string_view(strDisplay).substr(0, iMax)).x;
				rList.AddRectFilled({ flOriginX + flX0, flTextY }, { flOriginX + flX1, flTextY + GetTextLineHeight() }, WithGlobalAlpha(rStyle.Get(EColor::Selection)));
			}
			rList.AddText({ flOriginX, flTextY }, WithGlobalAlpha(rStyle.Get(EColor::Text)), strDisplay);

			const float flBlinkRate = rStyle.flCursorBlinkRate;
			const bool bCaretVisible = rState.flBlink < .6f || std::fmod(rState.flBlink - .6f, flBlinkRate) < flBlinkRate * .5f;
			if (bCaretVisible)
			{
				const vec2 vecCaret{ std::floor(flOriginX + flCursorX), flTextY };
				rList.AddRectFilled(vecCaret, vecCaret + vec2{ std::max(2.f, flCellWidth * .55f), GetTextLineHeight() }, WithGlobalAlpha(rStyle.Get(EColor::Accent)));
			}
		}
		else
			rList.AddText({ rcText.min.x, flTextY }, WithGlobalAlpha(rStyle.Get(bReadOnly ? EColor::TextMuted : EColor::Text)), strDisplay);
		rList.PopClipRect();

		if (HasFlag(eFlags, EInputTextFlags::EnterReturnsTrue))
			return bSubmitted;
		return bEdited || (bSubmitted && HasFlag(eFlags, EInputTextFlags::KeepFocusOnEnter));
	}

	bool InputText(std::string_view strLabel, std::string* pText, std::string_view strHint, EInputTextFlags eFlags)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return false;
		const stStyle& rStyle = g.stCurrentStyle;
		const ID uID = pWindow->GetID(strLabel);
		rect rcTotal;
		float flLabelWidth = 0.f;
		const rect rcControl = LabeledControlRect(strLabel, FrameHeight(), rcTotal, flLabelWidth);
		ItemSize(rcTotal.Size(), rStyle.vecFramePadding.y);
		if (!ItemAdd(rcTotal, uID) && g.stText.uID != uID)
			return false;
		if (flLabelWidth > 0.f)
			RenderText({ rcTotal.min.x, std::floor(rcTotal.Center().y - GetTextLineHeight() * .5f) }, strLabel, rStyle.Get(EColor::Text));
		const bool bResult = InputTextFrame(uID, rcControl, *pText, strHint, eFlags, g.uNavActivateID == uID && g.pNavWindow == pWindow);
		pWindow->stLast.uID = uID;
		pWindow->stLast.rcItem = rcTotal;
		pWindow->stLast.bEdited = bResult;
		if (g.stText.uID != uID)
			RenderNavHighlight(rcTotal, uID);
		return bResult;
	}

	void SetInputTextValue(ID uID, std::string_view strText)
	{
		stTextState& rState = Ctx().stText;
		if (rState.uID != uID)
			return;
		rState.strBuffer = std::string(strText);
		rState.iCursor = rState.iSelectStart = rState.iSelectEnd = static_cast<int>(rState.strBuffer.size());
		rState.flBlink = 0.f;
	}

	namespace
	{
		bool HotkeyControl(std::string_view strLabel, KeyCode* pKey, std::string_view strBadge, bool bBadgeActive, bool bNotify)
		{
			stContext& g = Ctx();
			stWindow* pWindow = CurrentWindow();
			if (pWindow->bSkipItems)
				return false;
			const stStyle& rStyle = g.stCurrentStyle;
			clInput& rInput = g.clInputState;
			const ID uID = pWindow->GetID(strLabel);
			rect rcTotal;
			float flLabelWidth = 0.f;
			const rect rcControl = LabeledControlRect(strLabel, FrameHeight(), rcTotal, flLabelWidth);
			ItemSize(rcTotal.Size(), rStyle.vecFramePadding.y);
			if (!ItemAdd(rcTotal, uID) && g.uHotkeyCapture != uID)
				return false;

			bool bHovered = false;
			const bool bCapturing = g.uHotkeyCapture == uID;
			bool bChanged = false;
			if (!bCapturing)
			{
				if (ButtonBehavior(rcControl, uID, &bHovered, nullptr, EItemFlags::PressOnClick))
				{
					g.uHotkeyCapture = uID;
					g.iHotkeyCaptureFrame = g.iFrame;
					SetActiveID(uID, pWindow);
				}
			}
			else
			{
				KeepAliveID(uID);
				g.rcStickyActive = rcControl;
				bHovered = ItemHoverable(rcControl, uID);
				if (g.iFrame > g.iHotkeyCaptureFrame + 1)
				{
					KeyCode uPressed = 0;
					for (KeyCode uKey = 1; uKey < kKeyCount; ++uKey)
					{
						if (uKey == VK_SHIFT || uKey == VK_CONTROL || uKey == VK_MENU)
							continue;
						if (uKey >= GamepadKey(EGamepadButton::LeftStickUp) && uKey <= GamepadKey(EGamepadButton::LeftStickRight))
							continue;
						if (rInput.IsKeyPressed(uKey))
						{
							uPressed = uKey;
							break;
						}
					}
					if (uPressed)
					{
						if (uPressed == VK_ESCAPE || uPressed == GamepadKey(EGamepadButton::FaceRight))
							*pKey = 0;
						else
							*pKey = uPressed;
						bChanged = true;
						g.uHotkeyCapture = 0;
						ClearActiveID();
						MarkSettingsDirty();
					}
				}
			}

			const float flHover = AnimateBool(uID, bHovered || bCapturing || (g.uNavID == uID && g.bNavVisible));
			if (flLabelWidth > 0.f)
				RenderText({ rcTotal.min.x, std::floor(rcTotal.Center().y - GetTextLineHeight() * .5f) }, strLabel, rStyle.Get(EColor::Text).Lerp(rStyle.Get(EColor::TextHeading), flHover));
			RenderFrame(rcControl, rStyle.Get(EColor::Background), true, bCapturing ? rStyle.Get(EColor::Accent) : rStyle.Get(EColor::Border).Lerp(rStyle.Get(EColor::BorderHover), flHover), rStyle.flRoundingSmall);
			if (g.uHotkeyCapture == uID)
			{
				pWindow->drawList.AddGlowRect(rcControl.min, rcControl.max, WithGlobalAlpha(rStyle.Get(EColor::AccentGlow)), 3.f, rStyle.flRoundingSmall);
				const float flPulse = .55f + .45f * std::sin(static_cast<float>(g.dTime) * 6.f);
				RenderTextClipped(rcControl, "press a key...", rStyle.Get(EColor::Accent).MultiplyAlpha(flPulse), { .5f, .5f });
			}
			else
			{
				rect rcKey = rcControl;
				if (!strBadge.empty() && g.stShared.pFont)
				{
					const float flBadgeSize = std::floor(g.stShared.flFontSize * .86f);
					const vec2 vecBadge = g.stShared.pFont->MeasureText(strBadge, flBadgeSize);
					const float flPad = std::floor(flBadgeSize * .45f);
					const float flCenterY = rcControl.Center().y;
					const rect rcBadge{ { rcControl.max.x - flPad - vecBadge.x - flPad * 2.f, std::floor(flCenterY - vecBadge.y * .5f - flPad * .4f) }, { rcControl.max.x - flPad, std::ceil(flCenterY + vecBadge.y * .5f + flPad * .4f) } };
					const color colBadge = rStyle.Get(bBadgeActive ? EColor::Accent : EColor::TextMuted);
					pWindow->drawList.AddRectFilled(rcBadge.min, rcBadge.max, WithGlobalAlpha(colBadge.MultiplyAlpha(.16f)), rStyle.flRoundingSmall);
					pWindow->drawList.AddRect(rcBadge.min, rcBadge.max, WithGlobalAlpha(colBadge.MultiplyAlpha(.5f)), rStyle.flRoundingSmall);
					pWindow->drawList.AddText({ rcBadge.min.x + flPad, std::floor(flCenterY - vecBadge.y * .5f) }, WithGlobalAlpha(colBadge), strBadge, flBadgeSize);
					rcKey.max.x = rcBadge.min.x - flPad;
					if (bNotify)
					{
						const float flRadius = std::max(2.f, std::floor(flBadgeSize * .16f));
						pWindow->drawList.AddCircleFilled({ rcBadge.min.x - flPad - flRadius, flCenterY }, flRadius, WithGlobalAlpha(rStyle.Get(EColor::Accent)));
						rcKey.max.x -= flRadius * 2.f + flPad;
					}
				}
				const std::string strName = std::format("[ {} ]", KeyName(*pKey, rInput.GetGamepad().eType));
				RenderTextClipped(rcKey, strName, rStyle.Get(*pKey ? EColor::Accent : EColor::TextDisabled), { .5f, .5f });
			}
			RenderNavHighlight(rcTotal, uID);
			pWindow->stLast.bEdited = bChanged;
			return bChanged;
		}
	}

	bool Hotkey(std::string_view strLabel, KeyCode* pKey)
	{
		return HotkeyControl(strLabel, pKey, {}, false, false);
	}

	bool Hotkey(std::string_view strLabel, stHotkey* pHotkey, const stHotkeyText& stText)
	{
		if (CurrentWindow()->bSkipItems)
			return false;
		const std::array<std::string_view, 3> aModes = { stText.strAlwaysOn, stText.strHold, stText.strToggle };
		const std::size_t uMode = std::min<std::size_t>(static_cast<std::size_t>(pHotkey->eMode), aModes.size() - 1);
		bool bChanged = HotkeyControl(strLabel, &pHotkey->uKey, aModes[uMode], pHotkey->bActive, pHotkey->bNotify);
		if (!stText.strHint.empty())
			ItemTooltip(stText.strHint);
		if (BeginPopupContextItem())
		{
			TextDisabled(VisibleLabel(strLabel));
			Separator();
			for (std::size_t uIndex = 0; uIndex < aModes.size(); ++uIndex)
			{
				bool bSelected = uIndex == uMode;
				if (MenuItem(std::format("{}###mode{}", aModes[uIndex], uIndex), {}, &bSelected))
				{
					pHotkey->eMode = static_cast<EHotkeyMode>(uIndex);
					pHotkey->bActive = pHotkey->eMode == EHotkeyMode::AlwaysOn;
					bChanged = true;
				}
			}
			Separator();
			if (MenuItem(std::format("{}###notify", stText.strNotify), {}, &pHotkey->bNotify))
				bChanged = true;
			EndPopup();
		}
		if (bChanged)
			MarkSettingsDirty();
		return bChanged;
	}

	bool UpdateHotkey(stHotkey& rHotkey)
	{
		const bool bWasActive = rHotkey.bActive;
		const bool bKeysAllowed = !WantsKeyboard();
		const clInput& rInput = GetInput();
		switch (rHotkey.eMode)
		{
		case EHotkeyMode::AlwaysOn:
			rHotkey.bActive = true;
			break;
		case EHotkeyMode::Hold:
			rHotkey.bActive = bKeysAllowed && rHotkey.uKey != 0 && rInput.IsKeyDown(rHotkey.uKey);
			break;
		case EHotkeyMode::Toggle:
			if (bKeysAllowed && rHotkey.uKey != 0 && rInput.IsKeyPressed(rHotkey.uKey))
				rHotkey.bActive = !rHotkey.bActive;
			break;
		}
		return rHotkey.bActive != bWasActive;
	}

	bool ColorEdit(std::string_view strLabel, color* pColor, bool bAlpha)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return false;
		const stStyle& rStyle = g.stCurrentStyle;
		const ID uID = pWindow->GetID(strLabel);
		const ID uPopupID = HashString("#colorpopup", uID);
		const float flHeight = FrameHeight();
		rect rcTotal;
		float flLabelWidth = 0.f;
		const rect rcControl = LabeledControlRect(strLabel, flHeight, rcTotal, flLabelWidth);
		ItemSize(rcTotal.Size(), rStyle.vecFramePadding.y);
		const bool bVisible = ItemAdd(rcTotal, uID);

		bool bHovered = false;
		if (bVisible && ButtonBehavior(rcControl, uID, &bHovered, nullptr, EItemFlags::PressOnClick))
			OpenPopupEx(uPopupID, rcControl, false, true);

		auto DrawChecker = [&](clDrawList& rList, const rect& rcArea) {
			const float flCell = 5.f;
			rList.AddRectFilled(rcArea.min, rcArea.max, WithGlobalAlpha(color(200, 200, 200)));
			rList.PushClipRect(rcArea);
			for (float flY = rcArea.min.y; flY < rcArea.max.y; flY += flCell)
				for (float flX = rcArea.min.x + (static_cast<int>((flY - rcArea.min.y) / flCell) % 2 ? flCell : 0.f); flX < rcArea.max.x; flX += flCell * 2.f)
					rList.AddRectFilled({ flX, flY }, { flX + flCell, flY + flCell }, WithGlobalAlpha(color(140, 140, 140)));
			rList.PopClipRect();
		};

		if (bVisible)
		{
			const float flHover = AnimateBool(uID, bHovered || IsPopupOpenID(uPopupID) || (g.uNavID == uID && g.bNavVisible));
			if (flLabelWidth > 0.f)
				RenderText({ rcTotal.min.x, std::floor(rcTotal.Center().y - GetTextLineHeight() * .5f) }, strLabel, rStyle.Get(EColor::Text).Lerp(rStyle.Get(EColor::TextHeading), flHover));
			DrawChecker(pWindow->drawList, rcControl.Expanded(-1.f));
			pWindow->drawList.AddRectFilled(rcControl.min, rcControl.max, WithGlobalAlpha(*pColor), rStyle.flRoundingSmall);
			pWindow->drawList.AddRect(rcControl.min, rcControl.max, WithGlobalAlpha(rStyle.Get(EColor::Border).Lerp(rStyle.Get(EColor::Accent), flHover)), rStyle.flRoundingSmall);
			RenderNavHighlight(rcTotal, uID);
		}

		bool bChanged = false;
		if (!IsPopupOpenID(uPopupID))
			return false;
		SetNextWindowSizeConstraints({ 0.f, 0.f }, { 600.f, 600.f });
		if (!BeginPopupEx(uPopupID, EWindowFlags::None, {}))
			return false;

		stWindow* pPopup = CurrentWindow();
		float flH = 0.f, flS = 0.f, flV = 0.f;
		pColor->ToHSV(flH, flS, flV);
		float& rStoredHue = g.mapAnimations[HashString("#storedhue", uID)];
		if (flS <= 0.f || flV <= 0.f)
			flH = rStoredHue;
		float flA = pColor->A() / 255.f;

		const float flSize = 168.f * g.stSettings.flScale;
		const float flBar = 16.f * g.stSettings.flScale;
		stRegion& rRegion = pPopup->Region();
		const vec2 vecOrigin = rRegion.vecCursor;
		const rect rcSV{ vecOrigin, vecOrigin + vec2{ flSize, flSize } };
		const rect rcHue{ { rcSV.max.x + 8.f, rcSV.min.y }, { rcSV.max.x + 8.f + flBar, rcSV.max.y } };
		const rect rcAlpha{ { rcHue.max.x + 8.f, rcSV.min.y }, { rcHue.max.x + 8.f + flBar, rcSV.max.y } };
		const float flTotalWidth = (bAlpha ? rcAlpha.max.x : rcHue.max.x) - vecOrigin.x;
		ItemSize({ flTotalWidth, flSize });
		ItemAdd({ vecOrigin, vecOrigin + vec2{ flTotalWidth, flSize } }, 0);

		bool bHeldSV = false, bHeldHue = false, bHeldAlpha = false;
		ButtonBehavior(rcSV, HashString("#sv", uID), nullptr, &bHeldSV, EItemFlags::NoNav | EItemFlags::PressOnClick);
		ButtonBehavior(rcHue, HashString("#hue", uID), nullptr, &bHeldHue, EItemFlags::NoNav | EItemFlags::PressOnClick);
		if (bAlpha)
			ButtonBehavior(rcAlpha, HashString("#alpha", uID), nullptr, &bHeldAlpha, EItemFlags::NoNav | EItemFlags::PressOnClick);
		const vec2 vecMouse = g.clInputState.GetMousePosition();
		if (bHeldSV)
		{
			flS = std::clamp((vecMouse.x - rcSV.min.x) / rcSV.Width(), 0.f, 1.f);
			flV = 1.f - std::clamp((vecMouse.y - rcSV.min.y) / rcSV.Height(), 0.f, 1.f);
			bChanged = true;
		}
		if (bHeldHue)
		{
			flH = std::clamp((vecMouse.y - rcHue.min.y) / rcHue.Height(), 0.f, .9999f);
			bChanged = true;
		}
		if (bHeldAlpha)
		{
			flA = 1.f - std::clamp((vecMouse.y - rcAlpha.min.y) / rcAlpha.Height(), 0.f, 1.f);
			bChanged = true;
		}

		clDrawList& rList = pPopup->drawList;
		const color colHue = color::FromHSV(flH, 1.f, 1.f);
		rList.AddRectFilledMultiColor(rcSV.min, rcSV.max, Colors::White, colHue, colHue, Colors::White);
		rList.AddRectFilledMultiColor(rcSV.min, rcSV.max, Colors::Transparent.WithAlpha(0), Colors::Transparent.WithAlpha(0), Colors::Black, Colors::Black);
		rList.AddRect(rcSV.min, rcSV.max, rStyle.Get(EColor::Border));
		const vec2 vecPicker{ Lerp(rcSV.min.x, rcSV.max.x, flS), Lerp(rcSV.max.y, rcSV.min.y, flV) };
		rList.AddCircle(vecPicker, 5.f, Colors::Black, 0, 2.5f);
		rList.AddCircle(vecPicker, 5.f, Colors::White, 0, 1.2f);

		for (int i = 0; i < 6; ++i)
		{
			const float flY0 = Lerp(rcHue.min.y, rcHue.max.y, static_cast<float>(i) / 6.f);
			const float flY1 = Lerp(rcHue.min.y, rcHue.max.y, static_cast<float>(i + 1) / 6.f);
			const color colTop = color::FromHSV(static_cast<float>(i) / 6.f, 1.f, 1.f);
			const color colBottom = color::FromHSV(static_cast<float>(i + 1) / 6.f, 1.f, 1.f);
			rList.AddRectFilledMultiColor({ rcHue.min.x, flY0 }, { rcHue.max.x, flY1 }, colTop, colTop, colBottom, colBottom);
		}
		rList.AddRect(rcHue.min, rcHue.max, rStyle.Get(EColor::Border));
		const float flHueY = Lerp(rcHue.min.y, rcHue.max.y, flH);
		rList.AddRectFilled({ rcHue.min.x - 2.f, flHueY - 1.5f }, { rcHue.max.x + 2.f, flHueY + 1.5f }, rStyle.Get(EColor::TextHeading));

		if (bAlpha)
		{
			DrawChecker(rList, rcAlpha);
			const color colOpaque = color::FromHSV(flH, flS, flV, 1.f);
			rList.AddRectFilledMultiColor(rcAlpha.min, rcAlpha.max, colOpaque, colOpaque, colOpaque.WithAlpha(0), colOpaque.WithAlpha(0));
			rList.AddRect(rcAlpha.min, rcAlpha.max, rStyle.Get(EColor::Border));
			const float flAlphaY = Lerp(rcAlpha.max.y, rcAlpha.min.y, flA);
			rList.AddRectFilled({ rcAlpha.min.x - 2.f, flAlphaY - 1.5f }, { rcAlpha.max.x + 2.f, flAlphaY + 1.5f }, rStyle.Get(EColor::TextHeading));
		}

		PushItemWidth(flTotalWidth * .55f);
		float flHueDegrees = flH * 360.f;
		if (SliderFloat("hue", &flHueDegrees, 0.f, 359.f, "{:.0f}", 5.f))
		{
			flH = flHueDegrees / 360.f;
			bChanged = true;
		}
		bChanged |= SliderFloat("sat", &flS, 0.f, 1.f, "{:.2f}", .02f);
		bChanged |= SliderFloat("val", &flV, 0.f, 1.f, "{:.2f}", .02f);
		if (bAlpha)
			bChanged |= SliderFloat("alpha", &flA, 0.f, 1.f, "{:.2f}", .02f);
		std::string strHex = bAlpha ? std::format("{:02X}{:02X}{:02X}{:02X}", pColor->R(), pColor->G(), pColor->B(), pColor->A()) : std::format("{:02X}{:02X}{:02X}", pColor->R(), pColor->G(), pColor->B());
		bool bHexChanged = false;
		if (InputText("hex", &strHex, bAlpha ? "RRGGBBAA" : "RRGGBB", EInputTextFlags::CharsHex | EInputTextFlags::NoOnScreenKeyboard) && (strHex.size() == 6 || (bAlpha && strHex.size() == 8)))
		{
			std::uint32_t uValue = 0;
			if (std::from_chars(strHex.data(), strHex.data() + strHex.size(), uValue, 16).ec == std::errc())
			{
				if (strHex.size() == 6)
					*pColor = color(static_cast<std::uint8_t>(uValue >> 16), static_cast<std::uint8_t>(uValue >> 8), static_cast<std::uint8_t>(uValue), bAlpha ? pColor->A() : 255);
				else
					*pColor = color(static_cast<std::uint8_t>(uValue >> 24), static_cast<std::uint8_t>(uValue >> 16), static_cast<std::uint8_t>(uValue >> 8), static_cast<std::uint8_t>(uValue));
				float flNewS = 0.f, flNewV = 0.f;
				pColor->ToHSV(rStoredHue, flNewS, flNewV);
				bHexChanged = true;
			}
		}
		PopItemWidth();

		if (bChanged && !bHexChanged)
		{
			rStoredHue = flH;
			*pColor = color::FromHSV(flH, flS, flV, bAlpha ? flA : 1.f);
		}
		bChanged |= bHexChanged;
		if (bChanged)
			MarkSettingsDirty();
		EndPopup();
		return bChanged;
	}

	void RenderOnScreenKeyboard()
	{
		stContext& g = Ctx();
		stTextState& rState = g.stText;
		if (!rState.uID || !rState.bOnScreenKeyboard)
		{
			if (g.uOskReturnWindow)
			{
				if (stWindow* pReturn = FindWindowByID(g.uOskReturnWindow))
					FocusWindowInternal(pReturn);
				g.uOskReturnWindow = 0;
			}
			return;
		}

		const stStyle& rStyle = g.stCurrentStyle;
		clInput& rInput = g.clInputState;
		SetNextWindowPos({ g.vecDisplaySize.x * .5f, g.vecDisplaySize.y - 24.f }, ECondition::Always, { .5f, 1.f });
		const bool bOpen = Begin("keyboard##OmegaGUIOsk", nullptr, EWindowFlags::AlwaysAutoResize | EWindowFlags::NoSavedSettings | EWindowFlags::NoCollapse | EWindowFlags::NoMaximize | EWindowFlags::NoResize | EWindowFlags::NoMove);
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bAppearing)
		{
			FocusWindowInternal(pWindow);
			g.uNavID = 0;
		}
		BringWindowToFront(pWindow);
		KeepAliveID(rState.uID);

		if (bOpen)
		{
			const float flKey = std::floor(FrameHeight() * 1.25f);
			const vec2 vecPadding = rStyle.vecItemSpacing * .5f;
			stRegion& rRegion = pWindow->Region();

			const float flPreviewWidth = (flKey + vecPadding.x) * 10.f - vecPadding.x;
			const rect rcPreview{ rRegion.vecCursor, rRegion.vecCursor + vec2{ flPreviewWidth, FrameHeight() } };
			ItemSize(rcPreview.Size());
			ItemAdd(rcPreview, 0);
			RenderFrame(rcPreview, rStyle.Get(EColor::Background), true, rStyle.Get(EColor::Accent), rStyle.flRoundingSmall);
			const std::string strDisplay = DisplayText(rState.strBuffer, rState.eFlags);
			pWindow->drawList.PushClipRect(rcPreview.Expanded(-4.f));
			const float flTextWidth = MeasureText(strDisplay).x;
			const float flOffset = std::max(0.f, flTextWidth - (rcPreview.Width() - 24.f));
			const vec2 vecText{ rcPreview.min.x + rStyle.vecFramePadding.x - flOffset, std::floor(rcPreview.Center().y - GetTextLineHeight() * .5f) };
			pWindow->drawList.AddText(vecText, WithGlobalAlpha(rStyle.Get(EColor::Text)), strDisplay);
			if (std::fmod(static_cast<float>(g.dTime), rStyle.flCursorBlinkRate) < rStyle.flCursorBlinkRate * .5f)
				pWindow->drawList.AddRectFilled({ vecText.x + flTextWidth + 1.f, vecText.y }, { vecText.x + flTextWidth + 1.f + MeasureText(" ").x * .55f, vecText.y + GetTextLineHeight() }, WithGlobalAlpha(rStyle.Get(EColor::Accent)));
			pWindow->drawList.PopClipRect();

			static constexpr std::string_view s_aRows[] = { "1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm,./" };
			static constexpr std::string_view s_aShiftRows[] = { "!@#$%^&*()", "QWERTYUIOP", "ASDFGHJKL_", "ZXCVBNM<>?" };

			auto KeyButton = [&](std::string_view strLabel, float flWidth, bool bHighlighted) {
				const ID uKeyID = pWindow->GetID(strLabel);
				stRegion& rKeyRegion = pWindow->Region();
				const rect rcKey{ rKeyRegion.vecCursor, rKeyRegion.vecCursor + vec2{ flWidth, flKey } };
				ItemSize(rcKey.Size());
				ItemAdd(rcKey, uKeyID);
				const bool bHovered = ItemHoverable(rcKey, uKeyID, EItemFlags::AllowWhenBlocked);
				const bool bPressed = (bHovered && rInput.IsMouseClicked(EMouseButton::Left)) || (g.uNavActivateID == uKeyID && g.pNavWindow == pWindow);
				if (bPressed)
				{
					g.uNavID = uKeyID;
					g.pNavWindow = pWindow;
				}
				const float flHover = AnimateBool(uKeyID, bHovered || (g.uNavID == uKeyID && g.bNavVisible));
				RenderFrame(rcKey, (bHighlighted ? rStyle.Get(EColor::AccentSoft).WithAlpha(60) : rStyle.Get(EColor::Background)).Lerp(rStyle.Get(EColor::Hover), flHover), true,
					rStyle.Get(bHighlighted ? EColor::Accent : EColor::Border).Lerp(rStyle.Get(EColor::Accent), flHover), rStyle.flRoundingSmall);
				RenderTextClipped(rcKey, strLabel, rStyle.Get(EColor::Text).Lerp(rStyle.Get(EColor::Accent), flHover), { .5f, .5f });
				RenderNavHighlight(rcKey, uKeyID);
				return bPressed;
			};

			PushID("keys");
			for (std::size_t uRow = 0; uRow < 4; ++uRow)
			{
				const std::string_view strRow = rState.bOskShift ? s_aShiftRows[uRow] : s_aRows[uRow];
				for (std::size_t i = 0; i < strRow.size(); ++i)
				{
					if (i > 0)
						SameLine(0.f, vecPadding.x);
					PushID(static_cast<int>(uRow * 16 + i));
					if (KeyButton(strRow.substr(i, 1), flKey, false))
					{
						InsertText(rState, strRow.substr(i, 1));
						if (rState.bOskShift && uRow > 0)
							rState.bOskShift = false;
					}
					PopID();
				}
			}
			const float flWide = (flKey + vecPadding.x) * 2.f - vecPadding.x;
			const float flSpace = flPreviewWidth - flWide * 3.f - vecPadding.x * 3.f;
			if (KeyButton("shift", flWide, rState.bOskShift))
				rState.bOskShift = !rState.bOskShift;
			SameLine(0.f, vecPadding.x);
			if (KeyButton("space", flSpace, false))
				InsertText(rState, " ");
			SameLine(0.f, vecPadding.x);
			if (KeyButton("bksp", flWide, false))
			{
				if (rState.iSelectStart == rState.iSelectEnd)
					rState.iSelectStart = StepBack(rState.strBuffer, rState.iCursor);
				rState.iSelectEnd = rState.iCursor;
				DeleteSelection(rState);
			}
			SameLine(0.f, vecPadding.x);
			bool bDone = KeyButton("done", flWide, true);
			PopID();

			rState.iSelectStart = rState.iSelectEnd = rState.iCursor = std::min(rState.iCursor, static_cast<int>(rState.strBuffer.size()));
			if (rInput.IsGamepadPressed(EGamepadButton::FaceLeft, true))
			{
				rState.iSelectStart = StepBack(rState.strBuffer, rState.iCursor);
				rState.iSelectEnd = rState.iCursor;
				DeleteSelection(rState);
			}
			if (rInput.IsGamepadPressed(EGamepadButton::FaceUp, true))
				InsertText(rState, " ");
			if (rInput.IsGamepadPressed(EGamepadButton::LeftStick))
				rState.bOskShift = !rState.bOskShift;
			if (rInput.IsGamepadPressed(EGamepadButton::Start))
				bDone = true;

			Spacing();
			KeyHint(GamepadKey(EGamepadButton::FaceLeft), "bksp");
			SameLine(0.f, 14.f);
			KeyHint(GamepadKey(EGamepadButton::FaceUp), "space");
			SameLine(0.f, 14.f);
			KeyHint(GamepadKey(EGamepadButton::Start), "done");

			if (bDone)
				DeactivateText();
		}
		End();
	}

	void SetClipboardText(std::string_view strText)
	{
		SetClipboardUTF8(strText);
	}

	std::string GetClipboardText()
	{
		return GetClipboardUTF8();
	}
}
