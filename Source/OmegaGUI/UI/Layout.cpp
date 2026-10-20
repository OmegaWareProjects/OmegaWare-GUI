#include "OmegaGUI/UI/Internal.hpp"

namespace OmegaGUI
{
	float FrameHeight()
	{
		const stContext& g = Ctx();
		return std::floor(g.stCurrentStyle.flFontSize + g.stCurrentStyle.vecFramePadding.y * 2.f);
	}

	float GetFrameHeight() { return FrameHeight(); }
	float GetTextLineHeight() { return Ctx().stShared.pFont ? std::floor(Ctx().stShared.pFont->GetLineHeight() * Ctx().stShared.flFontSize / Ctx().stShared.pFont->GetSize()) : Ctx().stCurrentStyle.flFontSize; }

	vec2 MeasureText(std::string_view strText, float flWrapWidth)
	{
		const stContext& g = Ctx();
		if (!g.stShared.pFont)
			return {};
		return g.stShared.pFont->MeasureText(strText, g.stShared.flFontSize, flWrapWidth);
	}

	vec2 CalcTextSize(std::string_view strText, float flWrapWidth) { return MeasureText(strText, flWrapWidth); }

	void RenderText(const vec2& vecPosition, std::string_view strText, color col)
	{
		CurrentWindow()->drawList.AddText(vecPosition, WithGlobalAlpha(col), VisibleLabel(strText));
	}

	void RenderTextClipped(const rect& rcBounds, std::string_view strText, color col, const vec2& vecAlign, const rect* pClip)
	{
		stWindow* pWindow = CurrentWindow();
		const std::string_view strVisible = VisibleLabel(strText);
		const vec2 vecSize = MeasureText(strVisible);
		vec2 vecPosition = rcBounds.min;
		vecPosition.x += std::max(0.f, (rcBounds.Width() - vecSize.x) * vecAlign.x);
		vecPosition.y += std::max(0.f, (rcBounds.Height() - vecSize.y) * vecAlign.y);
		const bool bNeedsClip = vecSize.x > rcBounds.Width() || pClip;
		if (bNeedsClip)
			pWindow->drawList.PushClipRect(pClip ? *pClip : rcBounds);
		pWindow->drawList.AddText(vecPosition.Floor(), WithGlobalAlpha(col), strVisible);
		if (bNeedsClip)
			pWindow->drawList.PopClipRect();
	}

	void RenderFrame(const rect& rcFrame, color colFill, bool bBorder, color colBorder, float flRounding)
	{
		clDrawList& rList = CurrentWindow()->drawList;
		rList.AddRectFilled(rcFrame.min, rcFrame.max, WithGlobalAlpha(colFill), flRounding);
		if (bBorder)
			rList.AddRect(rcFrame.min, rcFrame.max, WithGlobalAlpha(colBorder), flRounding);
	}

	void RenderNavHighlight(const rect& rcItem, ID uID, float flRounding)
	{
		stContext& g = Ctx();
		if (!g.bNavVisible || g.uNavID != uID || g.pNavWindow != CurrentWindow())
			return;
		const stStyle& rStyle = g.stCurrentStyle;
		if (flRounding < 0.f)
			flRounding = rStyle.flRoundingSmall;
		clDrawList& rList = CurrentWindow()->drawList;
		const rect rcOutline = rcItem.Expanded(3.f);
		rList.AddGlowRect(rcOutline.min, rcOutline.max, WithGlobalAlpha(rStyle.Get(EColor::AccentGlow)), 4.f, flRounding + 3.f);
		rList.AddRect(rcOutline.min, rcOutline.max, WithGlobalAlpha(rStyle.Get(EColor::Accent)), flRounding + 3.f, 1.f);
	}

	bool IsDisabled() { return Ctx().iDisabledDepth > 0; }

	void BeginDisabled(bool bDisabled)
	{
		Ctx().aDisabledStack.push_back(bDisabled);
		if (bDisabled)
			++Ctx().iDisabledDepth;
	}

	void EndDisabled()
	{
		stContext& g = Ctx();
		if (g.aDisabledStack.empty())
			return;
		if (g.aDisabledStack.back() && g.iDisabledDepth > 0)
			--g.iDisabledDepth;
		g.aDisabledStack.pop_back();
	}

	bool IsClippedEx(const rect& rcItem)
	{
		return !rcItem.Overlaps(CurrentWindow()->drawList.GetClipRect());
	}

