#include "OmegaGUI/UI/Internal.hpp"

namespace OmegaGUI
{
	namespace
	{
		constexpr float kTabScrollbarGap = 2.f;

		[[nodiscard]] float TabScrollbarThickness(const stStyle& rStyle)
		{
			return std::max(4.f, std::floor(rStyle.flScrollbarSize * .5f));
		}

		[[nodiscard]] float TabScrollMax(const stTabBar& rBar)
		{
			return rBar.bScrollable ? std::max(0.f, rBar.flContentExtent - rBar.flViewExtent) : 0.f;
		}

		void RevealTab(stTabBar& rBar, float flStart, float flEnd, float flViewStart)
		{
			const float flViewEnd = flViewStart + rBar.flViewExtent;
			float flNew = rBar.flScroll;
			if (flStart < flViewStart)
				flNew -= flViewStart - flStart;
			else if (flEnd > flViewEnd)
				flNew += flEnd - flViewEnd;
			if (flNew != rBar.flScroll)
				rBar.flScrollTarget = std::clamp(flNew, 0.f, TabScrollMax(rBar));
		}

		void UpdateTabScroll(stContext& g, stWindow* pWindow, stTabBar& rBar, bool bVertical)
		{
			const stStyle& rStyle = g.stCurrentStyle;
			rBar.bScrollable = !HasFlag(rBar.eFlags, ETabBarFlags::NoScroll) && !HasFlag(rBar.eFlags, ETabBarFlags::Stretch) && rBar.flViewExtent > 0.f && rBar.flContentExtent > rBar.flViewExtent + .5f;
			if (!rBar.bScrollable)
			{
				rBar.flScroll = 0.f;
				rBar.flScrollTarget = -1.f;
				return;
			}
			const float flMax = TabScrollMax(rBar);
			if (g.uWheelRegion == rBar.uID && g.pWheelWindow == pWindow)
			{
				const float flWheel = g.clInputState.GetMouseWheel();
				if (flWheel != 0.f)
				{
					const float flStep = rStyle.flTabHeight * (bVertical ? 2.f : 3.f);
					rBar.flScrollTarget = std::clamp((rBar.flScrollTarget >= 0.f ? rBar.flScrollTarget : rBar.flScroll) - flWheel * flStep, 0.f, flMax);
				}
			}
			if (rBar.flScrollTarget >= 0.f)
			{
				rBar.flScroll = rStyle.bAnimations ? ApproachExp(rBar.flScroll, rBar.flScrollTarget, 18.f, g.flDeltaTime) : rBar.flScrollTarget;
				if (std::abs(rBar.flScroll - rBar.flScrollTarget) < .5f)
				{
					rBar.flScroll = rBar.flScrollTarget;
					rBar.flScrollTarget = -1.f;
				}
			}
			rBar.flScroll = std::clamp(rBar.flScroll, 0.f, flMax);
		}

