#include "OmegaGUI/UI/Internal.hpp"

#include <charconv>

namespace OmegaGUI
{
	namespace
	{
		[[nodiscard]] std::string FormatValue(std::string_view strFormat, double dValue, bool bInteger)
		{
			try
			{
				if (bInteger)
				{
					const int iValue = static_cast<int>(dValue);
					return std::vformat(strFormat, std::make_format_args(iValue));
				}
				const float flValue = static_cast<float>(dValue);
				return std::vformat(strFormat, std::make_format_args(flValue));
			}
			catch (const std::format_error&)
			{
				return bInteger ? std::to_string(static_cast<int>(dValue)) : std::to_string(dValue);
			}
		}

		void TextEx(std::string_view strText, color col, float flWrapWidth = 0.f)
		{
			stWindow* pWindow = CurrentWindow();
			if (pWindow->bSkipItems)
				return;
			stRegion& rRegion = pWindow->Region();
			const vec2 vecPosition{ rRegion.vecCursor.x, rRegion.vecCursor.y + rRegion.flTextOffset };
			const vec2 vecSize = MeasureText(strText, flWrapWidth);
			const rect rcText{ vecPosition, vecPosition + vecSize };
			ItemSize({ vecSize.x, vecSize.y + rRegion.flTextOffset });
			if (!ItemAdd(rcText, 0))
				return;
			pWindow->drawList.AddText(vecPosition, WithGlobalAlpha(col), strText, 0.f, flWrapWidth);
		}

		void PrefixedText(std::string_view strPrefix, color colPrefix, std::string_view strText, color colText)
		{
			stWindow* pWindow = CurrentWindow();
			if (pWindow->bSkipItems)
				return;
			stRegion& rRegion = pWindow->Region();
			const vec2 vecPosition{ rRegion.vecCursor.x, rRegion.vecCursor.y + rRegion.flTextOffset };
			const float flPrefix = Style().bConsolePrefixes ? MeasureText(strPrefix).x : 0.f;
			const vec2 vecText = MeasureText(strText);
			const vec2 vecSize{ flPrefix + vecText.x, vecText.y };
			ItemSize(vecSize);
			if (!ItemAdd({ vecPosition, vecPosition + vecSize }, 0))
				return;
			if (flPrefix > 0.f)
				pWindow->drawList.AddText(vecPosition, WithGlobalAlpha(colPrefix), strPrefix);
			pWindow->drawList.AddText(vecPosition + vec2{ flPrefix, 0.f }, WithGlobalAlpha(colText), strText);
		}

		[[nodiscard]] float LabelY(const rect& rcFrame)
		{
			return std::floor(rcFrame.Center().y - GetTextLineHeight() * .5f);
		}

		bool SliderBehavior(std::string_view strLabel, double& rValue, double dMin, double dMax, bool bInteger, std::string_view strFormat, double dNavStep)
		{
			stContext& g = Ctx();
			stWindow* pWindow = CurrentWindow();
			if (pWindow->bSkipItems)
				return false;
			const stStyle& rStyle = g.stCurrentStyle;
			const ID uID = pWindow->GetID(strLabel);
			const float flHeight = FrameHeight();
			rect rcTotal;
			float flLabelWidth = 0.f;
			const rect rcControl = LabeledControlRect(strLabel, flHeight, rcTotal, flLabelWidth);
			ItemSize(rcTotal.Size(), rStyle.vecFramePadding.y);
			const ID uInputID = HashString("#input", uID);
			bool bEditing = g.stText.uID == uInputID;
			if (!ItemAdd(rcTotal, uID, EItemFlags::NavConsumesHorizontal) && !bEditing)
				return false;

			const bool bStartEditing = !bEditing && g.clInputState.IsCtrlDown() && g.clInputState.IsMouseClicked(EMouseButton::Left) && ItemHoverable(rcControl, uID);
			if (bEditing || bStartEditing)
			{
				std::string strText = bInteger ? std::format("{}", static_cast<long long>(rValue)) : std::format("{:g}", rValue);
				bool bTyped = false;
				pWindow->stLast.uID = uInputID;
				if (InputTextFrame(uInputID, rcControl, strText, {}, EInputTextFlags::CharsDecimal | EInputTextFlags::NoOnScreenKeyboard, bStartEditing))
				{
					double dParsed = 0.0;
					if (std::from_chars(strText.data(), strText.data() + strText.size(), dParsed).ec == std::errc())
					{
						dParsed = bInteger ? std::round(dParsed) : dParsed;
						bTyped = dParsed != rValue;
						rValue = dParsed;
					}
				}
				pWindow->stLast.uID = uID;
				pWindow->stLast.rcItem = rcTotal;
				if (flLabelWidth > 0.f)
					RenderText({ rcTotal.min.x, LabelY(rcTotal) }, strLabel, rStyle.Get(EColor::TextHeading));
				pWindow->stLast.bEdited = bTyped;
				if (bTyped)
					MarkSettingsDirty();
				return bTyped;
			}

			const float flValueWidth = std::max(MeasureText(FormatValue(strFormat, dMin, bInteger)).x, MeasureText(FormatValue(strFormat, dMax, bInteger)).x) + 4.f;
			const float flCenterY = std::floor(rcControl.Center().y);
			const rect rcTrack{ rcControl.min.x + 2.f, flCenterY - rStyle.flSliderHeight * .5f, std::max(rcControl.min.x + 20.f, rcControl.max.x - flValueWidth - rStyle.vecItemInnerSpacing.x * 2.f), flCenterY + rStyle.flSliderHeight * .5f };
			const rect rcHit{ { rcTrack.min.x - 4.f, rcControl.min.y }, { rcTrack.max.x + 4.f, rcControl.max.y } };

			bool bHovered = false, bHeld = false;
			ButtonBehavior(rcHit, uID, &bHovered, &bHeld, EItemFlags::PressOnClick);
			bool bChanged = false;
			const double dRange = dMax - dMin;
			if (bHeld && dRange != 0.0)
			{
				const double dT = std::clamp(static_cast<double>((g.clInputState.GetMousePosition().x - rcTrack.min.x) / rcTrack.Width()), 0.0, 1.0);
				double dNew = dMin + dRange * dT;
				if (bInteger)
					dNew = std::round(dNew);
				if (dNew != rValue)
				{
					rValue = dNew;
					bChanged = true;
				}
			}
			if (g.uNavID == uID && g.pNavWindow == pWindow && g.iNavAdjust != 0)
			{
				double dStep = dNavStep > 0.0 ? dNavStep : (bInteger ? 1.0 : dRange / 50.0);
				if (g.clInputState.IsGamepadDown(EGamepadButton::RightShoulder) || g.clInputState.IsShiftDown())
					dStep *= 10.0;
				double dNew = std::clamp(rValue + dStep * g.iNavAdjust, std::min(dMin, dMax), std::max(dMin, dMax));
				if (bInteger)
					dNew = std::round(dNew);
				if (dNew != rValue)
				{
					rValue = dNew;
					bChanged = true;
				}
			}

			clDrawList& rList = pWindow->drawList;
			const float flHover = AnimateBool(uID, bHovered || bHeld || (g.uNavID == uID && g.bNavVisible));
			const double dFraction = dRange != 0.0 ? std::clamp((rValue - dMin) / dRange, 0.0, 1.0) : 0.0;
			const float flKnobX = Lerp(rcTrack.min.x, rcTrack.max.x, Animate(HashString("#knob", uID), static_cast<float>(dFraction), 30.f));

			if (flLabelWidth > 0.f)
				RenderText({ rcTotal.min.x, LabelY(rcTotal) }, strLabel, rStyle.Get(EColor::Text).Lerp(rStyle.Get(EColor::TextHeading), flHover));
			rList.AddRectFilled(rcTrack.min, rcTrack.max, WithGlobalAlpha(rStyle.Get(EColor::Background)), rStyle.flSliderHeight * .5f);
			rList.AddRect(rcTrack.min, rcTrack.max, WithGlobalAlpha(rStyle.Get(EColor::Border).Lerp(rStyle.Get(EColor::BorderHover), flHover)), rStyle.flSliderHeight * .5f);
			rList.AddRectFilled(rcTrack.min, { flKnobX, rcTrack.max.y }, WithGlobalAlpha(rStyle.Get(EColor::Accent)), rStyle.flSliderHeight * .5f);
			const rect rcKnob{ flKnobX - 3.f, rcControl.min.y + 4.f, flKnobX + 3.f, rcControl.max.y - 4.f };
			if (flHover > .01f)
				rList.AddGlowRect(rcKnob.min, rcKnob.max, WithGlobalAlpha(rStyle.Get(EColor::AccentGlow).MultiplyAlpha(flHover)), 5.f, 2.f);
			rList.AddRectFilled(rcKnob.min, rcKnob.max, WithGlobalAlpha(rStyle.Get(EColor::Accent).Lerp(rStyle.Get(EColor::AccentActive), flHover)), 2.f);

			const std::string strValue = FormatValue(strFormat, rValue, bInteger);
			const vec2 vecValue = MeasureText(strValue);
			RenderText({ rcControl.max.x - vecValue.x, LabelY(rcControl) }, strValue, rStyle.Get(EColor::Accent));
			RenderNavHighlight(rcTotal, uID);
			pWindow->stLast.bEdited = bChanged;
			if (bChanged)
				MarkSettingsDirty();
			return bChanged;
		}

