#include "OmegaGUI/UI/Internal.hpp"

namespace OmegaGUI
{
	namespace
	{
		constexpr float flPi = std::numbers::pi_v<float>;

		[[nodiscard]] bool ConditionPasses(ECondition eCondition, const stWindow* pWindow, bool bFirstUse)
		{
			switch (eCondition)
			{
			case ECondition::Always: return true;
			case ECondition::FirstUse: return bFirstUse;
			case ECondition::Appearing: return pWindow->bAppearing;
			}
			return false;
		}

		void DrawDotGrid(clDrawList& rList, const rect& rcArea, float flSpacing, color col)
		{
			if (col.A() == 0 || flSpacing < 4.f)
				return;
			const float flStartX = std::ceil(rcArea.min.x / flSpacing) * flSpacing;
			const float flStartY = std::ceil(rcArea.min.y / flSpacing) * flSpacing;
			for (float flY = flStartY; flY < rcArea.max.y; flY += flSpacing)
				for (float flX = flStartX; flX < rcArea.max.x; flX += flSpacing)
					rList.AddRectFilled({ flX, flY }, { flX + 1.f, flY + 1.f }, col);
		}

		bool WindowControlButton(stWindow* pWindow, ID uID, const rect& rcButton, color colBase, int iSymbol, float flAlpha)
		{
			stContext& g = Ctx();
			const stStyle& rStyle = g.stCurrentStyle;
			bool bHovered = false, bHeld = false;
			const bool bPressed = ButtonBehavior(rcButton, uID, &bHovered, &bHeld, EItemFlags::NoNav);
			const float flHover = AnimateBool(uID, bHovered || bHeld);
			clDrawList& rList = pWindow->drawList;
			const vec2 vecCenter = rcButton.Center();

			if (rStyle.eWindowControls == EWindowControls::Dots)
			{
				const float flRadius = rcButton.Width() * .5f;
				rList.AddCircleFilled(vecCenter, flRadius, colBase.MultiplyAlpha(flAlpha * (pWindow == g.pFocusedWindow || bHovered ? 1.f : .55f)));
				if (flHover > .01f)
				{
					const color colSymbol = color(40, 20, 20, static_cast<std::uint8_t>(200 * flHover * flAlpha));
					const float flSize = flRadius * .45f;
					switch (iSymbol)
					{
					case 0:
						rList.AddLine(vecCenter - vec2{ flSize, flSize } - vec2{ .5f, .5f }, vecCenter + vec2{ flSize, flSize } - vec2{ .5f, .5f }, colSymbol, 1.4f);
						rList.AddLine(vecCenter + vec2{ -flSize, flSize } - vec2{ .5f, .5f }, vecCenter + vec2{ flSize, -flSize } - vec2{ .5f, .5f }, colSymbol, 1.4f);
						break;
					case 1:
						rList.AddRectFilled(vecCenter - vec2{ flSize, .7f }, vecCenter + vec2{ flSize, .7f }, colSymbol);
						break;
					default:
						rList.AddRectFilled(vecCenter - vec2{ flSize, .7f }, vecCenter + vec2{ flSize, .7f }, colSymbol);
						rList.AddRectFilled(vecCenter - vec2{ .7f, flSize }, vecCenter + vec2{ .7f, flSize }, colSymbol);
						break;
					}
				}
			}
			else
			{
				static constexpr std::string_view s_aSymbols[] = { "[x]", "[_]", "[+]" };
				const std::string_view strSymbol = s_aSymbols[std::clamp(iSymbol, 0, 2)];
				const color colIdle = rStyle.Get(EColor::TextDisabled);
				const color colHot = iSymbol == 0 ? rStyle.Get(EColor::Danger) : rStyle.Get(EColor::Accent);
				const vec2 vecText = MeasureText(strSymbol);
				rList.AddText(vecCenter - vecText * .5f, colIdle.Lerp(colHot, flHover).MultiplyAlpha(flAlpha), strSymbol);
			}
			return bPressed;
		}
	}

	stWindow* CreateWindowState(std::string_view strName, EWindowFlags eFlags)
	{
		stContext& g = Ctx();
		auto pWindow = std::make_unique<stWindow>();
		pWindow->strName = std::string(strName);
		pWindow->uID = HashString(strName);
		pWindow->eFlags = eFlags;
		const float flCascade = 40.f + 28.f * static_cast<float>(g.mapWindows.size() % 8);
		pWindow->vecPosition = { flCascade, flCascade };
		pWindow->vecSize = pWindow->vecSizeFull = { 0.f, 0.f };
		stWindow* pRaw = pWindow.get();
		g.mapWindows.emplace(pRaw->uID, std::move(pWindow));
		g.aWindowOrder.push_back(pRaw);
		ApplySettings(pRaw);
		return pRaw;
	}

	void SetNextWindowPos(const vec2& vecPosition, ECondition eCondition, const vec2& vecPivot)
	{
		Ctx().stNext.optPosition = std::pair{ vecPosition, vecPivot };
		Ctx().stNext.ePositionCondition = eCondition;
	}

	void SetNextWindowSize(const vec2& vecSize, ECondition eCondition)
	{
		Ctx().stNext.optSize = vecSize;
		Ctx().stNext.eSizeCondition = eCondition;
	}

	void SetNextWindowSizeConstraints(const vec2& vecMin, const vec2& vecMax)
	{
		Ctx().stNext.optConstraints = std::pair{ vecMin, vecMax };
	}

	void SetNextWindowCollapsed(bool bCollapsed, ECondition eCondition)
	{
		Ctx().stNext.optCollapsed = bCollapsed;
		Ctx().stNext.eCollapsedCondition = eCondition;
	}

	void SetNextWindowFocus()
	{
		Ctx().stNext.bFocus = true;
	}