	void SetActiveID(ID uID, stWindow* pWindow)
	{
		stContext& g = Ctx();
		g.uActiveID = uID;
		g.pActiveWindow = pWindow;
		g.bActiveIDJustActivated = true;
		g.bActiveIDAlive = true;
	}

	void ClearActiveID()
	{
		stContext& g = Ctx();
		g.uActiveID = 0;
		g.pActiveWindow = nullptr;
	}

	void KeepAliveID(ID uID)
	{
		if (Ctx().uActiveID == uID)
			Ctx().bActiveIDAlive = true;
	}

	bool ItemAdd(const rect& rcItem, ID uID, EItemFlags eFlags)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		stRegion& rRegion = pWindow->Region();
		pWindow->stLast = {};
		pWindow->stLast.uID = uID;
		pWindow->stLast.rcItem = rcItem;

		if (uID && !HasFlag(eFlags, EItemFlags::NoNav) && !IsDisabled() && !HasFlag(pWindow->eFlags, EWindowFlags::NoNav) && !pWindow->bSkipItems)
		{
			pWindow->aNavItems.push_back({ uID, rcItem, rRegion.uID, rRegion.rcClip, eFlags });
			if (g.bNavRequestFocusHere)
			{
				g.bNavRequestFocusHere = false;
				g.uNavID = uID;
				g.pNavWindow = pWindow;
			}
		}
		pWindow->stLast.bVisible = !IsClippedEx(rcItem);
		return pWindow->stLast.bVisible;
	}

	void ItemSize(const vec2& vecSize, float flTextOffset)
	{
		const stContext& g = Ctx();
		stRegion& rRegion = CurrentWindow()->Region();
		const float flLineHeight = std::max(rRegion.flCurrLineHeight, vecSize.y);
		rRegion.vecCursorPrevLine = { rRegion.vecCursor.x + vecSize.x, rRegion.vecCursor.y };
		rRegion.vecCursorMax = Max(rRegion.vecCursorMax, { rRegion.vecCursorPrevLine.x, rRegion.vecCursor.y + flLineHeight });
		rRegion.vecCursor = { rRegion.flLineStartX + rRegion.flIndent, std::floor(rRegion.vecCursor.y + flLineHeight + g.stCurrentStyle.vecItemSpacing.y) };
		rRegion.flPrevLineHeight = flLineHeight;
		rRegion.flPrevTextOffset = std::max(rRegion.flTextOffset, flTextOffset);
		rRegion.flCurrLineHeight = 0.f;
		rRegion.flTextOffset = 0.f;
	}

	bool ItemHoverable(const rect& rcItem, ID uID, EItemFlags eFlags)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (!g.bVisible || pWindow->bSkipItems || HasFlag(pWindow->eFlags, EWindowFlags::NoInputs))
			return false;
		if (g.pHoveredWindow != pWindow || pWindow->bResizeHovered)
			return false;
		if (g.uActiveID && g.uActiveID != uID && !HasFlag(eFlags, EItemFlags::AllowWhenBlocked))
			return false;
		if (g.pMovingWindow || g.pResizingWindow)
			return false;
		if (!rcItem.Clipped(pWindow->drawList.GetClipRect()).Contains(g.clInputState.GetMousePosition()))
			return false;
		if (IsDisabled())
			return false;
		g.uHoveredID = uID;
		return true;
	}

	bool ButtonBehavior(const rect& rcItem, ID uID, bool* pHovered, bool* pHeld, EItemFlags eFlags)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		clInput& rInput = g.clInputState;
		const EMouseButton eButton = HasFlag(eFlags, EItemFlags::RightClick) ? EMouseButton::Right : EMouseButton::Left;

		const bool bHovered = ItemHoverable(rcItem, uID, eFlags);
		bool bPressed = false;
		bool bHeld = false;

		if (bHovered && rInput.IsMouseClicked(eButton))
		{
			SetActiveID(uID, pWindow);
			g.vecActiveClickOffset = rInput.GetMousePosition() - rcItem.min;
			if (HasFlag(eFlags, EItemFlags::PressOnClick))
				bPressed = true;
			if (!HasFlag(eFlags, EItemFlags::NoNav) && !HasFlag(pWindow->eFlags, EWindowFlags::NoNav))
			{
				g.uNavID = uID;
				g.pNavWindow = pWindow;
			}
			g.bNavVisible = false;
		}

		if (g.uActiveID == uID && g.pActiveWindow == pWindow)
		{
			KeepAliveID(uID);
			if (rInput.IsMouseDown(eButton))
			{
				bHeld = true;
				if (HasFlag(eFlags, EItemFlags::Repeat) && bHovered && !g.bActiveIDJustActivated)
				{
					const KeyCode uKey = eButton == EMouseButton::Right ? 0x02 : 0x01;
					if (rInput.IsKeyPressed(uKey, true) && rInput.GetKeyDownDuration(uKey) > 0.f)
						bPressed = true;
				}
			}
			else if (!g.bActiveIDJustActivated || rInput.IsMouseReleased(eButton))
			{
				if (bHovered && !HasFlag(eFlags, EItemFlags::PressOnClick))
					bPressed = true;
				ClearActiveID();
			}
		}

		if (g.uNavActivateID == uID && g.pNavWindow == pWindow)
		{
			bPressed = true;
			g.mapAnimations[HashString("#navflash", uID)] = 1.f;
		}

		if (pHovered)
			*pHovered = bHovered;
		if (pHeld)
			*pHeld = bHeld;
		pWindow->stLast.bHovered = bHovered;
		pWindow->stLast.bClicked = bPressed;
		return bPressed;
	}

	vec2 GetContentRegionAvail()
	{
		stRegion& rRegion = CurrentWindow()->Region();
		float flMaxX = rRegion.rcContent.max.x;
		if (rRegion.optColumns)
			flMaxX = rRegion.optColumns->flStartX + rRegion.optColumns->flWidth * static_cast<float>(rRegion.optColumns->iCurrent + 1) - (rRegion.optColumns->iCurrent + 1 < rRegion.optColumns->iCount ? Style().vecItemSpacing.x : 0.f);
		const float flMaxY = rRegion.pScroll && rRegion.pScroll->flMaxY > 0.f ? rRegion.vecCursor.y + 1e5f : rRegion.rcContent.max.y;
		return { std::max(flMaxX - rRegion.vecCursor.x, 1.f), std::max(std::min(flMaxY, rRegion.rcContent.max.y) - rRegion.vecCursor.y, 1.f) };
	}

	vec2 GetCursorPos()
	{
		stWindow* pWindow = CurrentWindow();
		return pWindow->Region().vecCursor - pWindow->vecPosition;
	}

	void SetCursorPos(const vec2& vecPosition)
	{
		stWindow* pWindow = CurrentWindow();
		stRegion& rRegion = pWindow->Region();
		rRegion.vecCursor = pWindow->vecPosition + vecPosition;
		rRegion.vecCursorMax = Max(rRegion.vecCursorMax, rRegion.vecCursor);
	}

	vec2 GetCursorScreenPos() { return CurrentWindow()->Region().vecCursor; }

	void SetCursorScreenPos(const vec2& vecPosition)
	{
		stRegion& rRegion = CurrentWindow()->Region();
		rRegion.vecCursor = vecPosition;
		rRegion.vecCursorMax = Max(rRegion.vecCursorMax, vecPosition);
	}

	void SameLine(float flOffsetFromStart, float flSpacing)
	{
		stRegion& rRegion = CurrentWindow()->Region();
		if (flOffsetFromStart != 0.f)
			rRegion.vecCursor.x = rRegion.flLineStartX + flOffsetFromStart;
		else
			rRegion.vecCursor.x = rRegion.vecCursorPrevLine.x + (flSpacing < 0.f ? Style().vecItemSpacing.x : flSpacing);
		rRegion.vecCursor.y = rRegion.vecCursorPrevLine.y;
		rRegion.flCurrLineHeight = rRegion.flPrevLineHeight;
		rRegion.flTextOffset = rRegion.flPrevTextOffset;
	}

	void NewLine()
	{
		stRegion& rRegion = CurrentWindow()->Region();
		if (rRegion.flCurrLineHeight > 0.f)
			ItemSize({ 0.f, 0.f });
		else
			ItemSize({ 0.f, GetTextLineHeight() });
	}

	void Spacing()
	{
		ItemSize({ 0.f, Style().vecItemSpacing.y });
	}

	void Dummy(const vec2& vecSize)
	{
		stRegion& rRegion = CurrentWindow()->Region();
		ItemAdd({ rRegion.vecCursor, rRegion.vecCursor + vecSize }, 0);
		ItemSize(vecSize);
	}

	void Indent(float flWidth)
	{
		stRegion& rRegion = CurrentWindow()->Region();
		const float flAmount = flWidth > 0.f ? flWidth : Style().flIndent;
		rRegion.flIndent += flAmount;
		rRegion.vecCursor.x += flAmount;
	}

	void Unindent(float flWidth)
	{
		stRegion& rRegion = CurrentWindow()->Region();
		const float flAmount = flWidth > 0.f ? flWidth : Style().flIndent;
		rRegion.flIndent -= flAmount;
		rRegion.vecCursor.x -= flAmount;
	}

	void BeginGroup()
	{
		stRegion& rRegion = CurrentWindow()->Region();
		rRegion.aGroups.push_back({ rRegion.vecCursor, rRegion.vecCursorMax, rRegion.flLineStartX, rRegion.flPrevLineHeight, rRegion.flCurrLineHeight });
		rRegion.flLineStartX = rRegion.vecCursor.x - rRegion.flIndent;
		rRegion.vecCursorMax = rRegion.vecCursor;
		rRegion.flCurrLineHeight = 0.f;
	}

	void EndGroup()
	{
		stRegion& rRegion = CurrentWindow()->Region();
		if (rRegion.aGroups.empty())
			return;
		const stGroup stGroupData = rRegion.aGroups.back();
		rRegion.aGroups.pop_back();
		const vec2 vecSize = Max(rRegion.vecCursorMax - stGroupData.vecCursorStart, { 0.f, 0.f });
		rRegion.flLineStartX = stGroupData.flLineStartBackup;
		rRegion.vecCursor = stGroupData.vecCursorStart;
		rRegion.vecCursorMax = Max(stGroupData.vecCursorMaxBackup, rRegion.vecCursorMax);
		rRegion.flCurrLineHeight = stGroupData.flCurrLineHeightBackup;
		ItemSize(vecSize);
		CurrentWindow()->stLast.rcItem = { stGroupData.vecCursorStart, stGroupData.vecCursorStart + vecSize };
		CurrentWindow()->stLast.uID = 0;
	}

	bool BeginSection(std::string_view strLabel)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems)
			return false;
		stRegion& rRegion = pWindow->Region();
		if (rRegion.flCurrLineHeight > 0.f)
			NewLine();

		const stStyle& rStyle = Style();
		const ID uID = pWindow->GetID(strLabel);
		const float flPadding = std::round(rStyle.vecWindowPadding.x * .75f);
		const float flTextHeight = GetTextLineHeight();
		const vec2 vecStart = rRegion.vecCursor + vec2{ 1.f, 0.f };
		const float flRight = rRegion.vecCursor.x + GetContentRegionAvail().x - 1.f;
		const float flTop = vecStart.y + std::floor(flTextHeight * .5f);

		const float flHeight = g.mapAnimations[HashString("#section", uID)];
		if (flHeight > 0.f)
			pWindow->drawList.AddRectFilled({ vecStart.x, flTop }, { flRight, flTop + flHeight }, WithGlobalAlpha(rStyle.Get(EColor::Surface2).MultiplyAlpha(.45f)), rStyle.flRounding);

		pWindow->aSections.push_back({ uID, vecStart, flRight, flPadding, rRegion.rcContent.max.x, rRegion.optColumns });
		rRegion.optColumns.reset();
		rRegion.rcContent.max.x = flRight - flPadding;
		rRegion.vecCursor = { vecStart.x - 1.f, vecStart.y + flTextHeight + std::floor(flPadding * .5f) };
		Indent(flPadding);
		BeginGroup();

		const std::string_view strVisible = VisibleLabel(strLabel);
		const float flLabelWidth = MeasureText(strVisible).x;
		const float flLabelX = vecStart.x + flPadding;
		const color colBorder = WithGlobalAlpha(rStyle.Get(EColor::Border));
		pWindow->drawList.AddRectFilled({ vecStart.x, flTop }, { flLabelX - 6.f, flTop + 1.f }, colBorder);
		pWindow->drawList.AddRectFilled({ std::min(flLabelX + flLabelWidth + 6.f, flRight), flTop }, { flRight, flTop + 1.f }, colBorder);
		pWindow->drawList.AddText({ flLabelX, vecStart.y }, WithGlobalAlpha(rStyle.Get(EColor::TextHeading)), strVisible);
		return true;
	}

	void EndSection()
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->aSections.empty())
			return;
		const stSection stSectionData = pWindow->aSections.back();
		pWindow->aSections.pop_back();
		stRegion& rRegion = pWindow->Region();
		const stStyle& rStyle = Style();

		EndGroup();
		const float flContentBottom = pWindow->stLast.rcItem.max.y;
		Unindent(stSectionData.flPadding);
		rRegion.rcContent.max.x = stSectionData.flSavedContentMaxX;
		rRegion.optColumns = stSectionData.optSavedColumns;

		const float flTop = stSectionData.vecStart.y + std::floor(GetTextLineHeight() * .5f);
		const float flBottom = std::max(flContentBottom, flTop + 1.f) + std::floor(stSectionData.flPadding * .75f);
		g.mapAnimations[HashString("#section", stSectionData.uID)] = flBottom - flTop;

		const color colBorder = WithGlobalAlpha(rStyle.Get(EColor::Border));
		clDrawList& rList = pWindow->drawList;
		rList.AddRectFilled({ stSectionData.vecStart.x, flTop }, { stSectionData.vecStart.x + 1.f, flBottom }, colBorder);
		rList.AddRectFilled({ stSectionData.flRight - 1.f, flTop }, { stSectionData.flRight, flBottom }, colBorder);
		rList.AddRectFilled({ stSectionData.vecStart.x, flBottom - 1.f }, { stSectionData.flRight, flBottom }, colBorder);

		rRegion.vecCursor = { stSectionData.vecStart.x - 1.f, flBottom };
		rRegion.flCurrLineHeight = 0.f;
		ItemSize({ stSectionData.flRight + 1.f - (stSectionData.vecStart.x - 1.f), 0.f });
		pWindow->stLast.rcItem = { stSectionData.vecStart, { stSectionData.flRight, flBottom } };
		pWindow->stLast.uID = stSectionData.uID;
	}

	void BeginColumns(int iCount)
	{
		stRegion& rRegion = CurrentWindow()->Region();
		if (rRegion.flCurrLineHeight > 0.f)
			NewLine();
		stColumns stNew;
		stNew.iCount = std::max(iCount, 1);
		stNew.flStartX = rRegion.vecCursor.x;
		stNew.flStartY = rRegion.vecCursor.y;
		stNew.flWidth = (rRegion.rcContent.max.x - rRegion.vecCursor.x + Style().vecItemSpacing.x) / static_cast<float>(stNew.iCount);
		stNew.flMaxY = rRegion.vecCursor.y;
		stNew.flSavedLineStartX = rRegion.flLineStartX;
		rRegion.optColumns = stNew;
		rRegion.flLineStartX = stNew.flStartX - rRegion.flIndent;
	}

	void NextColumn()
	{
		stRegion& rRegion = CurrentWindow()->Region();
		if (!rRegion.optColumns)
			return;
		stColumns& rColumns = *rRegion.optColumns;
		if (rRegion.flCurrLineHeight > 0.f)
			ItemSize({ 0.f, 0.f });
		rColumns.flMaxY = std::max(rColumns.flMaxY, rRegion.vecCursor.y);
		rColumns.iCurrent = (rColumns.iCurrent + 1) % rColumns.iCount;
		if (rColumns.iCurrent == 0)
			rColumns.flStartY = rColumns.flMaxY;
		const float flX = rColumns.flStartX + rColumns.flWidth * static_cast<float>(rColumns.iCurrent);
		rRegion.flLineStartX = flX - rRegion.flIndent;
		rRegion.vecCursor = { flX, rColumns.flStartY };
		rRegion.flCurrLineHeight = 0.f;
	}

	void EndColumns()
	{
		stRegion& rRegion = CurrentWindow()->Region();
		if (!rRegion.optColumns)
			return;
		if (rRegion.flCurrLineHeight > 0.f)
			ItemSize({ 0.f, 0.f });
		const stColumns stColumnsData = *rRegion.optColumns;
		rRegion.optColumns.reset();
		rRegion.flLineStartX = stColumnsData.flSavedLineStartX;
		rRegion.vecCursor = { rRegion.flLineStartX + rRegion.flIndent, std::max(stColumnsData.flMaxY, rRegion.vecCursor.y) };
		rRegion.vecCursorMax.y = std::max(rRegion.vecCursorMax.y, rRegion.vecCursor.y);
	}

	void PushItemWidth(float flWidth) { CurrentWindow()->aItemWidthStack.push_back(flWidth); }

	void PopItemWidth()
	{
		stWindow* pWindow = CurrentWindow();
		if (!pWindow->aItemWidthStack.empty())
			pWindow->aItemWidthStack.pop_back();
	}

	void SetNextItemWidth(float flWidth) { CurrentWindow()->flNextItemWidth = flWidth; }

	float CalcItemWidth()
	{
		stWindow* pWindow = CurrentWindow();
		const float flAvailable = GetContentRegionAvail().x;
		float flWidth = pWindow->flNextItemWidth;
		if (flWidth == -1.f && !pWindow->aItemWidthStack.empty())
			flWidth = pWindow->aItemWidthStack.back();
		pWindow->flNextItemWidth = -1.f;
		if (flWidth == -1.f)
			return std::max(std::floor(flAvailable * .5f), 60.f);
		if (flWidth < 0.f)
			return std::max(flAvailable + flWidth, 20.f);
		return flWidth;
	}

	rect LabeledControlRect(std::string_view strLabel, float flHeight, rect& rcTotal, float& flLabelWidth)
	{
		stRegion& rRegion = CurrentWindow()->Region();
		const std::string_view strVisible = VisibleLabel(strLabel);
		const float flAvailable = GetContentRegionAvail().x;
		flLabelWidth = strVisible.empty() ? 0.f : MeasureText(strVisible).x;
		const float flControlWidth = strVisible.empty() ? CalcItemWidthFull(flAvailable) : std::min(CalcItemWidth(), std::max(flAvailable - flLabelWidth - Style().vecItemInnerSpacing.x * 2.f, std::min(60.f, flAvailable)));
		const float flTotalWidth = strVisible.empty() ? flControlWidth : std::max(flAvailable, flLabelWidth + Style().vecItemInnerSpacing.x * 2.f + flControlWidth);
		rcTotal = { rRegion.vecCursor, rRegion.vecCursor + vec2{ flTotalWidth, flHeight } };
		return { { rcTotal.max.x - flControlWidth, rcTotal.min.y }, rcTotal.max };
	}

	float CalcItemWidthFull(float flAvailable)
	{
		stWindow* pWindow = CurrentWindow();
		const float flWidth = pWindow->flNextItemWidth != -1.f ? pWindow->flNextItemWidth : (pWindow->aItemWidthStack.empty() ? -1.f : pWindow->aItemWidthStack.back());
		pWindow->flNextItemWidth = -1.f;
		if (flWidth == -1.f)
			return flAvailable;
		return flWidth < 0.f ? std::max(flAvailable + flWidth, 20.f) : flWidth;
	}

	void AlignTextToFramePadding()
	{
		stRegion& rRegion = CurrentWindow()->Region();
		rRegion.flCurrLineHeight = std::max(rRegion.flCurrLineHeight, FrameHeight());
		rRegion.flTextOffset = std::max(rRegion.flTextOffset, Style().vecFramePadding.y);
	}

	void PushID(std::string_view strID)
	{
		stWindow* pWindow = CurrentWindow();
		pWindow->aIDStack.push_back(pWindow->GetID(strID));
	}

	void PushID(int iID)
	{
		stWindow* pWindow = CurrentWindow();
		pWindow->aIDStack.push_back(HashBytes(&iID, sizeof(iID), pWindow->aIDStack.back()));
	}

	void PushID(const void* pID)
	{
		stWindow* pWindow = CurrentWindow();
		pWindow->aIDStack.push_back(HashBytes(&pID, sizeof(pID), pWindow->aIDStack.back()));
	}

	void PopID()
	{
		stWindow* pWindow = CurrentWindow();
		if (pWindow->aIDStack.size() > 1)
			pWindow->aIDStack.pop_back();
	}

	ID GetID(std::string_view strID) { return CurrentWindow()->GetID(strID); }

	clDrawList& GetWindowDrawList() { return CurrentWindow()->drawList; }

	bool IsItemHovered() { return CurrentWindow()->stLast.bHovered || (CurrentWindow()->stLast.uID && Ctx().uHoveredID == CurrentWindow()->stLast.uID); }
	bool IsItemActive() { return CurrentWindow()->stLast.uID && Ctx().uActiveID == CurrentWindow()->stLast.uID; }
	bool IsItemClicked() { return CurrentWindow()->stLast.bClicked; }
	bool IsItemFocused() { return CurrentWindow()->stLast.uID && Ctx().uNavID == CurrentWindow()->stLast.uID; }
	bool IsItemEdited() { return CurrentWindow()->stLast.bEdited; }
	ID GetItemID() { return CurrentWindow()->stLast.uID; }
	vec2 GetItemRectMin() { return CurrentWindow()->stLast.rcItem.min; }
	vec2 GetItemRectMax() { return CurrentWindow()->stLast.rcItem.max; }

	void SetKeyboardFocusHere()
	{
		Ctx().bNavRequestFocusHere = true;
	}
}