		void DrawCheckGlyph(const vec2& vecPosition, std::string_view strOpen, std::string_view strMark, std::string_view strClose, float flMark, color colBracket, color colMark)
		{
			clDrawList& rList = CurrentWindow()->drawList;
			const float flOpen = MeasureText(strOpen).x;
			const float flMarkWidth = MeasureText(strMark).x;
			rList.AddText(vecPosition, WithGlobalAlpha(colBracket), strOpen);
			if (flMark > .01f)
				rList.AddText(vecPosition + vec2{ flOpen, 0.f }, WithGlobalAlpha(colMark.MultiplyAlpha(flMark)), strMark);
			rList.AddText(vecPosition + vec2{ flOpen + flMarkWidth, 0.f }, WithGlobalAlpha(colBracket), strClose);
		}
	}

	void TextUnformatted(std::string_view strText) { TextEx(strText, Style().Get(EColor::Text)); }
	void TextColored(color col, std::string_view strText) { TextEx(strText, col); }
	namespace
	{
		[[nodiscard]] float HintWrapWidth()
		{
			const stWindow* pWindow = CurrentWindow();
			if (HasFlag(pWindow->eFlags, EWindowFlags::AlwaysAutoResize) || pWindow->iAutoFitFrames > 0)
				return 0.f;
			const float flAvailable = GetContentRegionAvail().x;
			return flAvailable > 40.f ? flAvailable : 0.f;
		}
	}

	void TextMuted(std::string_view strText)
	{
		if (CurrentWindow()->bSkipItems)
			return;
		TextEx(strText, Style().Get(EColor::TextMuted), HintWrapWidth());
	}

	void TextDisabled(std::string_view strText)
	{
		if (CurrentWindow()->bSkipItems)
			return;
		TextEx(strText, Style().Get(EColor::TextDisabled), HintWrapWidth());
	}

	void TextWrapped(std::string_view strText)
	{
		if (CurrentWindow()->bSkipItems)
			return;
		TextEx(strText, Style().Get(EColor::Text), GetContentRegionAvail().x);
	}

	void LabelText(std::string_view strLabel, std::string_view strValue)
	{
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return;
		rect rcTotal;
		float flLabelWidth = 0.f;
		const rect rcControl = LabeledControlRect(strLabel, GetTextLineHeight(), rcTotal, flLabelWidth);
		ItemSize(rcTotal.Size());
		if (!ItemAdd(rcTotal, 0))
			return;
		RenderText(rcTotal.min, strLabel, Style().Get(EColor::TextMuted));
		RenderText(rcControl.min, strValue, Style().Get(EColor::Text));
	}

	void BulletText(std::string_view strText) { PrefixedText("- ", Style().Get(EColor::TextDisabled), strText, Style().Get(EColor::Text)); }
	void Title(std::string_view strText) { PrefixedText("> ", Style().Get(EColor::Accent), strText, Style().Get(EColor::TextHeading)); }

	void Header(std::string_view strText)
	{
		if (CurrentWindow()->bSkipItems)
			return;
		stRegion& rRegion = CurrentWindow()->Region();
		if (rRegion.vecCursor.y > rRegion.vecCursorStart.y + 1.f)
			Spacing();
		PrefixedText("## ", Style().Get(EColor::TextDisabled), strText, Style().Get(EColor::TextHeading));
	}

	void SubHeader(std::string_view strText) { PrefixedText("### ", Style().Get(EColor::TextDisabled), strText, Style().Get(EColor::TextHeading)); }

	void Tag(std::string_view strText, color col)
	{
		const stStyle& rStyle = Style();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return;
		if (col.A() == 0)
			col = rStyle.Get(EColor::Accent);
		stRegion& rRegion = pWindow->Region();
		const vec2 vecPosition{ rRegion.vecCursor.x, rRegion.vecCursor.y + rRegion.flTextOffset };
		const float flOpen = MeasureText("[").x;
		const vec2 vecText = MeasureText(strText);
		const vec2 vecSize{ flOpen * 2.f + vecText.x, vecText.y };
		ItemSize(vecSize);
		if (!ItemAdd({ vecPosition, vecPosition + vecSize }, 0))
			return;
		DrawCheckGlyph(vecPosition, "[", strText, "]", 1.f, rStyle.Get(EColor::TextDisabled), col);
	}

	void Breadcrumbs(std::span<const std::string_view> aParts)
	{
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return;
		const stStyle& rStyle = Style();
		stRegion& rRegion = pWindow->Region();
		vec2 vecPosition{ rRegion.vecCursor.x, rRegion.vecCursor.y + rRegion.flTextOffset };
		const vec2 vecStart = vecPosition;
		float flHeight = GetTextLineHeight();
		pWindow->drawList.AddText(vecPosition, WithGlobalAlpha(rStyle.Get(EColor::TextDisabled)), "~/");
		vecPosition.x += MeasureText("~/").x;
		for (std::size_t i = 0; i < aParts.size(); ++i)
		{
			const bool bLast = i + 1 == aParts.size();
			pWindow->drawList.AddText(vecPosition, WithGlobalAlpha(rStyle.Get(bLast ? EColor::Text : EColor::TextMuted)), aParts[i]);
			vecPosition.x += MeasureText(aParts[i]).x;
			if (!bLast)
			{
				pWindow->drawList.AddText(vecPosition, WithGlobalAlpha(rStyle.Get(EColor::TextDisabled)), "/");
				vecPosition.x += MeasureText("/").x;
			}
		}
		ItemSize({ vecPosition.x - vecStart.x, flHeight });
		ItemAdd({ vecStart, { vecPosition.x, vecStart.y + flHeight } }, 0);
	}

	void Separator()
	{
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return;
		stRegion& rRegion = pWindow->Region();
		if (rRegion.flCurrLineHeight > 0.f)
			NewLine();
		const float flWidth = GetContentRegionAvail().x;
		const vec2 vecStart{ rRegion.vecCursor.x, rRegion.vecCursor.y + 2.f };
		ItemSize({ flWidth, 5.f });
		if (ItemAdd({ vecStart, vecStart + vec2{ flWidth, 1.f } }, 0))
			pWindow->drawList.AddDashedLine(vecStart, vecStart + vec2{ flWidth, 0.f }, WithGlobalAlpha(Style().Get(EColor::Border)), 4.f, 3.f, 1.f);
	}