	void BeginRegion(stWindow* pWindow, ID uID, const rect& rcOuter, const rect& rcContent, bool bBorder, bool bScrollbar, bool bWheel)
	{
		stContext& g = Ctx();
		const stStyle& rStyle = g.stCurrentStyle;
		stScrollState& rScroll = pWindow->mapScroll[uID];

		if (bWheel && g.uWheelRegion == uID && g.pWheelWindow == pWindow)
		{
			const float flWheel = g.clInputState.GetMouseWheel();
			if (flWheel != 0.f)
			{
				const float flStep = std::max(FrameHeight() * 3.f, std::min(rcOuter.Height() * .25f, 160.f));
				rScroll.flTargetY = std::clamp((rScroll.flTargetY >= 0.f ? rScroll.flTargetY : rScroll.flScrollY) - flWheel * flStep, 0.f, rScroll.flMaxY);
			}
		}
		if (rScroll.flTargetY >= 0.f)
		{
			rScroll.flScrollY = rStyle.bAnimations ? ApproachExp(rScroll.flScrollY, rScroll.flTargetY, 18.f, g.flDeltaTime) : rScroll.flTargetY;
			if (std::abs(rScroll.flScrollY - rScroll.flTargetY) < .5f)
			{
				rScroll.flScrollY = rScroll.flTargetY;
				rScroll.flTargetY = -1.f;
			}
		}
		rScroll.flScrollY = std::clamp(rScroll.flScrollY, 0.f, rScroll.flMaxY);

		stRegion stNew;
		stNew.uID = uID;
		stNew.rcOuter = rcOuter;
		stNew.rcClip = pWindow->aRegions.empty() ? rcOuter : rcOuter.Clipped(pWindow->Region().rcClip);
		stNew.rcContent = rcContent;
		stNew.bBorder = bBorder;
		stNew.bScrollbar = bScrollbar;
		stNew.bWheel = bWheel;
		stNew.pScroll = &rScroll;
		if (bScrollbar && rScroll.flMaxY > 0.f)
			stNew.rcContent.max.x -= rStyle.flScrollbarSize + 2.f;
		stNew.vecCursorStart = { rcContent.min.x, rcContent.min.y - std::floor(rScroll.flScrollY) };
		stNew.vecCursor = stNew.vecCursorStart;
		stNew.vecCursorMax = stNew.vecCursorStart;
		stNew.vecCursorPrevLine = stNew.vecCursorStart;
		stNew.flLineStartX = rcContent.min.x;
		pWindow->drawList.PushClipRect(stNew.rcClip, false);
		pWindow->aRegions.push_back(std::move(stNew));
	}

	void EndRegion(stWindow* pWindow)
	{
		stContext& g = Ctx();
		const stStyle& rStyle = g.stCurrentStyle;
		stRegion& rRegion = pWindow->Region();
		stScrollState& rScroll = *rRegion.pScroll;

		const float flTopPadding = rRegion.rcContent.min.y - rRegion.rcOuter.min.y;
		const float flBottomPadding = std::max(0.f, rRegion.rcOuter.max.y - rRegion.rcContent.max.y);
		rScroll.flContentHeight = rRegion.vecCursorMax.y - rRegion.vecCursorStart.y;
		rScroll.flVisibleHeight = rRegion.rcOuter.Height();
		const float flExtent = flTopPadding + rScroll.flContentHeight + flBottomPadding;
		rScroll.flMaxY = std::max(0.f, flExtent - rRegion.rcOuter.Height());
		pWindow->drawList.PopClipRect();

		const ID uID = rRegion.uID;
		const rect rcOuter = rRegion.rcOuter;
		const bool bScrollbar = rRegion.bScrollbar;
		const bool bWheel = rRegion.bWheel;
		pWindow->aRegions.pop_back();

		const vec2 vecMouse = g.clInputState.GetMousePosition();
		const rect rcVisible = pWindow->aRegions.empty() ? rcOuter : rcOuter.Clipped(pWindow->Region().rcClip);
		if (bWheel && rScroll.flMaxY > 0.f && !g.uWheelRegionNext && g.pHoveredWindow == pWindow && rcVisible.Contains(vecMouse))
		{
			g.uWheelRegionNext = uID;
			g.pWheelWindowNext = pWindow;
		}

		if (!bScrollbar || rScroll.flMaxY <= 0.f || pWindow->bSkipItems)
			return;

		const rect rcTrack{ rcOuter.max.x - rStyle.flScrollbarSize - 3.f, rcOuter.min.y + 4.f, rcOuter.max.x - 3.f, rcOuter.max.y - 4.f };
		const float flThumbHeight = std::max(rcTrack.Height() * rScroll.flVisibleHeight / flExtent, 24.f);
		const float flTravel = std::max(rcTrack.Height() - flThumbHeight, 1.f);
		const float flThumbY = rcTrack.min.y + flTravel * (rScroll.flScrollY / rScroll.flMaxY);
		const rect rcThumb{ rcTrack.min.x, flThumbY, rcTrack.max.x, flThumbY + flThumbHeight };

		const ID uScrollID = HashString("#scrollbar", uID);
		bool bHovered = false, bHeld = false;
		pWindow->drawList.PushClipRect(rcVisible.Expanded(0.f), false);
		ButtonBehavior(rcTrack, uScrollID, &bHovered, &bHeld, EItemFlags::NoNav | EItemFlags::PressOnClick);
		if (bHeld)
		{
			if (g.bActiveIDJustActivated)
				g.vecActiveClickOffset.y = rcThumb.Contains(vecMouse) ? vecMouse.y - rcThumb.min.y : flThumbHeight * .5f;
			const float flRatio = std::clamp((vecMouse.y - g.vecActiveClickOffset.y - rcTrack.min.y) / flTravel, 0.f, 1.f);
			rScroll.flScrollY = flRatio * rScroll.flMaxY;
			rScroll.flTargetY = -1.f;
		}
		const float flHover = AnimateBool(uScrollID, bHovered || bHeld);
		pWindow->drawList.AddRectFilled(rcTrack.min, rcTrack.max, WithGlobalAlpha(rStyle.Get(EColor::Background).MultiplyAlpha(.5f * flHover)), rStyle.flScrollbarSize * .5f);
		pWindow->drawList.AddRectFilled(rcThumb.min, rcThumb.max, WithGlobalAlpha(rStyle.Get(EColor::Border).Lerp(rStyle.Get(EColor::Accent), flHover * .7f)), rStyle.flScrollbarSize * .5f);
		pWindow->drawList.PopClipRect();
	}