		void DrawTabScrollbar(stContext& g, stWindow* pWindow, stTabBar& rBar, bool bVertical)
		{
			const stStyle& rStyle = g.stCurrentStyle;
			const float flThickness = TabScrollbarThickness(rStyle);
			const rect& rcView = rBar.rcView;
			const rect rcTrack = bVertical ? rect{ rcView.max.x - flThickness - 1.f, rcView.min.y, rcView.max.x - 1.f, rcView.max.y } : rect{ rcView.min.x, rcView.max.y + kTabScrollbarGap, rcView.max.x, rcView.max.y + kTabScrollbarGap + flThickness };
			const float flTrackLength = bVertical ? rcTrack.Height() : rcTrack.Width();
			const float flMax = TabScrollMax(rBar);
			if (flTrackLength <= 0.f || flMax <= 0.f)
				return;
			const float flThumbLength = std::max(flTrackLength * rBar.flViewExtent / rBar.flContentExtent, 20.f);
			const float flTravel = std::max(flTrackLength - flThumbLength, 1.f);
			const float flThumbPos = flTravel * (rBar.flScroll / flMax);
			const rect rcThumb = bVertical ? rect{ rcTrack.min.x, rcTrack.min.y + flThumbPos, rcTrack.max.x, rcTrack.min.y + flThumbPos + flThumbLength } : rect{ rcTrack.min.x + flThumbPos, rcTrack.min.y, rcTrack.min.x + flThumbPos + flThumbLength, rcTrack.max.y };

			const vec2 vecMouse = g.clInputState.GetMousePosition();
			const ID uScrollID = HashString("#tabscroll", rBar.uID);
			bool bHovered = false;
			bool bHeld = false;
			ButtonBehavior(rcTrack.Expanded(2.f), uScrollID, &bHovered, &bHeld, EItemFlags::NoNav | EItemFlags::PressOnClick);
			if (bHeld)
			{
				const float flMouse = bVertical ? vecMouse.y : vecMouse.x;
				if (g.bActiveIDJustActivated)
				{
					const float flOffset = bVertical ? vecMouse.y - rcThumb.min.y : vecMouse.x - rcThumb.min.x;
					g.vecActiveClickOffset.x = rcThumb.Contains(vecMouse) ? flOffset : flThumbLength * .5f;
				}
				const float flTrackMin = bVertical ? rcTrack.min.y : rcTrack.min.x;
				rBar.flScroll = std::clamp((flMouse - g.vecActiveClickOffset.x - flTrackMin) / flTravel, 0.f, 1.f) * flMax;
				rBar.flScrollTarget = -1.f;
			}
			const float flHover = AnimateBool(uScrollID, bHovered || bHeld);
			clDrawList& rList = pWindow->drawList;
			rList.AddRectFilled(rcTrack.min, rcTrack.max, WithGlobalAlpha(rStyle.Get(EColor::Background).MultiplyAlpha(.5f * flHover)), flThickness * .5f);
			rList.AddRectFilled(rcThumb.min, rcThumb.max, WithGlobalAlpha(rStyle.Get(EColor::Border).Lerp(rStyle.Get(EColor::Accent), flHover * .7f)), flThickness * .5f);
		}
	}

	bool BeginTabBar(std::string_view strID, ETabBarFlags eFlags)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return false;
		const stStyle& rStyle = g.stCurrentStyle;
		const ID uID = pWindow->GetID(strID);
		stTabBar& rBar = g.mapTabBars[uID];
		rBar.uID = uID;
		rBar.eFlags = eFlags;
		rBar.pWindow = pWindow;
		if (rBar.iLastFrame != g.iFrame)
		{
			rBar.aTabsPrevious = std::move(rBar.aTabs);
			rBar.aTabs.clear();
		}
		rBar.iLastFrame = g.iFrame;
		if (!pWindow->uFirstTabBarID && !HasFlag(eFlags, ETabBarFlags::NoGamepadCycle))
			pWindow->uFirstTabBarID = uID;
		if (rBar.uNextSelected)
		{
			rBar.uSelected = rBar.uNextSelected;
			rBar.uNextSelected = 0;
		}
		if (rBar.uSelected && !rBar.aTabsPrevious.empty() && std::ranges::find(rBar.aTabsPrevious, rBar.uSelected) == rBar.aTabsPrevious.end())
			rBar.uSelected = 0;
		rBar.bSubmittedSelected = false;