	void SeparatorText(std::string_view strLabel)
	{
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return;
		const stStyle& rStyle = Style();
		stRegion& rRegion = pWindow->Region();
		if (rRegion.flCurrLineHeight > 0.f)
			NewLine();
		const float flWidth = GetContentRegionAvail().x;
		const vec2 vecText = MeasureText(VisibleLabel(strLabel));
		const vec2 vecStart = rRegion.vecCursor;
		ItemSize({ flWidth, vecText.y });
		if (!ItemAdd({ vecStart, vecStart + vec2{ flWidth, vecText.y } }, 0))
			return;
		const float flLineY = std::floor(vecStart.y + vecText.y * .5f);
		pWindow->drawList.AddDashedLine({ vecStart.x, flLineY }, { vecStart.x + 10.f, flLineY }, WithGlobalAlpha(rStyle.Get(EColor::Border)), 4.f, 3.f);
		RenderText({ vecStart.x + 16.f, vecStart.y }, strLabel, rStyle.Get(EColor::TextMuted));
		const float flAfter = vecStart.x + 22.f + vecText.x;
		if (flAfter < vecStart.x + flWidth)
			pWindow->drawList.AddDashedLine({ flAfter, flLineY }, { vecStart.x + flWidth, flLineY }, WithGlobalAlpha(rStyle.Get(EColor::Border)), 4.f, 3.f);
	}

	void Alert(EToastKind eKind, std::string_view strText)
	{
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return;
		const stStyle& rStyle = Style();
		std::string_view strPrefix = "info: ";
		color colKind = rStyle.Get(EColor::TextMuted);
		switch (eKind)
		{
		case EToastKind::Success: strPrefix = "ok: "; colKind = rStyle.Get(EColor::Success); break;
		case EToastKind::Warning: strPrefix = "warn: "; colKind = rStyle.Get(EColor::Warning); break;
		case EToastKind::Error: strPrefix = "error: "; colKind = rStyle.Get(EColor::Danger); break;
		default: break;
		}
		stRegion& rRegion = pWindow->Region();
		const float flWidth = GetContentRegionAvail().x;
		const vec2 vecPadding{ 14.f, 9.f };
		const float flPrefix = MeasureText(strPrefix).x;
		const vec2 vecText = MeasureText(strText, flWidth - vecPadding.x * 2.f - flPrefix);
		const rect rcBox{ rRegion.vecCursor, rRegion.vecCursor + vec2{ flWidth, vecText.y + vecPadding.y * 2.f } };
		ItemSize(rcBox.Size());
		if (!ItemAdd(rcBox, 0))
			return;
		clDrawList& rList = pWindow->drawList;
		rList.AddRectFilled(rcBox.min, rcBox.max, WithGlobalAlpha(rStyle.Get(EColor::Surface)), rStyle.flRoundingSmall);
		rList.AddRectFilled(rcBox.min, rcBox.max, WithGlobalAlpha(colKind.WithAlpha(eKind == EToastKind::Info ? 0 : 26)), rStyle.flRoundingSmall);
		rList.AddRect(rcBox.min, rcBox.max, WithGlobalAlpha(rStyle.Get(EColor::Border)), rStyle.flRoundingSmall);
		rList.AddRectFilled(rcBox.min, { rcBox.min.x + 3.f, rcBox.max.y }, WithGlobalAlpha(colKind), rStyle.flRoundingSmall, ECorner::Left);
		rList.AddText(rcBox.min + vecPadding, WithGlobalAlpha(colKind), strPrefix);
		rList.AddText(rcBox.min + vecPadding + vec2{ flPrefix, 0.f }, WithGlobalAlpha(eKind == EToastKind::Error ? colKind : rStyle.Get(EColor::Text)), strText, 0.f, flWidth - vecPadding.x * 2.f - flPrefix);
	}

	void KeyHint(KeyCode uKey, std::string_view strAction)
	{
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return;
		const stStyle& rStyle = Style();
		const std::string strKey = KeyName(uKey, Ctx().clInputState.GetGamepad().eType);
		const vec2 vecKey = MeasureText(strKey);
		const vec2 vecAction = MeasureText(strAction);
		stRegion& rRegion = pWindow->Region();
		const float flHeight = FrameHeight() - 4.f;
		const vec2 vecPosition{ rRegion.vecCursor.x, rRegion.vecCursor.y + 2.f };
		const rect rcCap{ vecPosition, vecPosition + vec2{ vecKey.x + 12.f, flHeight } };
		const vec2 vecSize{ rcCap.Width() + 6.f + vecAction.x, FrameHeight() };
		ItemSize(vecSize, rStyle.vecFramePadding.y);
		if (!ItemAdd({ rRegion.vecCursorPrevLine - vec2{ vecSize.x, 0.f }, rRegion.vecCursorPrevLine + vec2{ 0.f, vecSize.y } }, 0))
			return;
		clDrawList& rList = pWindow->drawList;
		rList.AddRectFilled(rcCap.min, rcCap.max, WithGlobalAlpha(rStyle.Get(EColor::Background)), rStyle.flRoundingSmall);
		rList.AddRect(rcCap.min, rcCap.max, WithGlobalAlpha(rStyle.Get(EColor::Border)), rStyle.flRoundingSmall);
		rList.AddRectFilled({ rcCap.min.x + 1.f, rcCap.max.y - 2.f }, { rcCap.max.x - 1.f, rcCap.max.y - 1.f }, WithGlobalAlpha(rStyle.Get(EColor::Border)));
		rList.AddText({ rcCap.min.x + 6.f, std::floor(rcCap.Center().y - vecKey.y * .5f) }, WithGlobalAlpha(rStyle.Get(EColor::Accent)), strKey);
		rList.AddText({ rcCap.max.x + 6.f, std::floor(rcCap.Center().y - vecAction.y * .5f) }, WithGlobalAlpha(rStyle.Get(EColor::TextMuted)), strAction);
	}

	void ControllerLegend()
	{
		stContext& g = Ctx();
		if (!g.clInputState.GetGamepad().bConnected)
			return;
		const vec2 vecDisplay = GetDisplaySize();
		SetNextWindowPos({ vecDisplay.x * .5f, vecDisplay.y - 16.f }, ECondition::Always, { .5f, 1.f });
		constexpr EWindowFlags eFlags = EWindowFlags::NoTitleBar | EWindowFlags::NoResize | EWindowFlags::NoMove | EWindowFlags::NoScrollbar | EWindowFlags::NoScrollWithMouse |
			EWindowFlags::NoCollapse | EWindowFlags::NoSavedSettings | EWindowFlags::NoNav | EWindowFlags::NoInputs | EWindowFlags::NoBringToFront | EWindowFlags::AlwaysAutoResize | EWindowFlags::NoSnap;
		if (Begin("##controller_legend", nullptr, eFlags))
		{
			const float flSpacing = Style().vecItemSpacing.x * 2.f;
			KeyHint(GamepadKey(EGamepadButton::DPadUp), "/");
			SameLine(0.f, 2.f);
			KeyHint(GamepadKey(EGamepadButton::DPadDown), "navigate");
			SameLine(0.f, flSpacing);
			KeyHint(GamepadKey(EGamepadButton::DPadLeft), "/");
			SameLine(0.f, 2.f);
			KeyHint(GamepadKey(EGamepadButton::DPadRight), "adjust");
			SameLine(0.f, flSpacing);
			KeyHint(GamepadKey(EGamepadButton::FaceDown), "select");
			SameLine(0.f, flSpacing);
			KeyHint(GamepadKey(EGamepadButton::FaceRight), "back");
			SameLine(0.f, flSpacing);
			KeyHint(GamepadKey(EGamepadButton::LeftShoulder), "/");
			SameLine(0.f, 2.f);
			KeyHint(GamepadKey(EGamepadButton::RightShoulder), "tabs");
			SameLine(0.f, flSpacing);
			KeyHint(GamepadKey(EGamepadButton::LeftTrigger), "/");
			SameLine(0.f, 2.f);
			KeyHint(GamepadKey(EGamepadButton::RightTrigger), "windows");
			SameLine(0.f, flSpacing);
			KeyHint(GamepadKey(EGamepadButton::Back), "+ sticks move/size");
			if (g.stSettings.bToggleWithGamepad)
			{
				SameLine(0.f, flSpacing);
				KeyHint(GamepadKey(EGamepadButton::Back), "+");
				SameLine(0.f, 2.f);
				KeyHint(GamepadKey(EGamepadButton::Start), "close menu");
			}
		}
		End();
	}