	bool Begin(std::string_view strName, bool* pOpen, EWindowFlags eFlags)
	{
		stContext& g = Ctx();
		const stStyle& rStyle = g.stCurrentStyle;
		clInput& rInput = g.clInputState;

		const ID uID = HashString(strName);
		stWindow* pWindow = FindWindowByID(uID);
		const bool bFirstUse = !pWindow;
		if (!pWindow)
			pWindow = CreateWindowState(strName, eFlags);

		const bool bPopup = HasFlag(eFlags, EWindowFlags::Popup | EWindowFlags::Tooltip | EWindowFlags::Modal);
		pWindow->eFlags = eFlags;
		pWindow->bAppearing = pWindow->iLastFrameActive < g.iFrame - 1;
		pWindow->iLastFrameActive = g.iFrame;
		pWindow->bActive = true;
		pWindow->pParentWindow = g.pCurrentWindow;
		g.aWindowStack.push_back(pWindow);
		g.pCurrentWindow = pWindow;

		if (pWindow->bAppearing && HasFlag(eFlags, EWindowFlags::AlwaysAutoResize))
			pWindow->iAutoFitFrames = 2;

		stNextWindow& rNext = g.stNext;
		if (rNext.optConstraints)
		{
			pWindow->vecSizeMin = rNext.optConstraints->first;
			pWindow->vecSizeMax = rNext.optConstraints->second;
		}
		if (rNext.optSize && ConditionPasses(rNext.eSizeCondition, pWindow, bFirstUse || !pWindow->vecSizeFull.x))
			pWindow->vecSize = pWindow->vecSizeFull = *rNext.optSize;
		if (rNext.optCollapsed && ConditionPasses(rNext.eCollapsedCondition, pWindow, bFirstUse))
			pWindow->bCollapsed = *rNext.optCollapsed;
		const bool bApplyPosition = rNext.optPosition && ConditionPasses(rNext.ePositionCondition, pWindow, bFirstUse || pWindow->bNew);
		const auto optPosition = rNext.optPosition;
		const bool bFocusRequested = rNext.bFocus;
		rNext.Clear();

		if (pWindow->vecSizeFull.x <= 0.f || pWindow->vecSizeFull.y <= 0.f)
			pWindow->vecSize = pWindow->vecSizeFull = HasFlag(eFlags, EWindowFlags::AlwaysAutoResize) ? vec2{ 32.f, 32.f } : vec2{ 420.f, 320.f } * g.stSettings.flScale;

		const float flTitleHeight = HasFlag(eFlags, EWindowFlags::NoTitleBar) ? 0.f : rStyle.flTitleBarHeight;
		const float flMenuHeight = HasFlag(eFlags, EWindowFlags::MenuBar) ? FrameHeight() + 8.f : 0.f;
		const vec2 vecPadding = HasFlag(eFlags, EWindowFlags::Tooltip) ? rStyle.vecFramePadding + vec2{ 3.f, 2.f } : (bPopup && !HasFlag(eFlags, EWindowFlags::Modal) ? vec2{ 6.f, 6.f } * g.stSettings.flScale : rStyle.vecWindowPadding);

		if (HasFlag(eFlags, EWindowFlags::AlwaysAutoResize) || pWindow->iAutoFitFrames > 0)
		{
			vec2 vecFit = pWindow->vecContentSize + vecPadding * 2.f + vec2{ 0.f, flTitleHeight + flMenuHeight };
			if (!HasFlag(eFlags, EWindowFlags::Tooltip | EWindowFlags::Popup))
				vecFit = Max(vecFit, rStyle.vecWindowMinSize);
			vecFit = Clamp(vecFit, pWindow->vecSizeMin, Min(pWindow->vecSizeMax, g.vecDisplaySize - vec2{ 16.f, 16.f }));
			pWindow->vecSize = pWindow->vecSizeFull = vecFit;
		}
		pWindow->bHiddenThisFrame = pWindow->iAutoFitFrames == 2;
		if (pWindow->iAutoFitFrames > 0)
			--pWindow->iAutoFitFrames;

		if (bApplyPosition)
		{
			pWindow->vecPosition = optPosition->first - pWindow->vecSizeFull * optPosition->second;
			pWindow->optAutoFitPosition = pWindow->iAutoFitFrames > 0 ? optPosition : std::nullopt;
			pWindow->vecAutoFitSize = pWindow->vecSizeFull;
		}
		else if (pWindow->optAutoFitPosition)
		{
			pWindow->vecPosition = pWindow->optAutoFitPosition->first - pWindow->vecSizeFull * pWindow->optAutoFitPosition->second;
			if (pWindow->iAutoFitFrames == 0 && pWindow->vecSizeFull.x == pWindow->vecAutoFitSize.x && pWindow->vecSizeFull.y == pWindow->vecAutoFitSize.y)
				pWindow->optAutoFitPosition.reset();
			pWindow->vecAutoFitSize = pWindow->vecSizeFull;
		}
		if (pWindow->bMaximized)
		{
			pWindow->vecPosition = {};
			pWindow->vecSize = pWindow->vecSizeFull = g.vecDisplaySize;
		}
		pWindow->bNew = false;

		if (bFocusRequested || (bFirstUse && !HasFlag(eFlags, EWindowFlags::NoBringToFront | EWindowFlags::Tooltip | EWindowFlags::NoInputs)))
			FocusWindowInternal(pWindow);
		if (bPopup)
			BringWindowToFront(pWindow);

		const bool bShown = HasFlag(eFlags, EWindowFlags::AlwaysVisible) || g.flVisibility > .01f;
		pWindow->bHiddenThisFrame = pWindow->bHiddenThisFrame || !bShown;

		if (!bPopup && !pWindow->bMaximized)
		{
			pWindow->vecPosition.x = std::clamp(pWindow->vecPosition.x, -pWindow->vecSize.x + 60.f, std::max(g.vecDisplaySize.x - 60.f, 0.f));
			pWindow->vecPosition.y = std::clamp(pWindow->vecPosition.y, 0.f, std::max(g.vecDisplaySize.y - std::max(flTitleHeight, 24.f), 0.f));
		}
		else if (bPopup)
		{
			pWindow->vecPosition = Clamp(pWindow->vecPosition, { 4.f, 4.f }, Max(g.vecDisplaySize - pWindow->vecSize - vec2{ 4.f, 4.f }, { 4.f, 4.f }));
		}

		const bool bCanCollapse = flTitleHeight > 0.f && !HasFlag(eFlags, EWindowFlags::NoCollapse);
		if (!bCanCollapse)
			pWindow->bCollapsed = false;
		pWindow->flCollapse = Animate(HashString("#collapse", uID), pWindow->bCollapsed ? 1.f : 0.f, 20.f);
		pWindow->vecSize.x = pWindow->vecSizeFull.x;
		pWindow->vecSize.y = Lerp(pWindow->vecSizeFull.y, flTitleHeight, pWindow->flCollapse);

		const ID uFadeID = HashString("#fade", uID);
		if (pWindow->bAppearing && rStyle.bAnimations && !HasFlag(eFlags, EWindowFlags::Tooltip))
			g.mapAnimations[uFadeID] = 0.f;
		const float flFade = Animate(uFadeID, 1.f, bPopup ? 30.f : 14.f);
		pWindow->flAlpha = flFade * (HasFlag(eFlags, EWindowFlags::AlwaysVisible) ? 1.f : g.flVisibility);

		pWindow->drawList.Reset(&g.stShared);
		pWindow->aIDStack.assign(1, uID);
		pWindow->aRegions.clear();
		pWindow->aItemWidthStack.clear();
		pWindow->aTreeStack.clear();
		pWindow->aSections.clear();
		pWindow->flNextItemWidth = -1.f;
		pWindow->stLast = {};
		pWindow->bInMenuBar = false;
		pWindow->bResizeHovered = false;
		pWindow->bSkipItems = false;

		const rect rcWindow = pWindow->Rect();
		pWindow->rcTitleBar = { rcWindow.min, { rcWindow.max.x, rcWindow.min.y + flTitleHeight } };
		pWindow->rcMenuBar = { { rcWindow.min.x, pWindow->rcTitleBar.max.y }, { rcWindow.max.x, pWindow->rcTitleBar.max.y + flMenuHeight } };
		pWindow->drawList.PushClipRect(rcWindow.Expanded(rStyle.flShadowSize + 16.f), false);

		const bool bInteractive = g.bVisible && !pWindow->bHiddenThisFrame && !HasFlag(eFlags, EWindowFlags::NoInputs);
		const float flAlpha = pWindow->flAlpha;
		clDrawList& rList = pWindow->drawList;

		if (HasFlag(eFlags, EWindowFlags::Modal) && !pWindow->bHiddenThisFrame)
		{
			rList.PushClipRectFullScreen();
			rList.AddRectFilled({}, g.vecDisplaySize, rStyle.Get(EColor::ModalDim).MultiplyAlpha(flAlpha));
			rList.PopClipRect();
		}

		const bool bDrawBackground = !HasFlag(eFlags, EWindowFlags::NoBackground) && !pWindow->bHiddenThisFrame;
		const float flRounding = bPopup && !HasFlag(eFlags, EWindowFlags::Modal) ? rStyle.flRounding : rStyle.flWindowRounding;
		if (bDrawBackground)
		{
			rList.AddShadowRect(rcWindow.min, rcWindow.max, rStyle.Get(EColor::Shadow).MultiplyAlpha(flAlpha), bPopup ? rStyle.flShadowSize * .5f : rStyle.flShadowSize, flRounding, { 0.f, bPopup ? 4.f : 10.f });
			rList.AddRectFilled(rcWindow.min, rcWindow.max, rStyle.Get(bPopup ? EColor::Surface2 : EColor::Surface).MultiplyAlpha(flAlpha), flRounding);
			if (HasFlag(eFlags, EWindowFlags::DotGrid) || (rStyle.bDotGrid && !bPopup))
				DrawDotGrid(rList, { { rcWindow.min.x, pWindow->rcMenuBar.max.y }, rcWindow.max }, rStyle.flDotSpacing, rStyle.Get(EColor::DotGrid).MultiplyAlpha(flAlpha));
		}

		if (flTitleHeight > 0.f && !pWindow->bHiddenThisFrame)
		{
			const bool bFocused = g.pFocusedWindow == pWindow;
			const rect& rcTitle = pWindow->rcTitleBar;
			if (bDrawBackground)
			{
				rList.AddRectFilled(rcTitle.min, rcTitle.max, rStyle.Get(EColor::Surface2).MultiplyAlpha(flAlpha), flRounding, pWindow->flCollapse > .99f ? ECorner::All : ECorner::Top);
				if (pWindow->flCollapse < .99f)
					rList.AddRectFilled({ rcTitle.min.x, rcTitle.max.y - 1.f }, rcTitle.max, rStyle.Get(EColor::Border).MultiplyAlpha(flAlpha));
			}

			float flControlsRight = rcTitle.max.x - 10.f;
			float flControlsLeft = rcTitle.min.x + 10.f;
			const float flDot = std::round(rStyle.flTitleBarHeight * .38f);
			const float flCenterY = std::floor(rcTitle.Center().y);
			struct stControl { int iSymbol; EColor eColor; bool bEnabled; };
			const stControl aControls[] = {
				{ 0, EColor::ControlClose, pOpen != nullptr || HasFlag(eFlags, EWindowFlags::CloseHidesMenu) },
				{ 1, EColor::ControlMinimize, bCanCollapse },
				{ 2, EColor::ControlMaximize, !HasFlag(eFlags, EWindowFlags::NoMaximize | EWindowFlags::NoResize | EWindowFlags::AlwaysAutoResize) && !bPopup },
			};
			const bool bDots = rStyle.eWindowControls == EWindowControls::Dots;
			const float flBracketWidth = MeasureText("[x]").x;
			for (int i = 0; i < 3; ++i)
			{
				const stControl& rControl = aControls[bDots ? i : 2 - i];
				if (!rControl.bEnabled)
					continue;
				rect rcButton;
				if (bDots)
				{
					rcButton = { { flControlsLeft, flCenterY - flDot * .5f }, { flControlsLeft + flDot, flCenterY + flDot * .5f } };
					flControlsLeft += flDot + 7.f;
				}
				else
				{
					rcButton = { { flControlsRight - flBracketWidth, rcTitle.min.y + 2.f }, { flControlsRight, rcTitle.max.y - 2.f } };
					flControlsRight -= flBracketWidth + 6.f;
				}
				if (!bInteractive)
				{
					if (bDots)
						rList.AddCircleFilled(rcButton.Center(), flDot * .5f, rStyle.Get(rControl.eColor).MultiplyAlpha(flAlpha * (bFocused ? 1.f : .55f)));
					continue;
				}
				if (WindowControlButton(pWindow, HashString(std::format("#control{}", rControl.iSymbol), uID), rcButton, rStyle.Get(rControl.eColor), rControl.iSymbol, flAlpha))
				{
					switch (rControl.iSymbol)
					{
					case 0:
						if (HasFlag(eFlags, EWindowFlags::CloseHidesMenu))
							SetVisible(false);
						else
							*pOpen = false;
						break;
					case 1:
						pWindow->bCollapsed = !pWindow->bCollapsed;
						MarkSettingsDirty();
						break;
					default:
						if (pWindow->bMaximized)
						{
							pWindow->bMaximized = false;
							pWindow->vecPosition = pWindow->rcRestore.min;
							pWindow->vecSize = pWindow->vecSizeFull = pWindow->rcRestore.Size();
						}
						else
						{
							pWindow->rcRestore = { pWindow->vecPosition, pWindow->vecPosition + pWindow->vecSizeFull };
							pWindow->bMaximized = true;
							pWindow->bCollapsed = false;
						}
						MarkSettingsDirty();
						break;
					}
				}
			}

			std::string_view strTitle = VisibleLabel(strName);
			const color colTitle = rStyle.Get(bFocused ? EColor::TextMuted : EColor::TextDisabled).MultiplyAlpha(flAlpha);
			const rect rcTitleText{ { flControlsLeft + 4.f, rcTitle.min.y }, { flControlsRight - 4.f, rcTitle.max.y } };
			if (bDots)
			{
				const vec2 vecTitle = MeasureText(strTitle);
				vec2 vecPosition{ std::floor(rcTitle.Center().x - vecTitle.x * .5f), std::floor(flCenterY - vecTitle.y * .5f) };
				vecPosition.x = std::max(vecPosition.x, rcTitleText.min.x);
				rList.PushClipRect(rcTitleText);
				rList.AddText(vecPosition, colTitle, strTitle);
				rList.PopClipRect();
			}
			else
			{
				const vec2 vecPrompt = MeasureText("> ");
				const float flY = std::floor(flCenterY - vecPrompt.y * .5f);
				rList.PushClipRect({ { rcTitle.min.x + 10.f, rcTitle.min.y }, rcTitleText.max });
				rList.AddText({ rcTitle.min.x + 12.f, flY }, rStyle.Get(EColor::Accent).MultiplyAlpha(flAlpha), "> ");
				rList.AddText({ rcTitle.min.x + 12.f + vecPrompt.x, flY }, colTitle, strTitle);
				rList.PopClipRect();
			}

			if (bInteractive && !HasFlag(eFlags, EWindowFlags::NoMove) && g.pHoveredWindow == pWindow && !g.uActiveID && !g.pResizingWindow)
			{
				const vec2 vecMouse = rInput.GetMousePosition();
				const bool bOverControl = g.uHoveredID != 0;
				if (rcTitle.Contains(vecMouse) && !bOverControl)
				{
					if (rInput.IsMouseDoubleClicked(EMouseButton::Left) && bCanCollapse)
					{
						pWindow->bCollapsed = !pWindow->bCollapsed;
						MarkSettingsDirty();
						g.pMovingWindow = nullptr;
					}
					else if (rInput.IsMouseClicked(EMouseButton::Left))
					{
						g.pMovingWindow = pWindow;
						g.vecMoveOffset = vecMouse - pWindow->vecPosition;
					}
				}
			}
		}

		if (bInteractive && !HasFlag(eFlags, EWindowFlags::NoResize | EWindowFlags::AlwaysAutoResize) && !pWindow->bCollapsed && pWindow->flCollapse < .01f && !pWindow->bMaximized)
		{
			const vec2 vecMouse = rInput.GetMousePosition();
			const float flBorder = rStyle.flResizeBorder;
			int iEdges = 0;
			if (g.pHoveredWindow == pWindow && !g.uActiveID && !g.pMovingWindow && !g.pResizingWindow && rcWindow.Expanded(flBorder * .5f).Contains(vecMouse))
			{
				const float flCorner = flBorder * 2.5f;
				if (vecMouse.x < rcWindow.min.x + flBorder * .5f) iEdges |= 1;
				if (vecMouse.x > rcWindow.max.x - flBorder * .5f) iEdges |= 2;
				if (vecMouse.y < rcWindow.min.y + flBorder * .5f) iEdges |= 4;
				if (vecMouse.y > rcWindow.max.y - flBorder * .5f) iEdges |= 8;
				if (vecMouse.x > rcWindow.max.x - flCorner && vecMouse.y > rcWindow.max.y - flCorner)
					iEdges = 2 | 8;
			}
			if (iEdges)
			{
				pWindow->bResizeHovered = true;
				static constexpr EMouseCursor s_aCursors[16] = {
					EMouseCursor::Arrow, EMouseCursor::ResizeEW, EMouseCursor::ResizeEW, EMouseCursor::Arrow,
					EMouseCursor::ResizeNS, EMouseCursor::ResizeNWSE, EMouseCursor::ResizeNESW, EMouseCursor::Arrow,
					EMouseCursor::ResizeNS, EMouseCursor::ResizeNESW, EMouseCursor::ResizeNWSE, EMouseCursor::Arrow,
					EMouseCursor::Arrow, EMouseCursor::Arrow, EMouseCursor::Arrow, EMouseCursor::Arrow,
				};
				g.eCursor = s_aCursors[iEdges];
				if (rInput.IsMouseClicked(EMouseButton::Left))
				{
					g.pResizingWindow = pWindow;
					g.iResizeEdges = iEdges;
					g.rcResizeStart = rcWindow;
					g.vecResizeMouseStart = vecMouse;
					g.pMovingWindow = nullptr;
				}
			}
		}

		const rect rcInner{ { rcWindow.min.x, pWindow->rcMenuBar.max.y }, rcWindow.max };
		pWindow->rcInner = rcInner;
		if (flMenuHeight > 0.f && bDrawBackground && pWindow->flCollapse < .99f)
		{
			rList.AddRectFilled({ pWindow->rcMenuBar.min.x, pWindow->rcMenuBar.max.y - 1.f }, pWindow->rcMenuBar.max, rStyle.Get(EColor::Border).MultiplyAlpha(flAlpha));
		}

		const rect rcContent{ rcInner.min + vecPadding, Max(rcInner.max - vecPadding, rcInner.min + vecPadding) };
		pWindow->bSkipItems = !bShown || pWindow->flCollapse > .99f;
		BeginRegion(pWindow, uID, { rcInner.min, { rcInner.max.x, rcInner.max.y - (HasFlag(eFlags, EWindowFlags::NoBackground) ? 0.f : 1.f) } }, rcContent, false, !HasFlag(eFlags, EWindowFlags::NoScrollbar), !HasFlag(eFlags, EWindowFlags::NoScrollWithMouse));
		return !pWindow->bSkipItems;
	}