		stRegion& rRegion = pWindow->Region();
		if (rRegion.flCurrLineHeight > 0.f)
			NewLine();
		const vec2 vecStart = rRegion.vecCursor;
		const bool bVertical = HasFlag(eFlags, ETabBarFlags::Vertical);
		rBar.flViewExtent = bVertical ? std::max(rRegion.rcContent.max.y - vecStart.y, 0.f) : GetContentRegionAvail().x;
		UpdateTabScroll(g, pWindow, rBar, bVertical);
		if (!bVertical)
		{
			const float flWidth = GetContentRegionAvail().x;
			const float flExtra = rBar.bScrollable ? TabScrollbarThickness(rStyle) + kTabScrollbarGap + 1.f : 0.f;
			rBar.rcBar = { vecStart, vecStart + vec2{ flWidth, rStyle.flTabHeight } };
			rBar.rcView = rBar.rcBar;
			rBar.vecBarCursor = vecStart - vec2{ std::floor(rBar.flScroll), 0.f };
			pWindow->drawList.AddRectFilled({ rBar.rcBar.min.x, rBar.rcBar.max.y - 1.f }, rBar.rcBar.max, WithGlobalAlpha(rStyle.Get(EColor::Border)));
			ItemSize(rBar.rcBar.Size() + vec2{ 0.f, flExtra });
			Spacing();
		}
		else
		{
			rBar.rcBar = { vecStart, vecStart + vec2{ rStyle.flSidebarWidth, 0.f } };
			rBar.rcView = { vecStart, vecStart + vec2{ rStyle.flSidebarWidth, rBar.flViewExtent } };
			rBar.vecBarCursor = vecStart - vec2{ 0.f, std::floor(rBar.flScroll) };
			rBar.flSavedLineStartX = rRegion.flLineStartX;
			rBar.flContentStartY = vecStart.y;
			rRegion.flLineStartX = rBar.rcBar.max.x + rStyle.vecItemSpacing.x * 2.f - rRegion.flIndent;
			rRegion.vecCursor = { rBar.rcBar.max.x + rStyle.vecItemSpacing.x * 2.f, vecStart.y };
			rRegion.flCurrLineHeight = 0.f;
		}
		g.aTabBarStack.push_back(&rBar);
		return true;
	}

	void EndTabBar()
	{
		stContext& g = Ctx();
		if (g.aTabBarStack.empty())
			return;
		stTabBar& rBar = *g.aTabBarStack.back();
		g.aTabBarStack.pop_back();
		stWindow* pWindow = CurrentWindow();
		const stStyle& rStyle = g.stCurrentStyle;

		if (!rBar.bSubmittedSelected && !rBar.aTabs.empty() && std::ranges::find(rBar.aTabs, rBar.uSelected) == rBar.aTabs.end())
			rBar.uSelected = rBar.aTabs.front();

		const bool bVertical = HasFlag(rBar.eFlags, ETabBarFlags::Vertical);
		rBar.flContentExtent = std::max(bVertical ? rBar.vecBarCursor.y + std::floor(rBar.flScroll) - rBar.rcBar.min.y - 2.f : rBar.vecBarCursor.x + std::floor(rBar.flScroll) - rBar.rcBar.min.x - 2.f, 0.f);
		if (rBar.bScrollable && !g.uWheelRegionNext && g.pHoveredWindow == pWindow && rBar.rcView.Contains(g.clInputState.GetMousePosition()))
		{
			g.uWheelRegionNext = rBar.uID;
			g.pWheelWindowNext = pWindow;
		}
		if (rBar.bScrollable && !pWindow->bSkipItems)
			DrawTabScrollbar(g, pWindow, rBar, bVertical);
		if (rBar.bSubmittedSelected)
		{
			const rect& rcSelected = rBar.rcSelected;
			if (!rBar.bIndicatorInitialized)
			{
				rBar.flIndicatorX = rcSelected.min.x - rBar.rcBar.min.x;
				rBar.flIndicatorWidth = rcSelected.Width();
				rBar.flIndicatorY = rcSelected.min.y - rBar.rcBar.min.y;
				rBar.flIndicatorHeight = rcSelected.Height();
				rBar.bIndicatorInitialized = true;
			}
			const float flSpeed = rStyle.bAnimations ? 20.f : 1000.f;
			rBar.flIndicatorX = ApproachExp(rBar.flIndicatorX, rcSelected.min.x - rBar.rcBar.min.x, flSpeed, g.flDeltaTime);
			rBar.flIndicatorWidth = ApproachExp(rBar.flIndicatorWidth, rcSelected.Width(), flSpeed, g.flDeltaTime);
			rBar.flIndicatorY = ApproachExp(rBar.flIndicatorY, rcSelected.min.y - rBar.rcBar.min.y, flSpeed, g.flDeltaTime);
			rBar.flIndicatorHeight = ApproachExp(rBar.flIndicatorHeight, rcSelected.Height(), flSpeed, g.flDeltaTime);
		}

		clDrawList& rList = pWindow->drawList;
		const color colAccent = WithGlobalAlpha(rStyle.Get(EColor::Accent));
		if (!bVertical)
		{
			if (rBar.bSubmittedSelected)
			{
				const float flX = rBar.rcBar.min.x + rBar.flIndicatorX;
				rList.PushClipRect(rBar.rcView);
				rList.AddRectFilled({ flX, rBar.rcBar.max.y - 2.f }, { flX + rBar.flIndicatorWidth, rBar.rcBar.max.y }, colAccent);
				rList.PopClipRect();
			}
			return;
		}

		stRegion& rRegion = pWindow->Region();
		const float flContentBottom = rRegion.flCurrLineHeight > 0.f ? rRegion.vecCursor.y + rRegion.flCurrLineHeight : rRegion.vecCursor.y - rStyle.vecItemSpacing.y;
		const float flBarBottom = rBar.bScrollable ? rBar.rcView.max.y : rBar.vecBarCursor.y;
		const float flBottom = std::max({ flBarBottom, flContentBottom, rRegion.vecCursorMax.y });
		const float flLineX = rBar.rcBar.max.x + rStyle.vecItemSpacing.x - 1.f;
		rList.AddRectFilled({ flLineX, rBar.rcBar.min.y }, { flLineX + 1.f, flBottom }, WithGlobalAlpha(rStyle.Get(EColor::Border)));
		if (rBar.bSubmittedSelected)
		{
			const float flY = rBar.rcBar.min.y + rBar.flIndicatorY;
			rList.PushClipRect(rBar.rcView);
			rList.AddRectFilled({ rBar.rcBar.min.x, flY }, { rBar.rcBar.min.x + 2.f, flY + rBar.flIndicatorHeight }, colAccent);
			rList.PopClipRect();
		}
		rRegion.flLineStartX = rBar.flSavedLineStartX;
		rRegion.vecCursor = { rRegion.flLineStartX + rRegion.flIndent, flBottom + rStyle.vecItemSpacing.y };
		rRegion.vecCursorMax.y = std::max(rRegion.vecCursorMax.y, flBottom);
		rRegion.vecCursorMax.x = std::max(rRegion.vecCursorMax.x, flLineX);
		rRegion.flCurrLineHeight = 0.f;
	}

	bool BeginTabItem(std::string_view strLabel, bool* pOpen)
	{
		stContext& g = Ctx();
		if (g.aTabBarStack.empty())
			return false;
		stTabBar& rBar = *g.aTabBarStack.back();
		stWindow* pWindow = CurrentWindow();
		const stStyle& rStyle = g.stCurrentStyle;
		if (pOpen && !*pOpen)
			return false;

		const ID uID = HashString(strLabel, rBar.uID);
		rBar.aTabs.push_back(uID);
		if (rBar.uSelected == 0)
			rBar.uSelected = uID;
		const bool bVertical = HasFlag(rBar.eFlags, ETabBarFlags::Vertical);

		const std::string_view strPrefix = rStyle.bConsolePrefixes ? "./" : "";
		const float flPrefix = MeasureText(strPrefix).x;
		const vec2 vecText = MeasureText(VisibleLabel(strLabel));
		const float flClose = pOpen ? MeasureText(" x").x + 4.f : 0.f;
		rect rcTab;
		if (!bVertical)
		{
			float flWidth = flPrefix + vecText.x + flClose + rStyle.vecFramePadding.x * 2.f + 4.f;
			if (HasFlag(rBar.eFlags, ETabBarFlags::Stretch) && !rBar.aTabsPrevious.empty())
				flWidth = std::floor(rBar.rcBar.Width() / static_cast<float>(rBar.aTabsPrevious.size())) - 2.f;
			rcTab = { rBar.vecBarCursor, rBar.vecBarCursor + vec2{ flWidth, rStyle.flTabHeight } };
			rBar.vecBarCursor.x += flWidth + 2.f;
		}
		else
		{
			rcTab = { rBar.vecBarCursor, { rBar.rcBar.max.x - (rBar.bScrollable ? TabScrollbarThickness(rStyle) + 3.f : 0.f), rBar.vecBarCursor.y + rStyle.flTabHeight } };
			rBar.vecBarCursor.y += rStyle.flTabHeight + 2.f;
		}

		const bool bScrolled = rBar.bScrollable;
		const rect& rcView = rBar.rcView;
		const bool bVisibleTab = !bScrolled || (rcTab.max.x > rcView.min.x && rcTab.min.x < rcView.max.x && rcTab.max.y > rcView.min.y && rcTab.min.y < rcView.max.y);
		const rect rcHit = bScrolled ? rcTab.Clipped(rcView) : rcTab;
		stLastItem stSavedLast = pWindow->stLast;
		bool bHovered = false;
		bool bPressed = false;
		if (bVisibleTab)
		{
			ItemAdd(rcHit, uID);
			bPressed = ButtonBehavior(rcHit, uID, &bHovered, nullptr, EItemFlags::PressOnClick);
		}

		bool bCloseHovered = false;
		rect rcClose;
		if (pOpen && bVisibleTab)
		{
			const float flCloseWidth = MeasureText("x").x + 6.f;
			rcClose = { { rcTab.max.x - flCloseWidth - rStyle.vecFramePadding.x * .5f, rcTab.min.y + 4.f }, { rcTab.max.x - rStyle.vecFramePadding.x * .5f, rcTab.max.y - 4.f } };
			const ID uCloseID = HashString("#close", uID);
			if (ButtonBehavior(rcClose, uCloseID, &bCloseHovered, nullptr, EItemFlags::NoNav))
			{
				*pOpen = false;
				bPressed = false;
			}
			if (bCloseHovered)
				bHovered = false;
		}

		if (bPressed)
			rBar.uSelected = uID;
		const bool bNowSelected = rBar.uSelected == uID && (!pOpen || *pOpen);
		if (bScrolled && bNowSelected && rBar.uRevealed != uID)
		{
			rBar.uRevealed = uID;
			RevealTab(rBar, bVertical ? rcTab.min.y : rcTab.min.x, bVertical ? rcTab.max.y : rcTab.max.x, bVertical ? rcView.min.y : rcView.min.x);
		}
		clDrawList& rList = pWindow->drawList;
		if (bVisibleTab)
		{
		const float flHover = AnimateBool(uID, bHovered || (g.uNavID == uID && g.bNavVisible));
		if (bScrolled)
			rList.PushClipRect(rcView);

		if (bVertical)
		{
			if (bNowSelected)
				rList.AddRectFilled(rcTab.min, rcTab.max, WithGlobalAlpha(rStyle.Get(EColor::AccentSoft)), rStyle.flRoundingSmall);
			if (flHover > .01f)
				rList.AddRectFilled(rcTab.min, rcTab.max, WithGlobalAlpha(rStyle.Get(EColor::Hover).MultiplyAlpha(flHover)), rStyle.flRoundingSmall);
		}
		else if (flHover > .01f && !bNowSelected)
			rList.AddRectFilled(rcTab.min, { rcTab.max.x, rcTab.max.y - 1.f }, WithGlobalAlpha(rStyle.Get(EColor::Hover).MultiplyAlpha(flHover * .6f)), rStyle.flRoundingSmall, ECorner::Top);

		const color colText = bNowSelected ? rStyle.Get(EColor::Accent) : rStyle.Get(EColor::TextMuted).Lerp(rStyle.Get(EColor::Accent), flHover);
		const float flTextX = bVertical ? rcTab.min.x + rStyle.vecFramePadding.x + 4.f : (HasFlag(rBar.eFlags, ETabBarFlags::Stretch) ? std::floor(rcTab.Center().x - (flPrefix + vecText.x + flClose) * .5f) : rcTab.min.x + rStyle.vecFramePadding.x + 2.f);
		const float flTextY = std::floor(rcTab.Center().y - vecText.y * .5f);
		rList.PushClipRect(rcTab);
		rList.AddText({ flTextX, flTextY }, WithGlobalAlpha(rStyle.Get(EColor::TextDisabled)), strPrefix);
		rList.AddText({ flTextX + flPrefix, flTextY }, WithGlobalAlpha(colText), EllipsizeText(VisibleLabel(strLabel), std::max(rcTab.max.x - (flTextX + flPrefix) - flClose - rStyle.vecFramePadding.x, 10.f)));
		if (pOpen && (bHovered || bCloseHovered || bNowSelected))
			rList.AddText({ rcClose.min.x + 3.f, flTextY }, WithGlobalAlpha(bCloseHovered ? rStyle.Get(EColor::Danger) : rStyle.Get(EColor::TextDisabled)), "x");
		rList.PopClipRect();
		if (bScrolled)
			rList.PopClipRect();
		RenderNavHighlight(rcTab, uID);
		}

		pWindow->stLast = stSavedLast;
		pWindow->stLast.uID = uID;
		pWindow->stLast.rcItem = rcTab;
		pWindow->stLast.bHovered = bHovered;

		if (!bNowSelected)
			return false;
		rBar.bSubmittedSelected = true;
		rBar.rcSelected = rcTab;
		pWindow->aIDStack.push_back(uID);
		return true;
	}

	void EndTabItem()
	{
		PopID();
	}

	void SetTabItemSelected(std::string_view strTabBarID, std::string_view strLabel)
	{
		stContext& g = Ctx();
		const ID uBarID = CurrentWindow() ? CurrentWindow()->GetID(strTabBarID) : HashString(strTabBarID);
		stTabBar& rBar = g.mapTabBars[uBarID];
		rBar.uNextSelected = HashString(strLabel, uBarID);
	}

	void TabBarSection(std::string_view strLabel)
	{
		stContext& g = Ctx();
		if (g.aTabBarStack.empty())
			return;
		stTabBar& rBar = *g.aTabBarStack.back();
		if (!HasFlag(rBar.eFlags, ETabBarFlags::Vertical))
			return;
		stWindow* pWindow = CurrentWindow();
		const stStyle& rStyle = g.stCurrentStyle;
		const float flGap = rBar.vecBarCursor.y > rBar.rcBar.min.y - std::floor(rBar.flScroll) ? std::round(rStyle.vecItemSpacing.y * 1.5f) : 0.f;
		const vec2 vecText{ rBar.vecBarCursor.x + rStyle.vecFramePadding.x + 4.f, rBar.vecBarCursor.y + flGap };
		const std::string_view strPrefix = rStyle.bConsolePrefixes ? "## " : "";
		const bool bVisibleSection = !rBar.bScrollable || (vecText.y + GetTextLineHeight() > rBar.rcView.min.y && vecText.y < rBar.rcView.max.y);
		if (bVisibleSection)
		{
			if (rBar.bScrollable)
				pWindow->drawList.PushClipRect(rBar.rcView);
			pWindow->drawList.AddText(vecText, WithGlobalAlpha(rStyle.Get(EColor::TextDisabled)), strPrefix);
			pWindow->drawList.AddText(vecText + vec2{ MeasureText(strPrefix).x, 0.f }, WithGlobalAlpha(rStyle.Get(EColor::TextMuted)), EllipsizeText(VisibleLabel(strLabel), std::max(rBar.rcBar.max.x - vecText.x - MeasureText(strPrefix).x - rStyle.vecFramePadding.x, 10.f)));
			if (rBar.bScrollable)
				pWindow->drawList.PopClipRect();
		}
		rBar.vecBarCursor.y += flGap + GetTextLineHeight() + std::round(rStyle.vecItemSpacing.y * .5f);
	}

	void UpdateTabBarGamepad(int iDirection)
	{
		stContext& g = Ctx();
		stWindow* pWindow = g.pNavWindow;
		if (!pWindow)
			return;
		const auto itBar = g.mapTabBars.find(pWindow->uFirstTabBarIDPrevious);
		if (itBar == g.mapTabBars.end())
			return;
		stTabBar& rBar = itBar->second;
		const std::vector<ID>& aTabs = rBar.aTabs.empty() ? rBar.aTabsPrevious : rBar.aTabs;
		if (aTabs.empty())
			return;
		const auto itCurrent = std::ranges::find(aTabs, rBar.uSelected);
		const std::size_t uIndex = itCurrent == aTabs.end() ? 0 : static_cast<std::size_t>(itCurrent - aTabs.begin());
		const std::size_t uNext = (uIndex + aTabs.size() + static_cast<std::size_t>(iDirection + static_cast<int>(aTabs.size()))) % aTabs.size();
		rBar.uNextSelected = aTabs[uNext];
		g.uNavID = aTabs[uNext];
		g.bNavVisible = true;
	}

	bool BeginMenuBar()
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems || !HasFlag(pWindow->eFlags, EWindowFlags::MenuBar))
			return false;
		const stStyle& rStyle = g.stCurrentStyle;
		pWindow->bInMenuBar = true;
		pWindow->vecMenuBarCursor = { pWindow->rcMenuBar.min.x + rStyle.vecWindowPadding.x * .5f, std::floor(pWindow->rcMenuBar.Center().y - FrameHeight() * .5f) };
		pWindow->drawList.PushClipRect(pWindow->rcMenuBar, false);
		return true;
	}

	void EndMenuBar()
	{
		stWindow* pWindow = CurrentWindow();
		if (!pWindow->bInMenuBar)
			return;
		pWindow->drawList.PopClipRect();
		pWindow->bInMenuBar = false;
	}

	bool BeginMenu(std::string_view strLabel, bool bEnabled)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return false;
		const stStyle& rStyle = g.stCurrentStyle;
		const ID uID = pWindow->GetID(strLabel);
		const ID uPopupID = HashString("#menu", uID);
		const vec2 vecText = MeasureText(VisibleLabel(strLabel));
		const bool bInBar = pWindow->bInMenuBar;

		rect rcItem;
		if (bInBar)
		{
			rcItem = { pWindow->vecMenuBarCursor, pWindow->vecMenuBarCursor + vec2{ vecText.x + rStyle.vecFramePadding.x * 2.f, FrameHeight() } };
			pWindow->vecMenuBarCursor.x += rcItem.Width() + 2.f;
		}
		else
		{
			stRegion& rRegion = pWindow->Region();
			const float flWidth = std::max(GetContentRegionAvail().x, vecText.x + rStyle.vecFramePadding.x * 2.f + 24.f);
			rcItem = { rRegion.vecCursor, rRegion.vecCursor + vec2{ flWidth, FrameHeight() } };
			ItemSize(rcItem.Size(), rStyle.vecFramePadding.y);
		}
		if (!bEnabled)
			BeginDisabled();
		ItemAdd(rcItem, uID);
		bool bHovered = false;
		const bool bPressed = ButtonBehavior(rcItem, uID, &bHovered, nullptr, EItemFlags::PressOnClick);
		bool bOpen = IsPopupOpenID(uPopupID);

		if (bInBar)
		{
			if (bPressed)
			{
				if (bOpen)
					ClosePopupToLevel(g.aBeginPopupStack.size());
				else
					OpenPopupEx(uPopupID, rcItem, false, true);
			}
			else if (bHovered && pWindow->uMenuBarOpenMenu && pWindow->uMenuBarOpenMenu != uID && IsPopupOpenID(HashString("#menu", pWindow->uMenuBarOpenMenu)))
				OpenPopupEx(uPopupID, rcItem, false, true);
		}
		else if ((bHovered && !bOpen) || bPressed)
			OpenPopupEx(uPopupID, rcItem, false, true);
		bOpen = IsPopupOpenID(uPopupID);
		if (bInBar)
		{
			if (bOpen)
				pWindow->uMenuBarOpenMenu = uID;
			else if (pWindow->uMenuBarOpenMenu == uID)
				pWindow->uMenuBarOpenMenu = 0;
		}

		const float flHover = AnimateBool(uID, bHovered || bOpen || (g.uNavID == uID && g.bNavVisible));
		clDrawList& rList = pWindow->drawList;
		if (flHover > .01f)
			rList.AddRectFilled(rcItem.min, rcItem.max, WithGlobalAlpha(rStyle.Get(EColor::Hover).MultiplyAlpha(flHover)), rStyle.flRoundingSmall);
		RenderTextClipped({ rcItem.min + vec2{ rStyle.vecFramePadding.x, 0.f }, rcItem.max }, strLabel, (bOpen ? rStyle.Get(EColor::Accent) : rStyle.Get(EColor::TextMuted).Lerp(rStyle.Get(EColor::Accent), flHover)), { 0.f, .5f });
		if (!bInBar)
		{
			const vec2 vecArrow{ rcItem.max.x - rStyle.vecFramePadding.x - 3.f, rcItem.Center().y };
			rList.AddTriangleFilled(vecArrow + vec2{ -3.f, -4.f }, vecArrow + vec2{ 2.f, 0.f }, vecArrow + vec2{ -3.f, 4.f }, WithGlobalAlpha(rStyle.Get(EColor::TextDisabled)));
		}
		RenderNavHighlight(rcItem, uID);
		if (!bEnabled)
			EndDisabled();

		if (!bOpen)
			return false;
		if (!bInBar)
			SetNextWindowPos({ pWindow->vecPosition.x + pWindow->vecSize.x - 2.f, rcItem.min.y - 6.f }, ECondition::Always);
		return BeginPopupEx(uPopupID, EWindowFlags::None, {});
	}

	void EndMenu()
	{
		EndPopup();
	}

	bool MenuItem(std::string_view strLabel, std::string_view strShortcut, bool* pSelected, bool bEnabled)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return false;
		const stStyle& rStyle = g.stCurrentStyle;
		const ID uID = pWindow->GetID(strLabel);
		stRegion& rRegion = pWindow->Region();
		const float flCheck = pSelected ? MeasureText("[x] ").x : 0.f;
		const vec2 vecText = MeasureText(VisibleLabel(strLabel));
		const float flShortcut = strShortcut.empty() ? 0.f : MeasureText(strShortcut).x + 24.f;
		const float flWidth = std::max(GetContentRegionAvail().x, flCheck + vecText.x + flShortcut + rStyle.vecFramePadding.x * 2.f);
		const rect rcItem{ rRegion.vecCursor, rRegion.vecCursor + vec2{ flWidth, FrameHeight() } };
		ItemSize(rcItem.Size(), rStyle.vecFramePadding.y);
		if (!bEnabled)
			BeginDisabled();
		const bool bVisible = ItemAdd(rcItem, uID);
		bool bHovered = false;
		const bool bPressed = bVisible && ButtonBehavior(rcItem, uID, &bHovered, nullptr);
		if (bVisible)
		{
			const float flHover = AnimateBool(uID, bHovered || (g.uNavID == uID && g.bNavVisible));
			clDrawList& rList = pWindow->drawList;
			if (flHover > .01f)
				rList.AddRectFilled(rcItem.min, rcItem.max, WithGlobalAlpha(rStyle.Get(EColor::Hover).MultiplyAlpha(flHover)), rStyle.flRoundingSmall);
			const float flTextY = std::floor(rcItem.Center().y - vecText.y * .5f);
			float flX = rcItem.min.x + rStyle.vecFramePadding.x;
			if (pSelected)
			{
				rList.AddText({ flX, flTextY }, WithGlobalAlpha(rStyle.Get(EColor::TextDisabled)), "[ ]");
				if (*pSelected)
					rList.AddText({ flX + MeasureText("[").x, flTextY }, WithGlobalAlpha(rStyle.Get(EColor::Accent)), "x");
				flX += flCheck;
			}
			rList.AddText({ flX, flTextY }, WithGlobalAlpha(rStyle.Get(EColor::Text).Lerp(rStyle.Get(EColor::Accent), flHover)), VisibleLabel(strLabel));
			if (!strShortcut.empty())
				rList.AddText({ rcItem.max.x - rStyle.vecFramePadding.x - MeasureText(strShortcut).x, flTextY }, WithGlobalAlpha(rStyle.Get(EColor::TextDisabled)), strShortcut);
		}
		if (!bEnabled)
			EndDisabled();
		if (bPressed)
		{
			if (pSelected)
				*pSelected = !*pSelected;
			ClosePopupToLevel(0);
			if (g.pFocusedWindow && HasFlag(g.pFocusedWindow->eFlags, EWindowFlags::Popup))
				g.pFocusedWindow = nullptr;
		}
		return bPressed;
	}
}