	void ControllerHints()
	{
		stContext& g = Ctx();
		if (CurrentWindow()->bSkipItems)
			return;
		const float flSpacing = Style().vecItemSpacing.x * 2.f;
		if (g.clInputState.GetGamepad().bConnected)
		{
			KeyHint(GamepadKey(EGamepadButton::FaceDown), "select");
			SameLine(0.f, flSpacing);
			KeyHint(GamepadKey(EGamepadButton::FaceRight), "back");
			SameLine(0.f, flSpacing);
			KeyHint(GamepadKey(EGamepadButton::LeftShoulder), "/");
			SameLine(0.f, 2.f);
			KeyHint(GamepadKey(EGamepadButton::RightShoulder), "tabs");
			SameLine(0.f, flSpacing);
			KeyHint(GamepadKey(EGamepadButton::RightTrigger), "windows");
			SameLine(0.f, flSpacing);
			KeyHint(GamepadKey(EGamepadButton::Back), "+ sticks move/size");
		}
		else
		{
			KeyHint(0x26, "/");
			SameLine(0.f, 2.f);
			KeyHint(0x28, "navigate");
			SameLine(0.f, flSpacing);
			KeyHint(0x0D, "select");
			SameLine(0.f, flSpacing);
			KeyHint(0x1B, "back");
			SameLine(0.f, flSpacing);
			KeyHint(g.stSettings.uToggleKey, "toggle menu");
		}
	}

	bool Button(std::string_view strLabel, const vec2& vecSize, EButtonStyle eStyle)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return false;
		const stStyle& rStyle = g.stCurrentStyle;
		const ID uID = pWindow->GetID(strLabel);
		const vec2 vecText = MeasureText(VisibleLabel(strLabel));
		stRegion& rRegion = pWindow->Region();
		vec2 vecFrame{ vecSize.x > 0.f ? vecSize.x : vecText.x + rStyle.vecFramePadding.x * 2.f + 8.f, vecSize.y > 0.f ? vecSize.y : FrameHeight() };
		if (vecSize.x < 0.f)
			vecFrame.x = std::max(GetContentRegionAvail().x + vecSize.x, 8.f);
		const float flLineHeight = std::max(vecFrame.y, FrameHeight());
		const vec2 vecOrigin{ rRegion.vecCursor.x, rRegion.vecCursor.y + std::floor((flLineHeight - vecFrame.y) * .5f) };
		const rect rcFrame{ vecOrigin, vecOrigin + vecFrame.Floor() };
		ItemSize({ vecFrame.x, flLineHeight }, rStyle.vecFramePadding.y);
		if (!ItemAdd(rcFrame, uID))
			return false;

		bool bHovered = false, bHeld = false;
		const bool bPressed = ButtonBehavior(rcFrame, uID, &bHovered, &bHeld);
		const float flHover = AnimateBool(uID, bHovered || (g.uNavID == uID && g.bNavVisible));
		const float flFlash = Animate(HashString("#navflash", uID), 0.f, 10.f);
		clDrawList& rList = pWindow->drawList;
		const float flRounding = rStyle.flRoundingSmall;