	void End()
	{
		stContext& g = Ctx();
		stWindow* pWindow = g.pCurrentWindow;
		if (!pWindow)
			return;
		const stStyle& rStyle = g.stCurrentStyle;

		if (pWindow->bInMenuBar)
			EndMenuBar();
		while (pWindow->aRegions.size() > 1)
			EndRegion(pWindow);
		if (!pWindow->aRegions.empty())
		{
			const stRegion& rRoot = pWindow->Region();
			pWindow->vecContentSize = Max(rRoot.vecCursorMax - rRoot.vecCursorStart, { 0.f, 0.f });
			EndRegion(pWindow);
		}

		const rect rcWindow = pWindow->Rect();
		const bool bInteractive = g.bVisible && !pWindow->bHiddenThisFrame && !HasFlag(pWindow->eFlags, EWindowFlags::NoInputs);
		if (bInteractive && HasFlag(pWindow->eFlags, EWindowFlags::NoTitleBar) && !HasFlag(pWindow->eFlags, EWindowFlags::NoMove | EWindowFlags::Popup | EWindowFlags::Tooltip) &&
			g.pHoveredWindow == pWindow && !g.uHoveredID && !g.uActiveID && !g.pResizingWindow && !pWindow->bResizeHovered && g.clInputState.IsMouseClicked(EMouseButton::Left))
		{
			g.pMovingWindow = pWindow;
			g.vecMoveOffset = g.clInputState.GetMousePosition() - pWindow->vecPosition;
		}

		if (!pWindow->bHiddenThisFrame && !HasFlag(pWindow->eFlags, EWindowFlags::NoBackground))
		{
			const bool bPopup = HasFlag(pWindow->eFlags, EWindowFlags::Popup | EWindowFlags::Tooltip) && !HasFlag(pWindow->eFlags, EWindowFlags::Modal);
			const float flRounding = bPopup ? rStyle.flRounding : rStyle.flWindowRounding;
			const bool bFocused = g.pFocusedWindow == pWindow || g.pNavWindow == pWindow;
			const color colBorder = rStyle.Get(bFocused && !bPopup ? EColor::BorderHover : EColor::Border);
			pWindow->drawList.AddRect(rcWindow.min, rcWindow.max, colBorder.MultiplyAlpha(pWindow->flAlpha), flRounding);
			if (g.bNavVisible && g.pNavWindow == pWindow && g.bGamepadWindowManip)
				pWindow->drawList.AddRect(rcWindow.min - vec2{ 2.f, 2.f }, rcWindow.max + vec2{ 2.f, 2.f }, rStyle.Get(EColor::Accent).MultiplyAlpha(pWindow->flAlpha), flRounding + 2.f, 1.5f);

			if (!HasFlag(pWindow->eFlags, EWindowFlags::NoResize | EWindowFlags::AlwaysAutoResize) && !bPopup && pWindow->flCollapse < .01f && !pWindow->bMaximized)
			{
				const bool bGripHot = g.pResizingWindow == pWindow || (pWindow->bResizeHovered && g.eCursor == EMouseCursor::ResizeNWSE);
				const color colGrip = (bGripHot ? rStyle.Get(EColor::Accent) : rStyle.Get(EColor::TextDisabled)).MultiplyAlpha(pWindow->flAlpha);
				const vec2 vecCorner = rcWindow.max - vec2{ 4.f, 4.f };
				for (int i = 0; i < 3; ++i)
				{
					const float flOffset = 4.f + 4.f * static_cast<float>(i);
					pWindow->drawList.AddLine(vecCorner - vec2{ flOffset, 0.f }, vecCorner - vec2{ 0.f, flOffset }, colGrip, 1.f);
				}
			}
		}

		pWindow->drawList.PopClipRect();
		g.aWindowStack.pop_back();
		g.pCurrentWindow = g.aWindowStack.empty() ? nullptr : g.aWindowStack.back();
	}