		color colFill, colBorder, colText;
		switch (eStyle)
		{
		case EButtonStyle::Primary:
			colFill = rStyle.Get(EColor::Accent).Lerp(rStyle.Get(EColor::AccentActive), flHover);
			colBorder = colFill;
			colText = rStyle.Get(EColor::AccentText);
			break;
		case EButtonStyle::Danger:
			colFill = rStyle.Get(EColor::Danger).Lerp(color::Hex(0xFF9999), flHover);
			colBorder = colFill;
			colText = rStyle.Get(EColor::AccentText);
			break;
		case EButtonStyle::Secondary:
			colFill = rStyle.Get(EColor::Background).Lerp(rStyle.Get(EColor::Hover), flHover);
			colBorder = rStyle.Get(EColor::Border).Lerp(rStyle.Get(EColor::Accent), flHover);
			colText = rStyle.Get(EColor::Text).Lerp(rStyle.Get(EColor::Accent), flHover);
			break;
		case EButtonStyle::Ghost:
			colFill = rStyle.Get(EColor::Hover).MultiplyAlpha(flHover);
			colBorder = Colors::Transparent;
			colText = rStyle.Get(EColor::TextMuted).Lerp(rStyle.Get(EColor::Accent), flHover);
			break;
		}
		if (bHeld)
			colFill = colFill.Lerp(Colors::Black, .12f);
		if ((eStyle == EButtonStyle::Primary || eStyle == EButtonStyle::Danger) && flHover > .01f)
			rList.AddGlowRect(rcFrame.min, rcFrame.max, WithGlobalAlpha((eStyle == EButtonStyle::Danger ? rStyle.Get(EColor::DangerSoft) : rStyle.Get(EColor::AccentGlow)).MultiplyAlpha(flHover)), 4.f, flRounding);
		if (flFlash > .01f)
			colFill = colFill.Lerp(Colors::White, flFlash * .35f);
		rList.AddRectFilled(rcFrame.min, rcFrame.max, WithGlobalAlpha(colFill), flRounding);
		if (colBorder.A())
			rList.AddRect(rcFrame.min, rcFrame.max, WithGlobalAlpha(colBorder), flRounding);
		RenderTextClipped(rcFrame, strLabel, colText, { .5f, .5f });
		RenderNavHighlight(rcFrame, uID, flRounding);
		return bPressed;
	}

	bool SmallButton(std::string_view strLabel, EButtonStyle eStyle)
	{
		const stStyle& rStyle = Style();
		return Button(strLabel, { MeasureText(VisibleLabel(strLabel)).x + rStyle.vecFramePadding.x * 1.4f, std::floor(FrameHeight() * .8f) }, eStyle);
	}

	bool InvisibleButton(std::string_view strID, const vec2& vecSize)
	{
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return false;
		const ID uID = pWindow->GetID(strID);
		stRegion& rRegion = pWindow->Region();
		const rect rcFrame{ rRegion.vecCursor, rRegion.vecCursor + vecSize };
		ItemSize(vecSize);
		if (!ItemAdd(rcFrame, uID))
			return false;
		return ButtonBehavior(rcFrame, uID, nullptr, nullptr);
	}

	bool TextLink(std::string_view strLabel)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return false;
		const stStyle& rStyle = g.stCurrentStyle;
		const ID uID = pWindow->GetID(strLabel);
		stRegion& rRegion = pWindow->Region();
		const std::string_view strPrefix = rStyle.bConsolePrefixes ? "./" : "";
		const float flPrefix = MeasureText(strPrefix).x;
		const vec2 vecText = MeasureText(VisibleLabel(strLabel));
		const vec2 vecPosition{ rRegion.vecCursor.x, rRegion.vecCursor.y + rRegion.flTextOffset };
		const rect rcLink{ vecPosition, vecPosition + vec2{ flPrefix + vecText.x, vecText.y } };
		ItemSize(rcLink.Size());
		if (!ItemAdd(rcLink, uID))
			return false;
		bool bHovered = false;
		const bool bPressed = ButtonBehavior(rcLink, uID, &bHovered, nullptr);
		if (bHovered)
			g.eCursor = EMouseCursor::Hand;
		const float flHover = AnimateBool(uID, bHovered);
		pWindow->drawList.AddText(vecPosition, WithGlobalAlpha(rStyle.Get(EColor::TextDisabled)), strPrefix);
		pWindow->drawList.AddText(vecPosition + vec2{ flPrefix, 0.f }, WithGlobalAlpha(rStyle.Get(EColor::Accent).Lerp(rStyle.Get(EColor::AccentActive), flHover)), VisibleLabel(strLabel));
		if (flHover > .01f)
			pWindow->drawList.AddRectFilled({ vecPosition.x + flPrefix, rcLink.max.y - 1.f }, { rcLink.max.x, rcLink.max.y }, WithGlobalAlpha(rStyle.Get(EColor::AccentActive).MultiplyAlpha(flHover)));
		RenderNavHighlight(rcLink, uID);
		return bPressed;
	}

	bool Checkbox(std::string_view strLabel, bool* pValue)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return false;
		const stStyle& rStyle = g.stCurrentStyle;
		const ID uID = pWindow->GetID(strLabel);
		stRegion& rRegion = pWindow->Region();
		const float flGlyph = MeasureText("[x]").x;
		const vec2 vecLabel = MeasureText(VisibleLabel(strLabel));
		const float flHeight = FrameHeight();
		const rect rcTotal{ rRegion.vecCursor, rRegion.vecCursor + vec2{ flGlyph + (vecLabel.x > 0.f ? rStyle.vecItemInnerSpacing.x + vecLabel.x : 0.f), flHeight } };
		ItemSize(rcTotal.Size(), rStyle.vecFramePadding.y);
		if (!ItemAdd(rcTotal, uID))
			return false;

		bool bHovered = false;
		const bool bPressed = ButtonBehavior(rcTotal, uID, &bHovered, nullptr);
		if (bPressed)
		{
			*pValue = !*pValue;
			pWindow->stLast.bEdited = true;
			MarkSettingsDirty();
		}
		const float flHover = AnimateBool(uID, bHovered || (g.uNavID == uID && g.bNavVisible));
		const float flMark = AnimateBool(HashString("#mark", uID), *pValue, 22.f);
		const float flY = LabelY(rcTotal);
		DrawCheckGlyph({ rcTotal.min.x, flY }, "[", "x", "]", flMark, rStyle.Get(EColor::TextDisabled).Lerp(rStyle.Get(EColor::Accent), flHover * .6f), rStyle.Get(EColor::Accent));
		if (vecLabel.x > 0.f)
			RenderText({ rcTotal.min.x + flGlyph + rStyle.vecItemInnerSpacing.x, flY }, strLabel, rStyle.Get(EColor::Text).Lerp(rStyle.Get(EColor::TextHeading), flHover).Lerp(rStyle.Get(EColor::Accent), flHover * .35f));
		RenderNavHighlight(rcTotal, uID);
		return bPressed;
	}

	bool Toggle(std::string_view strLabel, bool* pValue)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return false;
		const stStyle& rStyle = g.stCurrentStyle;
		const ID uID = pWindow->GetID(strLabel);
		stRegion& rRegion = pWindow->Region();
		const float flHeight = FrameHeight();
		const float flTrackHeight = std::floor(flHeight * .62f);
		const float flTrackWidth = std::floor(flTrackHeight * 1.9f);
		const std::string_view strVisible = VisibleLabel(strLabel);
		const float flAvailable = GetContentRegionAvail().x;
		const float flLabelWidth = MeasureText(strVisible).x;
		const float flTotal = strVisible.empty() ? flTrackWidth : std::max(flAvailable, flLabelWidth + rStyle.vecItemInnerSpacing.x + flTrackWidth);
		const rect rcTotal{ rRegion.vecCursor, rRegion.vecCursor + vec2{ flTotal, flHeight } };
		ItemSize(rcTotal.Size(), rStyle.vecFramePadding.y);
		if (!ItemAdd(rcTotal, uID))
			return false;

		bool bHovered = false;
		const bool bPressed = ButtonBehavior(rcTotal, uID, &bHovered, nullptr);
		if (bPressed)
		{
			*pValue = !*pValue;
			pWindow->stLast.bEdited = true;
			MarkSettingsDirty();
		}
		const float flHover = AnimateBool(uID, bHovered || (g.uNavID == uID && g.bNavVisible));
		const float flOn = AnimateBool(HashString("#on", uID), *pValue, 18.f);
		const float flCenterY = std::floor(rcTotal.Center().y);
		const rect rcTrack{ rcTotal.max.x - flTrackWidth, flCenterY - flTrackHeight * .5f, rcTotal.max.x, flCenterY + flTrackHeight * .5f };
		clDrawList& rList = pWindow->drawList;
		if (flLabelWidth > 0.f)
			RenderText({ rcTotal.min.x, LabelY(rcTotal) }, strLabel, rStyle.Get(EColor::Text).Lerp(rStyle.Get(EColor::TextHeading), flHover));
		rList.AddRectFilled(rcTrack.min, rcTrack.max, WithGlobalAlpha(rStyle.Get(EColor::Background).Lerp(rStyle.Get(EColor::Accent), flOn)), flTrackHeight * .5f);
		rList.AddRect(rcTrack.min, rcTrack.max, WithGlobalAlpha(rStyle.Get(EColor::Border).Lerp(rStyle.Get(EColor::Accent), std::max(flOn, flHover * .5f))), flTrackHeight * .5f);
		const float flKnobRadius = flTrackHeight * .5f - 3.f;
		const float flKnobX = Lerp(rcTrack.min.x + flTrackHeight * .5f, rcTrack.max.x - flTrackHeight * .5f, flOn);
		rList.AddCircleFilled({ flKnobX, flCenterY }, flKnobRadius, WithGlobalAlpha(rStyle.Get(EColor::TextDisabled).Lerp(rStyle.Get(EColor::Background), flOn)));
		RenderNavHighlight(rcTotal, uID);
		return bPressed;
	}

	bool RadioButton(std::string_view strLabel, bool bActive)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return false;
		const stStyle& rStyle = g.stCurrentStyle;
		const ID uID = pWindow->GetID(strLabel);
		stRegion& rRegion = pWindow->Region();
		const float flGlyph = MeasureText("(*)").x;
		const vec2 vecLabel = MeasureText(VisibleLabel(strLabel));
		const rect rcTotal{ rRegion.vecCursor, rRegion.vecCursor + vec2{ flGlyph + rStyle.vecItemInnerSpacing.x + vecLabel.x, FrameHeight() } };
		ItemSize(rcTotal.Size(), rStyle.vecFramePadding.y);
		if (!ItemAdd(rcTotal, uID))
			return false;
		bool bHovered = false;
		const bool bPressed = ButtonBehavior(rcTotal, uID, &bHovered, nullptr);
		const float flHover = AnimateBool(uID, bHovered || (g.uNavID == uID && g.bNavVisible));
		const float flMark = AnimateBool(HashString("#mark", uID), bActive, 22.f);
		const float flY = LabelY(rcTotal);
		DrawCheckGlyph({ rcTotal.min.x, flY }, "(", "*", ")", flMark, rStyle.Get(EColor::TextDisabled).Lerp(rStyle.Get(EColor::Accent), flHover * .6f), rStyle.Get(EColor::Accent));
		RenderText({ rcTotal.min.x + flGlyph + rStyle.vecItemInnerSpacing.x, flY }, strLabel, rStyle.Get(bActive ? EColor::TextHeading : EColor::Text).Lerp(rStyle.Get(EColor::Accent), flHover * .35f));
		RenderNavHighlight(rcTotal, uID);
		return bPressed;
	}

	bool RadioButton(std::string_view strLabel, int* pValue, int iButtonValue)
	{
		const bool bPressed = RadioButton(strLabel, *pValue == iButtonValue);
		if (bPressed)
		{
			*pValue = iButtonValue;
			CurrentWindow()->stLast.bEdited = true;
			MarkSettingsDirty();
		}
		return bPressed;
	}

	bool SliderFloat(std::string_view strLabel, float* pValue, float flMin, float flMax, std::string_view strFormat, float flNavStep)
	{
		double dValue = *pValue;
		const bool bChanged = SliderBehavior(strLabel, dValue, flMin, flMax, false, strFormat, flNavStep);
		if (bChanged)
			*pValue = static_cast<float>(dValue);
		return bChanged;
	}

	bool SliderInt(std::string_view strLabel, int* pValue, int iMin, int iMax, std::string_view strFormat)
	{
		double dValue = *pValue;
		const bool bChanged = SliderBehavior(strLabel, dValue, iMin, iMax, true, strFormat, 0.0);
		if (bChanged)
			*pValue = static_cast<int>(dValue);
		return bChanged;
	}

	namespace
	{
		bool StepperBehavior(std::string_view strLabel, double& rValue, double dStep, double dMin, double dMax, bool bInteger, std::string_view strFormat)
		{
			stContext& g = Ctx();
			stWindow* pWindow = CurrentWindow();
			if (pWindow->bSkipItems)
				return false;
			const stStyle& rStyle = g.stCurrentStyle;
			const ID uID = pWindow->GetID(strLabel);
			const float flHeight = FrameHeight();
			rect rcTotal;
			float flLabelWidth = 0.f;
			const rect rcControl = LabeledControlRect(strLabel, flHeight, rcTotal, flLabelWidth);
			ItemSize(rcTotal.Size(), rStyle.vecFramePadding.y);
			if (!ItemAdd(rcTotal, uID, EItemFlags::NavConsumesHorizontal))
				return false;

			bool bChanged = false;
			const rect rcMinus{ rcControl.min, { rcControl.min.x + flHeight, rcControl.max.y } };
			const rect rcPlus{ { rcControl.max.x - flHeight, rcControl.min.y }, rcControl.max };
			const rect rcField{ { rcMinus.max.x + 2.f, rcControl.min.y }, { rcPlus.min.x - 2.f, rcControl.max.y } };

			auto StepButton = [&](const rect& rcButton, std::string_view strGlyph, ID uButtonID, double dDirection) {
				bool bHovered = false, bHeld = false;
				const bool bPressed = ButtonBehavior(rcButton, uButtonID, &bHovered, &bHeld, EItemFlags::NoNav | EItemFlags::PressOnClick | EItemFlags::Repeat);
				const float flHover = AnimateBool(uButtonID, bHovered || bHeld);
				RenderFrame(rcButton, rStyle.Get(EColor::Background).Lerp(rStyle.Get(EColor::Hover), flHover), true, rStyle.Get(EColor::Border).Lerp(rStyle.Get(EColor::Accent), flHover), rStyle.flRoundingSmall);
				RenderTextClipped(rcButton, strGlyph, rStyle.Get(EColor::TextMuted).Lerp(rStyle.Get(EColor::Accent), flHover), { .5f, .5f });
				if (bPressed)
				{
					rValue = std::clamp(rValue + dStep * dDirection, dMin, dMax);
					bChanged = true;
				}
			};
			StepButton(rcMinus, "-", HashString("#minus", uID), -1.0);
			StepButton(rcPlus, "+", HashString("#plus", uID), 1.0);

			if (g.uNavID == uID && g.pNavWindow == pWindow && g.iNavAdjust != 0)
			{
				rValue = std::clamp(rValue + dStep * g.iNavAdjust * (g.clInputState.IsGamepadDown(EGamepadButton::RightShoulder) || g.clInputState.IsShiftDown() ? 10.0 : 1.0), dMin, dMax);
				bChanged = true;
			}

			const ID uFieldID = HashString("#field", uID);
			std::string strText = FormatValue(strFormat, rValue, bInteger);
			pWindow->stLast.uID = uFieldID;
			if (InputTextFrame(uFieldID, rcField, strText, {}, EInputTextFlags::CharsDecimal | EInputTextFlags::NoOnScreenKeyboard, g.uNavActivateID == uID && g.pNavWindow == pWindow))
			{
				double dParsed = 0.0;
				const auto stResult = std::from_chars(strText.data(), strText.data() + strText.size(), dParsed);
				if (stResult.ec == std::errc())
				{
					rValue = std::clamp(bInteger ? std::round(dParsed) : dParsed, dMin, dMax);
					bChanged = true;
				}
			}
			pWindow->stLast.uID = uID;
			pWindow->stLast.rcItem = rcTotal;

			if (flLabelWidth > 0.f)
				RenderText({ rcTotal.min.x, LabelY(rcTotal) }, strLabel, rStyle.Get(EColor::Text));
			RenderNavHighlight(rcTotal, uID);
			pWindow->stLast.bEdited = bChanged;
			if (bChanged)
				MarkSettingsDirty();
			return bChanged;
		}
	}

	bool InputInt(std::string_view strLabel, int* pValue, int iStep, int iMin, int iMax)
	{
		double dValue = *pValue;
		const bool bChanged = StepperBehavior(strLabel, dValue, iStep, iMin, iMax, true, "{}");
		if (bChanged)
			*pValue = static_cast<int>(dValue);
		return bChanged;
	}

	bool InputFloat(std::string_view strLabel, float* pValue, float flStep, std::string_view strFormat)
	{
		double dValue = *pValue;
		const bool bChanged = StepperBehavior(strLabel, dValue, flStep, -1e30, 1e30, false, strFormat);
		if (bChanged)
			*pValue = static_cast<float>(dValue);
		return bChanged;
	}

	bool BeginCombo(std::string_view strLabel, std::string_view strPreview)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return false;
		const stStyle& rStyle = g.stCurrentStyle;
		const ID uID = pWindow->GetID(strLabel);
		const ID uPopupID = HashString("#combo", uID);
		const float flHeight = FrameHeight();
		rect rcTotal;
		float flLabelWidth = 0.f;
		const rect rcControl = LabeledControlRect(strLabel, flHeight, rcTotal, flLabelWidth);
		ItemSize(rcTotal.Size(), rStyle.vecFramePadding.y);
		const bool bVisible = ItemAdd(rcTotal, uID);

		bool bHovered = false;
		const bool bPressed = bVisible && ButtonBehavior(rcControl, uID, &bHovered, nullptr, EItemFlags::PressOnClick);
		const bool bOpen = IsPopupOpenID(uPopupID);
		if (bPressed && !bOpen)
			OpenPopupEx(uPopupID, rcControl, false, true);
		else if (bPressed && bOpen)
			ClosePopupToLevel(g.aBeginPopupStack.size());

		if (bVisible)
		{
			const float flHover = AnimateBool(uID, bHovered || bOpen || (g.uNavID == uID && g.bNavVisible));
			if (flLabelWidth > 0.f)
				RenderText({ rcTotal.min.x, LabelY(rcTotal) }, strLabel, rStyle.Get(EColor::Text).Lerp(rStyle.Get(EColor::TextHeading), flHover));
			RenderFrame(rcControl, rStyle.Get(EColor::Background), true, rStyle.Get(EColor::Border).Lerp(rStyle.Get(EColor::Accent), flHover), rStyle.flRoundingSmall);
			if (bOpen)
				pWindow->drawList.AddGlowRect(rcControl.min, rcControl.max, WithGlobalAlpha(rStyle.Get(EColor::AccentGlow)), 3.f, rStyle.flRoundingSmall);
			const rect rcText{ rcControl.min + vec2{ rStyle.vecFramePadding.x, 0.f }, { rcControl.max.x - flHeight, rcControl.max.y } };
			RenderTextClipped(rcText, strPreview, rStyle.Get(EColor::Text), { 0.f, .5f }, &rcText);
			const vec2 vecArrow{ rcControl.max.x - flHeight * .5f, rcControl.Center().y };
			const float flArrow = 3.5f;
			const float flFlip = Animate(HashString("#arrow", uID), bOpen ? -1.f : 1.f, 20.f);
			pWindow->drawList.AddTriangleFilled(vecArrow + vec2{ -flArrow, -flArrow * .5f * flFlip }, vecArrow + vec2{ flArrow, -flArrow * .5f * flFlip }, vecArrow + vec2{ 0.f, flArrow * .6f * flFlip }, WithGlobalAlpha(rStyle.Get(EColor::TextMuted).Lerp(rStyle.Get(EColor::Accent), flHover)));
			RenderNavHighlight(rcTotal, uID);
		}

		if (!IsPopupOpenID(uPopupID))
			return false;
		SetNextWindowSizeConstraints({ rcControl.Width(), 0.f }, { std::max(rcControl.Width(), 400.f), 320.f * g.stSettings.flScale });
		return BeginPopupEx(uPopupID, EWindowFlags::None, {});
	}

	void EndCombo()
	{
		EndPopup();
	}

	bool Combo(std::string_view strLabel, int* pCurrent, std::span<const std::string_view> aItems)
	{
		const std::string_view strPreview = *pCurrent >= 0 && *pCurrent < static_cast<int>(aItems.size()) ? aItems[*pCurrent] : std::string_view{};
		if (!BeginCombo(strLabel, strPreview))
			return false;
		bool bChanged = false;
		const bool bAppearing = CurrentWindow()->bAppearing;
		for (int i = 0; i < static_cast<int>(aItems.size()); ++i)
		{
			PushID(i);
			if (bAppearing && i == *pCurrent)
				SetKeyboardFocusHere();
			if (Selectable(aItems[i], i == *pCurrent))
			{
				*pCurrent = i;
				bChanged = true;
			}
			PopID();
		}
		EndCombo();
		if (bChanged)
		{
			MarkSettingsDirty();
			CurrentWindow()->stLast.bEdited = true;
		}
		return bChanged;
	}

	bool Selectable(std::string_view strLabel, bool bSelected, const vec2& vecSize)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return false;
		const stStyle& rStyle = g.stCurrentStyle;
		const ID uID = pWindow->GetID(strLabel);
		stRegion& rRegion = pWindow->Region();
		const vec2 vecText = MeasureText(VisibleLabel(strLabel));
		const float flWidth = vecSize.x > 0.f ? vecSize.x : std::max(GetContentRegionAvail().x, vecText.x + rStyle.vecFramePadding.x * 2.f);
		const rect rcRow{ rRegion.vecCursor, rRegion.vecCursor + vec2{ flWidth, vecSize.y > 0.f ? vecSize.y : FrameHeight() } };
		ItemSize(rcRow.Size(), rStyle.vecFramePadding.y);
		if (!ItemAdd(rcRow, uID))
			return false;
		bool bHovered = false;
		const bool bPressed = ButtonBehavior(rcRow, uID, &bHovered, nullptr);
		const bool bFocused = g.uNavID == uID && g.bNavVisible;
		const float flHover = AnimateBool(uID, bHovered || bFocused);
		clDrawList& rList = pWindow->drawList;
		if (bSelected)
		{
			rList.AddRectFilled(rcRow.min, rcRow.max, WithGlobalAlpha(rStyle.Get(EColor::AccentSoft)), rStyle.flRoundingSmall);
			rList.AddRectFilled(rcRow.min, { rcRow.min.x + 2.f, rcRow.max.y }, WithGlobalAlpha(rStyle.Get(EColor::Accent)));
		}
		if (flHover > .01f)
			rList.AddRectFilled(rcRow.min, rcRow.max, WithGlobalAlpha(rStyle.Get(EColor::Hover).MultiplyAlpha(flHover)), rStyle.flRoundingSmall);
		RenderTextClipped({ rcRow.min + vec2{ rStyle.vecFramePadding.x, 0.f }, rcRow.max }, strLabel, bSelected ? rStyle.Get(EColor::Accent) : rStyle.Get(EColor::Text).Lerp(rStyle.Get(EColor::Accent), flHover), { 0.f, .5f });
		if (bFocused && !HasFlag(pWindow->eFlags, EWindowFlags::Popup))
			RenderNavHighlight(rcRow, uID);
		if (bPressed && HasFlag(pWindow->eFlags, EWindowFlags::Popup) && !HasFlag(pWindow->eFlags, EWindowFlags::Modal))
			CloseCurrentPopup();
		return bPressed;
	}

	void ProgressBar(float flFraction, const vec2& vecSize, std::string_view strOverlay)
	{
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return;
		const stStyle& rStyle = Style();
		stRegion& rRegion = pWindow->Region();
		const vec2 vecFrame{ vecSize.x > 0.f ? vecSize.x : GetContentRegionAvail().x + (vecSize.x < 0.f ? vecSize.x : 0.f), vecSize.y > 0.f ? vecSize.y : FrameHeight() };
		const rect rcFrame{ rRegion.vecCursor, rRegion.vecCursor + vecFrame };
		ItemSize(vecFrame, rStyle.vecFramePadding.y);
		if (!ItemAdd(rcFrame, 0))
			return;
		const ID uAnimID = HashString("#progress", pWindow->GetID(std::to_string(static_cast<int>(rcFrame.min.y))));
		const float flShown = Animate(uAnimID, std::clamp(flFraction, 0.f, 1.f), 10.f);
		RenderFrame(rcFrame, rStyle.Get(EColor::Background), true, rStyle.Get(EColor::Border), rStyle.flRoundingSmall);
		const rect rcFill{ rcFrame.min + vec2{ 2.f, 2.f }, { rcFrame.min.x + 2.f + (rcFrame.Width() - 4.f) * flShown, rcFrame.max.y - 2.f } };
		pWindow->drawList.AddRectFilled(rcFill.min, rcFill.max, WithGlobalAlpha(rStyle.Get(EColor::Accent)), rStyle.flRoundingSmall);
		const std::string strText = strOverlay.empty() ? std::format("{:.0f}%", flFraction * 100.f) : std::string(strOverlay);
		const vec2 vecText = MeasureText(strText);
		const vec2 vecPosition{ std::floor(rcFrame.Center().x - vecText.x * .5f), std::floor(rcFrame.Center().y - vecText.y * .5f) };
		pWindow->drawList.PushClipRect({ rcFill.max.x, rcFrame.min.y, rcFrame.max.x, rcFrame.max.y });
		pWindow->drawList.AddText(vecPosition, WithGlobalAlpha(rStyle.Get(EColor::Text)), strText);
		pWindow->drawList.PopClipRect();
		pWindow->drawList.PushClipRect({ rcFrame.min, { rcFill.max.x, rcFrame.max.y } });
		pWindow->drawList.AddText(vecPosition, WithGlobalAlpha(rStyle.Get(EColor::AccentText)), strText);
		pWindow->drawList.PopClipRect();
	}

	namespace
	{
		void PlotEx(std::string_view strLabel, std::span<const float> aValues, float flMin, float flMax, const vec2& vecSize, bool bHistogram)
		{
			stContext& g = Ctx();
			stWindow* pWindow = CurrentWindow();
			if (pWindow->bSkipItems)
				return;
			const stStyle& rStyle = g.stCurrentStyle;
			const ID uID = pWindow->GetID(strLabel);
			stRegion& rRegion = pWindow->Region();
			const std::string_view strVisible = VisibleLabel(strLabel);
			const float flLabelHeight = strVisible.empty() ? 0.f : GetTextLineHeight() + 4.f;
			const vec2 vecFrame{ vecSize.x > 0.f ? vecSize.x : GetContentRegionAvail().x, vecSize.y > 0.f ? vecSize.y : 72.f * g.stSettings.flScale };
			const rect rcTotal{ rRegion.vecCursor, rRegion.vecCursor + vec2{ vecFrame.x, vecFrame.y + flLabelHeight } };
			const rect rcFrame{ { rcTotal.min.x, rcTotal.min.y + flLabelHeight }, rcTotal.max };
			ItemSize(rcTotal.Size());
			if (!ItemAdd(rcTotal, uID, EItemFlags::NoNav))
				return;
			const bool bHovered = ItemHoverable(rcFrame, uID);

			if (flMin == flMax)
			{
				flMin = aValues.empty() ? 0.f : *std::ranges::min_element(aValues);
				flMax = aValues.empty() ? 1.f : *std::ranges::max_element(aValues);
				if (flMin == flMax)
					flMax = flMin + 1.f;
			}

			clDrawList& rList = pWindow->drawList;
			if (!strVisible.empty())
				RenderText(rcTotal.min, strVisible, rStyle.Get(EColor::TextMuted));
			RenderFrame(rcFrame, rStyle.Get(EColor::Background), true, rStyle.Get(EColor::Border), rStyle.flRoundingSmall);
			for (int i = 1; i < 4; ++i)
			{
				const float flY = std::floor(Lerp(rcFrame.min.y, rcFrame.max.y, static_cast<float>(i) / 4.f));
				rList.AddDashedLine({ rcFrame.min.x + 4.f, flY }, { rcFrame.max.x - 4.f, flY }, WithGlobalAlpha(rStyle.Get(EColor::Border).MultiplyAlpha(.7f)), 3.f, 4.f);
			}
			if (aValues.empty())
				return;

			const rect rcPlot = rcFrame.Expanded(-4.f);
			auto PointAt = [&](std::size_t uIndex) {
				const float flT = aValues.size() > 1 ? static_cast<float>(uIndex) / static_cast<float>(aValues.size() - 1) : 0.f;
				const float flV = std::clamp((aValues[uIndex] - flMin) / (flMax - flMin), 0.f, 1.f);
				return vec2{ Lerp(rcPlot.min.x, rcPlot.max.x, flT), Lerp(rcPlot.max.y, rcPlot.min.y, flV) };
			};

			const color colAccent = WithGlobalAlpha(rStyle.Get(EColor::Accent));
			rList.PushClipRect(rcFrame);
			if (bHistogram)
			{
				const float flBarWidth = rcPlot.Width() / static_cast<float>(aValues.size());
				for (std::size_t i = 0; i < aValues.size(); ++i)
				{
					const float flV = std::clamp((aValues[i] - flMin) / (flMax - flMin), 0.f, 1.f);
					const float flX = rcPlot.min.x + flBarWidth * static_cast<float>(i);
					const vec2 vecTop{ flX + 1.f, Lerp(rcPlot.max.y, rcPlot.min.y, flV) };
					rList.AddRectFilled(vecTop, { flX + std::max(flBarWidth - 1.f, 1.f), rcPlot.max.y }, WithGlobalAlpha(rStyle.Get(EColor::AccentSoft).WithAlpha(70)));
					rList.AddRectFilled(vecTop, { flX + std::max(flBarWidth - 1.f, 1.f), vecTop.y + 1.5f }, colAccent);
				}
			}
			else
			{
				std::vector<vec2> aPoints(aValues.size());
				for (std::size_t i = 0; i < aValues.size(); ++i)
					aPoints[i] = PointAt(i);
				for (std::size_t i = 0; i + 1 < aPoints.size(); ++i)
					rList.AddQuadFilled(aPoints[i], aPoints[i + 1], { aPoints[i + 1].x, rcPlot.max.y }, { aPoints[i].x, rcPlot.max.y }, WithGlobalAlpha(rStyle.Get(EColor::Accent).WithAlpha(22)));
				rList.AddPolyline(aPoints, colAccent, false, 1.5f);
			}

			if (bHovered && aValues.size() > 1)
			{
				const float flT = std::clamp((g.clInputState.GetMousePosition().x - rcPlot.min.x) / rcPlot.Width(), 0.f, 1.f);
				const auto uIndex = static_cast<std::size_t>(std::round(flT * static_cast<float>(aValues.size() - 1)));
				const vec2 vecPoint = PointAt(uIndex);
				rList.AddLine({ vecPoint.x, rcPlot.min.y }, { vecPoint.x, rcPlot.max.y }, WithGlobalAlpha(rStyle.Get(EColor::TextDisabled)));
				rList.AddCircleFilled(vecPoint, 3.f, colAccent);
				rList.PopClipRect();
				SetTooltip(std::format("[{}] {:.3f}", uIndex, aValues[uIndex]));
				return;
			}
			rList.PopClipRect();
		}
	}

	void PlotLines(std::string_view strLabel, std::span<const float> aValues, float flMin, float flMax, const vec2& vecSize) { PlotEx(strLabel, aValues, flMin, flMax, vecSize, false); }
	void PlotHistogram(std::string_view strLabel, std::span<const float> aValues, float flMin, float flMax, const vec2& vecSize) { PlotEx(strLabel, aValues, flMin, flMax, vecSize, true); }

	namespace
	{
		bool TreeHeaderEx(std::string_view strLabel, bool bDefaultOpen, bool bFramed)
		{
			stContext& g = Ctx();
			stWindow* pWindow = CurrentWindow();
			if (pWindow->bSkipItems)
				return false;
			const stStyle& rStyle = g.stCurrentStyle;
			const ID uID = pWindow->GetID(strLabel);
			auto [itState, bInserted] = g.mapStorage.try_emplace(uID, bDefaultOpen ? 1 : 0);
			bool bOpen = itState->second != 0;

			stRegion& rRegion = pWindow->Region();
			const float flHeight = FrameHeight();
			const float flWidth = bFramed ? GetContentRegionAvail().x : MeasureText(VisibleLabel(strLabel)).x + MeasureText("[+] ").x + rStyle.vecFramePadding.x;
			const rect rcRow{ rRegion.vecCursor, rRegion.vecCursor + vec2{ flWidth, flHeight } };
			ItemSize(rcRow.Size(), rStyle.vecFramePadding.y);
			if (!ItemAdd(rcRow, uID))
				return bOpen;
			bool bHovered = false;
			const bool bPressed = ButtonBehavior(rcRow, uID, &bHovered, nullptr);
			bool bToggle = bPressed;
			if (g.uNavID == uID && g.pNavWindow == pWindow && g.iNavAdjust != 0)
				bToggle = (g.iNavAdjust > 0) != bOpen;
			if (bToggle)
			{
				bOpen = !bOpen;
				itState->second = bOpen ? 1 : 0;
			}
			const float flHover = AnimateBool(uID, bHovered || (g.uNavID == uID && g.bNavVisible));
			clDrawList& rList = pWindow->drawList;
			if (bFramed)
			{
				rList.AddRectFilled(rcRow.min, rcRow.max, WithGlobalAlpha(rStyle.Get(EColor::Surface2).Lerp(rStyle.Get(EColor::Hover), flHover)), rStyle.flRoundingSmall);
				rList.AddRect(rcRow.min, rcRow.max, WithGlobalAlpha(rStyle.Get(EColor::Border).Lerp(rStyle.Get(EColor::Accent), flHover * .6f)), rStyle.flRoundingSmall);
			}
			const vec2 vecPosition{ rcRow.min.x + (bFramed ? rStyle.vecFramePadding.x : 0.f), LabelY(rcRow) };
			DrawCheckGlyph(vecPosition, "[", bOpen ? "-" : "+", "] ", 1.f, rStyle.Get(EColor::TextDisabled), rStyle.Get(EColor::Accent));
			RenderText(vecPosition + vec2{ MeasureText("[+] ").x, 0.f }, strLabel, rStyle.Get(bOpen ? EColor::TextHeading : EColor::Text).Lerp(rStyle.Get(EColor::Accent), flHover * .4f));
			RenderNavHighlight(rcRow, uID);
			return bOpen;
		}
	}

	bool CollapsingHeader(std::string_view strLabel, bool bDefaultOpen)
	{
		return TreeHeaderEx(strLabel, bDefaultOpen, true);
	}

	bool TreeNode(std::string_view strLabel, bool bDefaultOpen)
	{
		const bool bOpen = TreeHeaderEx(strLabel, bDefaultOpen, false);
		if (bOpen)
		{
			stWindow* pWindow = CurrentWindow();
			pWindow->aIDStack.push_back(pWindow->GetID(strLabel));
			pWindow->aTreeStack.push_back(pWindow->aIDStack.back());
			Indent();
		}
		return bOpen;
	}

	void TreePop()
	{
		stWindow* pWindow = CurrentWindow();
		if (pWindow->aTreeStack.empty())
			return;
		pWindow->aTreeStack.pop_back();
		Unindent();
		PopID();
	}

	void Image(TextureHandle hTexture, const vec2& vecSize, const vec2& vecUVMin, const vec2& vecUVMax, color colTint)
	{
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return;
		stRegion& rRegion = pWindow->Region();
		const rect rcImage{ rRegion.vecCursor, rRegion.vecCursor + vecSize };
		ItemSize(vecSize);
		if (ItemAdd(rcImage, 0))
			pWindow->drawList.AddImage(hTexture, rcImage.min, rcImage.max, vecUVMin, vecUVMax, WithGlobalAlpha(colTint));
	}
}