	bool BeginChild(std::string_view strID, const vec2& vecSize, bool bBorder, EWindowFlags eFlags)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		const stStyle& rStyle = g.stCurrentStyle;
		const ID uID = pWindow->GetID(strID);
		stRegion& rParent = pWindow->Region();

		const vec2 vecAvailable = GetContentRegionAvail();
		vec2 vecChild = vecSize;
		if (vecChild.x <= 0.f) vecChild.x = std::max(vecAvailable.x + vecChild.x, 16.f);
		if (vecChild.y <= 0.f) vecChild.y = std::max(vecAvailable.y + vecChild.y, 16.f);
		const rect rcOuter{ rParent.vecCursor, rParent.vecCursor + vecChild.Floor() };

		ItemSize(vecChild);
		pWindow->aIDStack.push_back(uID);
		const bool bVisible = rcOuter.Overlaps(pWindow->Region().rcClip) && !pWindow->bSkipItems;

		if (bVisible && bBorder)
		{
			pWindow->drawList.AddRectFilled(rcOuter.min, rcOuter.max, WithGlobalAlpha(rStyle.Get(EColor::Background).MultiplyAlpha(.55f)), rStyle.flRounding);
			pWindow->drawList.AddRect(rcOuter.min, rcOuter.max, WithGlobalAlpha(rStyle.Get(EColor::Border)), rStyle.flRounding);
		}
		const vec2 vecPadding = bBorder ? rStyle.vecFramePadding + vec2{ 3.f, 3.f } : vec2{};
		BeginRegion(pWindow, uID, rcOuter.Expanded(-1.f), { rcOuter.min + vecPadding, rcOuter.max - vecPadding }, bBorder, !HasFlag(eFlags, EWindowFlags::NoScrollbar), !HasFlag(eFlags, EWindowFlags::NoScrollWithMouse));
		return bVisible;
	}

	void EndChild()
	{
		stWindow* pWindow = CurrentWindow();
		if (pWindow->aRegions.size() <= 1)
			return;
		EndRegion(pWindow);
		pWindow->aIDStack.pop_back();
	}

	vec2 GetWindowPos() { return CurrentWindow()->vecPosition; }
	vec2 GetWindowSize() { return CurrentWindow()->vecSize; }
	bool IsWindowFocused() { return Ctx().pFocusedWindow == CurrentWindow(); }
	bool IsWindowHovered() { return Ctx().pHoveredWindow == CurrentWindow(); }
	bool IsWindowCollapsed() { return CurrentWindow()->bCollapsed; }

	void SetWindowPos(std::string_view strName, const vec2& vecPosition)
	{
		if (stWindow* pWindow = FindWindowByName(strName))
			pWindow->vecPosition = vecPosition;
	}

	void SetWindowSize(std::string_view strName, const vec2& vecSize)
	{
		if (stWindow* pWindow = FindWindowByName(strName))
			pWindow->vecSize = pWindow->vecSizeFull = vecSize;
	}

	void SetWindowCollapsed(std::string_view strName, bool bCollapsed)
	{
		if (stWindow* pWindow = FindWindowByName(strName))
			pWindow->bCollapsed = bCollapsed;
	}

	float GetScrollY() { return CurrentWindow()->Region().pScroll->flScrollY; }
	float GetScrollMaxY() { return CurrentWindow()->Region().pScroll->flMaxY; }

	void SetScrollY(float flScroll)
	{
		stScrollState& rScroll = *CurrentWindow()->Region().pScroll;
		rScroll.flTargetY = std::clamp(flScroll, 0.f, std::max(rScroll.flMaxY, flScroll));
	}

	void SetScrollHereY(float flRatio)
	{
		stRegion& rRegion = CurrentWindow()->Region();
		const float flLocalY = rRegion.vecCursor.y - rRegion.vecCursorStart.y;
		const float flVisible = rRegion.rcOuter.Height();
		rRegion.pScroll->flTargetY = std::max(0.f, flLocalY - flVisible * flRatio);
	}

	void OpenPopupEx(ID uID, const rect& rcAnchor, bool bModal, bool bMenu)
	{
		stContext& g = Ctx();
		const std::size_t uLevel = g.aBeginPopupStack.size();
		if (uLevel < g.aOpenPopups.size() && g.aOpenPopups[uLevel].uID == uID)
			return;
		g.aOpenPopups.resize(std::min(uLevel, g.aOpenPopups.size()));
		stPopup stNew;
		stNew.uID = uID;
		stNew.uParentWindowID = g.pCurrentWindow ? g.pCurrentWindow->uID : 0;
		stNew.vecOpenPosition = g.clInputState.IsMouseValid() && !g.bNavVisible ? g.clInputState.GetMousePosition() : rcAnchor.Center();
		stNew.rcAnchor = rcAnchor;
		stNew.iOpenFrame = g.iFrame;
		stNew.bModal = bModal;
		stNew.bMenu = bMenu;
		g.aOpenPopups.push_back(stNew);
	}

	bool IsAnyPopupOpen()
	{
		return !Ctx().aOpenPopups.empty();
	}

	bool IsPopupOpenID(ID uID)
	{
		return std::ranges::any_of(Ctx().aOpenPopups, [&](const stPopup& rPopup) { return rPopup.uID == uID; });
	}

	bool BeginPopupEx(ID uID, EWindowFlags eFlags, std::string_view strName)
	{
		stContext& g = Ctx();
		const auto itPopup = std::ranges::find(g.aOpenPopups, uID, &stPopup::uID);
		if (itPopup == g.aOpenPopups.end() || !g.bVisible)
		{
			g.stNext.Clear();
			return false;
		}
		stPopup stPopupCopy = *itPopup;
		const bool bModal = stPopupCopy.bModal;

		if (!g.stNext.optPosition)
		{
			if (bModal)
				SetNextWindowPos(g.vecDisplaySize * .5f, ECondition::Appearing, { .5f, .5f });
			else if (stPopupCopy.bMenu)
				SetNextWindowPos({ stPopupCopy.rcAnchor.min.x, stPopupCopy.rcAnchor.max.y + 3.f }, ECondition::Always);
			else
				SetNextWindowPos(stPopupCopy.vecOpenPosition + vec2{ 2.f, 2.f }, ECondition::Appearing);
		}

		EWindowFlags eWindowFlags = eFlags | EWindowFlags::Popup | EWindowFlags::NoSavedSettings | EWindowFlags::NoCollapse | EWindowFlags::NoMaximize | EWindowFlags::AlwaysAutoResize | EWindowFlags::NoResize;
		if (bModal)
			eWindowFlags |= EWindowFlags::Modal;
		else
			eWindowFlags |= EWindowFlags::NoTitleBar | EWindowFlags::NoMove;

		const std::string strWindowName = strName.empty() ? std::format("##Popup_{:08X}", uID) : std::string(strName);
		const bool bOpen = Begin(strWindowName, nullptr, eWindowFlags);
		stWindow* pWindow = g.pCurrentWindow;
		pWindow->uPopupID = uID;
		for (stPopup& rPopup : g.aOpenPopups)
			if (rPopup.uID == uID)
				rPopup.pWindow = pWindow;

		if (stPopupCopy.bMenu && !bModal && pWindow->vecPosition.y + pWindow->vecSize.y > g.vecDisplaySize.y - 4.f)
			pWindow->vecPosition.y = std::max(4.f, stPopupCopy.rcAnchor.min.y - pWindow->vecSize.y - 3.f);

		if (pWindow->bAppearing && !pWindow->bHiddenThisFrame)
		{
			FocusWindowInternal(pWindow);
			g.uNavID = 0;
		}
		else if (pWindow->bAppearing)
			FocusWindowInternal(pWindow);

		g.aBeginPopupStack.push_back(stPopupCopy);
		g.aBeginPopupStack.back().pWindow = pWindow;
		if (!bOpen)
		{
			EndPopup();
			return false;
		}
		return true;
	}

	void OpenPopup(std::string_view strID)
	{
		stWindow* pWindow = CurrentWindow();
		const ID uID = pWindow ? pWindow->GetID(strID) : HashString(strID);
		OpenPopupEx(uID, pWindow ? pWindow->stLast.rcItem : rect{}, false);
	}

	bool BeginPopup(std::string_view strID)
	{
		stWindow* pWindow = CurrentWindow();
		return BeginPopupEx(pWindow ? pWindow->GetID(strID) : HashString(strID), EWindowFlags::None, {});
	}

	bool BeginPopupModal(std::string_view strName, bool* pOpen)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		const ID uID = pWindow ? pWindow->GetID(strName) : HashString(strName);
		const auto itPopup = std::ranges::find(g.aOpenPopups, uID, &stPopup::uID);
		if (itPopup == g.aOpenPopups.end())
			return false;
		itPopup->bModal = true;
		const bool bResult = BeginPopupEx(uID, EWindowFlags::None, strName);
		if (bResult && pOpen && !*pOpen)
		{
			CloseCurrentPopup();
			*pOpen = true;
		}
		return bResult;
	}

	bool BeginPopupContextItem(std::string_view strID)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		const ID uID = strID.empty() ? HashString("#context", pWindow->stLast.uID) : pWindow->GetID(strID);
		const bool bRightClicked = pWindow->stLast.bHovered && g.clInputState.IsMouseReleased(EMouseButton::Right);
		const bool bGamepad = g.uNavID == pWindow->stLast.uID && g.bNavVisible && g.clInputState.IsGamepadPressed(EGamepadButton::FaceLeft);
		if (bRightClicked || bGamepad)
			OpenPopupEx(uID, pWindow->stLast.rcItem, false);
		return BeginPopupEx(uID, EWindowFlags::None, {});
	}

	void EndPopup()
	{
		stContext& g = Ctx();
		if (g.aBeginPopupStack.empty())
			return;
		End();
		g.aBeginPopupStack.pop_back();
	}

	void CloseCurrentPopup()
	{
		stContext& g = Ctx();
		if (g.aBeginPopupStack.empty())
			return;
		const ID uID = g.aBeginPopupStack.back().uID;
		for (std::size_t i = 0; i < g.aOpenPopups.size(); ++i)
			if (g.aOpenPopups[i].uID == uID)
			{
				const ID uParent = g.aOpenPopups[i].uParentWindowID;
				ClosePopupToLevel(i);
				if (stWindow* pParent = FindWindowByID(uParent))
					FocusWindowInternal(pParent);
				return;
			}
	}

	bool IsPopupOpen(std::string_view strID)
	{
		stWindow* pWindow = CurrentWindow();
		return IsPopupOpenID(pWindow ? pWindow->GetID(strID) : HashString(strID));
	}

	bool BeginTooltip()
	{
		stContext& g = Ctx();
		if (!g.bVisible)
			return false;
		vec2 vecPosition = g.clInputState.GetMousePosition() + vec2{ 16.f, 18.f };
		if (g.bNavVisible && g.pNavWindow)
			if (const auto itItem = std::ranges::find(g.pNavWindow->aNavItemsPrevious, g.uNavID, &stNavItem::uID); itItem != g.pNavWindow->aNavItemsPrevious.end())
				vecPosition = { itItem->rcItem.min.x, itItem->rcItem.max.y + 6.f };
		SetNextWindowPos(vecPosition, ECondition::Always);
		const bool bOpen = Begin("##Tooltip", nullptr, EWindowFlags::Tooltip | EWindowFlags::NoTitleBar | EWindowFlags::AlwaysAutoResize | EWindowFlags::NoInputs | EWindowFlags::NoNav |
			EWindowFlags::NoSavedSettings | EWindowFlags::NoResize | EWindowFlags::NoMove | EWindowFlags::NoScrollbar);
		g.bTooltipRequested = true;
		return bOpen;
	}

	void EndTooltip()
	{
		End();
	}

	void SetTooltip(std::string_view strText)
	{
		if (BeginTooltip())
		{
			stWindow* pWindow = CurrentWindow();
			const float flWrap = 320.f * Ctx().stSettings.flScale;
			const vec2 vecSize = MeasureText(strText, flWrap);
			const vec2 vecPosition = pWindow->Region().vecCursor;
			ItemSize(vecSize);
			pWindow->drawList.AddText(vecPosition, WithGlobalAlpha(Style().Get(EColor::Text)), strText, 0.f, flWrap);
		}
		EndTooltip();
	}

	void ItemTooltip(std::string_view strText)
	{
		stContext& g = Ctx();
		const stWindow* pWindow = CurrentWindow();
		const stLastItem& rLast = pWindow->stLast;
		const bool bHovered = rLast.bHovered && g.uHoverTimerID == rLast.uID && g.flHoverTimer >= g.stCurrentStyle.flTooltipDelay;
		const bool bNav = g.bNavVisible && g.uNavID == rLast.uID && rLast.uID != 0 && g.pNavWindow == pWindow;
		if ((bHovered || bNav) && !g.uActiveID)
			SetTooltip(strText);
	}
}
