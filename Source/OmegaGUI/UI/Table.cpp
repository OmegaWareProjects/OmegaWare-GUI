#include "OmegaGUI/UI/Internal.hpp"

namespace OmegaGUI
{
	namespace
	{
		[[nodiscard]] stTable* CurrentTable()
		{
			stContext& g = Ctx();
			if (g.aTableStack.empty() || g.aTableStack.back()->pWindow != CurrentWindow())
				return nullptr;
			return g.aTableStack.back();
		}

		void LayoutColumns(stTable& rTable)
		{
			if (rTable.bLayoutDone)
				return;
			rTable.bLayoutDone = true;

			float flFixed = 0.f;
			float flWeights = 0.f;
			for (const stTableColumn& rColumn : rTable.aColumns)
			{
				if (HasFlag(rColumn.eFlags, ETableColumnFlags::WidthFixed))
					flFixed += rColumn.flWidthOrWeight > 0.f ? rColumn.flWidthOrWeight : MeasureText(VisibleLabel(rColumn.strLabel)).x + rTable.vecCellPadding.x * 2.f;
				else
					flWeights += rColumn.flWidthOrWeight > 0.f ? rColumn.flWidthOrWeight : 1.f;
			}

			const float flStretch = std::max(rTable.rcTable.Width() - flFixed, 0.f);
			float flX = rTable.rcTable.min.x;
			for (stTableColumn& rColumn : rTable.aColumns)
			{
				if (HasFlag(rColumn.eFlags, ETableColumnFlags::WidthFixed))
					rColumn.flWidth = rColumn.flWidthOrWeight > 0.f ? rColumn.flWidthOrWeight : MeasureText(VisibleLabel(rColumn.strLabel)).x + rTable.vecCellPadding.x * 2.f;
				else
					rColumn.flWidth = flWeights > 0.f ? std::floor(flStretch * (rColumn.flWidthOrWeight > 0.f ? rColumn.flWidthOrWeight : 1.f) / flWeights) : 0.f;
				rColumn.flX = flX;
				flX += rColumn.flWidth;
			}
			if (!rTable.aColumns.empty() && flWeights > 0.f)
			{
				stTableColumn& rLast = rTable.aColumns.back();
				if (!HasFlag(rLast.eFlags, ETableColumnFlags::WidthFixed))
					rLast.flWidth = rTable.rcTable.max.x - rLast.flX;
			}

			if (HasFlag(rTable.eFlags, ETableFlags::Sortable) && !rTable.bSortInitialized)
			{
				rTable.bSortInitialized = true;
				for (int i = 0; i < static_cast<int>(rTable.aColumns.size()); ++i)
				{
					const ETableColumnFlags eFlags = rTable.aColumns[i].eFlags;
					if (HasFlag(eFlags, ETableColumnFlags::DefaultSort) || HasFlag(eFlags, ETableColumnFlags::DefaultSortDescending))
					{
						rTable.stSort = { i, !HasFlag(eFlags, ETableColumnFlags::DefaultSortDescending), true };
						break;
					}
				}
			}
		}

		void EnsureColumns(stTable& rTable)
		{
			while (static_cast<int>(rTable.aColumns.size()) < rTable.iColumnCount)
				rTable.aColumns.push_back({});
			LayoutColumns(rTable);
		}

		void CloseCell(stTable& rTable)
		{
			stWindow* pWindow = CurrentWindow();
			stRegion& rRegion = pWindow->Region();
			if (rTable.iColumn >= 0 && rRegion.flCurrLineHeight > 0.f)
				ItemSize({ 0.f, 0.f });
			if (rTable.bClipPushed)
			{
				pWindow->drawList.PopClipRect();
				rTable.bClipPushed = false;
			}
		}

		void EndRow(stTable& rTable)
		{
			if (!rTable.bInRow)
				return;
			CloseCell(rTable);
			stWindow* pWindow = CurrentWindow();
			stRegion& rRegion = pWindow->Region();
			const float flBottom = std::floor(std::max(rTable.flRowTop + rTable.flRowMinHeight, std::max(rRegion.vecCursorMax.y, rTable.flRowTop) + rTable.vecCellPadding.y));
			if (rTable.iRow >= static_cast<int>(rTable.aRowHeights.size()))
				rTable.aRowHeights.resize(rTable.iRow + 1, 0.f);
			rTable.aRowHeights[rTable.iRow] = flBottom - rTable.flRowTop;
			if (HasFlag(rTable.eFlags, ETableFlags::Borders))
				pWindow->drawList.AddRectFilled({ rTable.rcTable.min.x, flBottom - 1.f }, { rTable.rcTable.max.x, flBottom }, WithGlobalAlpha(Style().Get(EColor::Border).MultiplyAlpha(.6f)));
			rTable.flRowTop = flBottom;
			rRegion.vecCursor.y = flBottom;
			rRegion.vecCursorMax.y = std::max(rRegion.vecCursorMax.y, flBottom);
			rTable.bInRow = false;
			rTable.iColumn = -1;
		}

		void OpenCell(stTable& rTable, int iColumn)
		{
			CloseCell(rTable);
			stWindow* pWindow = CurrentWindow();
			stRegion& rRegion = pWindow->Region();
			const stTableColumn& rColumn = rTable.aColumns[iColumn];
			rTable.iColumn = iColumn;

			const rect& rcClip = pWindow->drawList.GetClipRect();
			pWindow->drawList.PushClipRect({ { rColumn.flX, rcClip.min.y }, { rColumn.flX + rColumn.flWidth, rcClip.max.y } });
			rTable.bClipPushed = true;

			const float flX = rColumn.flX + rTable.vecCellPadding.x;
			rRegion.flLineStartX = flX - rRegion.flIndent;
			rRegion.vecCursor = { flX, rTable.flRowTop + rTable.vecCellPadding.y };
			rRegion.vecCursorPrevLine = rRegion.vecCursor;
			rRegion.rcContent.max.x = std::max(rColumn.flX + rColumn.flWidth - rTable.vecCellPadding.x, flX + 1.f);
			rRegion.flCurrLineHeight = 0.f;
			rRegion.flTextOffset = 0.f;
		}
	}

	bool BeginTable(std::string_view strID, int iColumns, ETableFlags eFlags, const vec2& vecOuterSize)
	{
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		if (pWindow->bSkipItems || iColumns <= 0)
			return false;

		const ID uID = pWindow->GetID(strID);
		bool bChild = false;
		if (vecOuterSize.y > 0.f)
		{
			if (!BeginChild(strID, vecOuterSize, false))
			{
				EndChild();
				return false;
			}
			bChild = true;
		}
		else if (pWindow->Region().flCurrLineHeight > 0.f)
			NewLine();

		stRegion& rRegion = pWindow->Region();
		stTable& rTable = g.mapTables[uID];
		rTable.uID = uID;
		rTable.eFlags = eFlags;
		rTable.pWindow = pWindow;
		rTable.bChild = bChild;
		rTable.aColumns.clear();
		rTable.aRowHeightsPrevious.swap(rTable.aRowHeights);
		rTable.aRowHeights.clear();
		rTable.bLayoutDone = false;
		rTable.iRow = -1;
		rTable.iColumn = -1;
		rTable.bInRow = false;
		rTable.bClipPushed = false;
		rTable.vecCellPadding = { std::floor(Style().vecFramePadding.x * .75f), std::floor(Style().vecFramePadding.y * .5f) };

		const float flWidth = vecOuterSize.x > 0.f ? vecOuterSize.x : (bChild ? GetContentRegionAvail().x : std::max(GetContentRegionAvail().x + vecOuterSize.x, 16.f));
		rTable.rcTable = { rRegion.vecCursor, rRegion.vecCursor + vec2{ std::floor(flWidth), 0.f } };
		rTable.flRowTop = rRegion.vecCursor.y;
		rTable.flSavedLineStartX = rRegion.flLineStartX;
		rTable.flSavedContentMaxX = rRegion.rcContent.max.x;
		rTable.optSavedColumns = rRegion.optColumns;
		rRegion.optColumns.reset();
		rTable.iColumnCount = iColumns;
		rTable.iHeaderRow = -1;
		rTable.bNextRowIsHeader = false;
		rTable.flRowMinHeight = 0.f;
		rTable.stSort.iColumn = std::min(rTable.stSort.iColumn, iColumns - 1);

		pWindow->aIDStack.push_back(uID);
		g.aTableStack.push_back(&rTable);
		return true;
	}

	void TableSetupColumn(std::string_view strLabel, ETableColumnFlags eFlags, float flWidthOrWeight)
	{
		stTable* pTable = CurrentTable();
		if (!pTable || pTable->bLayoutDone)
			return;
		if (static_cast<int>(pTable->aColumns.size()) >= pTable->iColumnCount)
			return;
		pTable->aColumns.push_back({ std::string(strLabel), eFlags, flWidthOrWeight });
	}

	void TableNextRow(float flMinHeight)
	{
		stTable* pTable = CurrentTable();
		if (!pTable)
			return;
		stTable& rTable = *pTable;
		EnsureColumns(rTable);
		EndRow(rTable);

		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		const stStyle& rStyle = Style();
		++rTable.iRow;
		rTable.bInRow = true;
		rTable.iColumn = -1;
		rTable.flRowMinHeight = flMinHeight;
		const bool bHeader = rTable.bNextRowIsHeader;
		rTable.bNextRowIsHeader = false;
		if (bHeader)
		{
			rTable.iHeaderRow = rTable.iRow;
			return;
		}

		const int iDataRow = rTable.iRow - (rTable.iHeaderRow >= 0 ? rTable.iHeaderRow + 1 : 0);
		const float flPreviousHeight = rTable.iRow < static_cast<int>(rTable.aRowHeightsPrevious.size()) ? rTable.aRowHeightsPrevious[rTable.iRow] : 0.f;
		if (flPreviousHeight > 0.f)
		{
			const rect rcRow{ { rTable.rcTable.min.x, rTable.flRowTop }, { rTable.rcTable.max.x, rTable.flRowTop + flPreviousHeight } };
			if (HasFlag(rTable.eFlags, ETableFlags::RowBackground) && (iDataRow & 1))
				pWindow->drawList.AddRectFilled(rcRow.min, rcRow.max, WithGlobalAlpha(rStyle.Get(EColor::Surface2).MultiplyAlpha(.35f)));
			const bool bHovered = g.pHoveredWindow == pWindow && !g.pMovingWindow && !g.pResizingWindow && rcRow.Clipped(pWindow->drawList.GetClipRect()).Contains(g.clInputState.GetMousePosition());
			if (HasFlag(rTable.eFlags, ETableFlags::HighlightHoveredRow) && bHovered)
				pWindow->drawList.AddRectFilled(rcRow.min, rcRow.max, WithGlobalAlpha(rStyle.Get(EColor::Hover)));
		}
	}

	bool TableSetColumnIndex(int iColumn)
	{
		stTable* pTable = CurrentTable();
		if (!pTable)
			return false;
		EnsureColumns(*pTable);
		if (!pTable->bInRow)
			TableNextRow();
		if (iColumn < 0 || iColumn >= static_cast<int>(pTable->aColumns.size()))
			return false;
		OpenCell(*pTable, iColumn);
		return pTable->aColumns[iColumn].flWidth > 0.f;
	}

	bool TableNextColumn()
	{
		stTable* pTable = CurrentTable();
		if (!pTable)
			return false;
		EnsureColumns(*pTable);
		if (!pTable->bInRow || pTable->iColumn + 1 >= static_cast<int>(pTable->aColumns.size()))
			TableNextRow();
		return TableSetColumnIndex(pTable->iColumn + 1);
	}

	int TableGetColumnIndex()
	{
		const stTable* pTable = CurrentTable();
		return pTable ? pTable->iColumn : -1;
	}

	int TableGetRowIndex()
	{
		const stTable* pTable = CurrentTable();
		return pTable ? pTable->iRow : -1;
	}

	stTableSortSpecs* TableGetSortSpecs()
	{
		stTable* pTable = CurrentTable();
		if (!pTable || !HasFlag(pTable->eFlags, ETableFlags::Sortable))
			return nullptr;
		EnsureColumns(*pTable);
		return &pTable->stSort;
	}

	void TableHeadersRow()
	{
		stTable* pTable = CurrentTable();
		if (!pTable)
			return;
		stTable& rTable = *pTable;
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		const stStyle& rStyle = Style();
		EnsureColumns(rTable);
		rTable.bNextRowIsHeader = true;
		TableNextRow(FrameHeight());
		const bool bSortable = HasFlag(rTable.eFlags, ETableFlags::Sortable);

		for (int i = 0; i < static_cast<int>(rTable.aColumns.size()); ++i)
		{
			TableSetColumnIndex(i);
			const stTableColumn& rColumn = rTable.aColumns[i];
			const rect rcHeader{ { rColumn.flX, rTable.flRowTop }, { rColumn.flX + rColumn.flWidth, rTable.flRowTop + FrameHeight() } };
			const ID uID = pWindow->GetID(rColumn.strLabel.empty() ? std::string_view("##column") : std::string_view(rColumn.strLabel));
			const bool bCanSort = bSortable && !HasFlag(rColumn.eFlags, ETableColumnFlags::NoSort);

			pWindow->drawList.AddRectFilled(rcHeader.min, rcHeader.max, WithGlobalAlpha(rStyle.Get(EColor::Surface2).MultiplyAlpha(.8f)));
			pWindow->drawList.AddRectFilled({ rcHeader.min.x, rcHeader.max.y - 1.f }, rcHeader.max, WithGlobalAlpha(rStyle.Get(EColor::Border)));

			bool bHovered = false;
			bool bPressed = false;
			if (ItemAdd(rcHeader, bCanSort ? uID : 0, bCanSort ? EItemFlags::None : EItemFlags::NoNav) && bCanSort)
				bPressed = ButtonBehavior(rcHeader, uID, &bHovered, nullptr);
			if (bPressed)
			{
				if (rTable.stSort.iColumn == i)
					rTable.stSort.bAscending = !rTable.stSort.bAscending;
				else
					rTable.stSort = { i, !HasFlag(rColumn.eFlags, ETableColumnFlags::DefaultSortDescending), false };
				rTable.stSort.bDirty = true;
			}
			const float flHover = bCanSort ? AnimateBool(uID, bHovered || (g.uNavID == uID && g.bNavVisible)) : 0.f;
			if (flHover > .01f)
				pWindow->drawList.AddRectFilled(rcHeader.min, rcHeader.max, WithGlobalAlpha(rStyle.Get(EColor::Hover).MultiplyAlpha(flHover)));

			const bool bSorted = bSortable && rTable.stSort.iColumn == i;
			const color colText = bSorted ? rStyle.Get(EColor::Accent) : rStyle.Get(EColor::TextHeading).Lerp(rStyle.Get(EColor::Accent), flHover);
			const float flArrowSpace = bSorted ? FrameHeight() * .6f : 0.f;
			RenderTextClipped({ { rcHeader.min.x + rTable.vecCellPadding.x, rcHeader.min.y }, { rcHeader.max.x - rTable.vecCellPadding.x - flArrowSpace, rcHeader.max.y } }, rColumn.strLabel, colText, { 0.f, .5f });
			if (bSorted)
			{
				const float flArrow = std::floor(FrameHeight() * .14f);
				const vec2 vecCenter{ rcHeader.max.x - rTable.vecCellPadding.x - flArrow * 1.5f, std::floor(rcHeader.Center().y) };
				const float flFlip = rTable.stSort.bAscending ? -1.f : 1.f;
				pWindow->drawList.AddTriangleFilled(vecCenter + vec2{ -flArrow, -flArrow * .5f * flFlip }, vecCenter + vec2{ flArrow, -flArrow * .5f * flFlip }, vecCenter + vec2{ 0.f, flArrow * .6f * flFlip }, WithGlobalAlpha(colText));
			}
			if (g.uNavID == uID && g.bNavVisible)
				RenderNavHighlight(rcHeader, uID, 0.f);

			stRegion& rRegion = pWindow->Region();
			rRegion.vecCursorMax.y = std::max(rRegion.vecCursorMax.y, rcHeader.max.y - rTable.vecCellPadding.y);
			pWindow->stLast.uID = bCanSort ? uID : 0;
			pWindow->stLast.rcItem = rcHeader;
			pWindow->stLast.bHovered = bHovered;
			pWindow->stLast.bClicked = bPressed;
		}
	}

	void EndTable()
	{
		stTable* pTable = CurrentTable();
		if (!pTable)
			return;
		stTable& rTable = *pTable;
		stContext& g = Ctx();
		stWindow* pWindow = CurrentWindow();
		EnsureColumns(rTable);
		EndRow(rTable);

		stRegion& rRegion = pWindow->Region();
		const float flBottom = rTable.flRowTop;
		rTable.rcTable.max.y = flBottom;
		if (HasFlag(rTable.eFlags, ETableFlags::Borders) && flBottom > rTable.rcTable.min.y)
		{
			const color colBorder = WithGlobalAlpha(Style().Get(EColor::Border));
			clDrawList& rList = pWindow->drawList;
			rList.AddRect(rTable.rcTable.min, rTable.rcTable.max, colBorder);
			for (std::size_t i = 1; i < rTable.aColumns.size(); ++i)
				rList.AddRectFilled({ rTable.aColumns[i].flX, rTable.rcTable.min.y }, { rTable.aColumns[i].flX + 1.f, flBottom }, colBorder.MultiplyAlpha(.6f));
		}

		rRegion.flLineStartX = rTable.flSavedLineStartX;
		rRegion.rcContent.max.x = rTable.flSavedContentMaxX;
		rRegion.optColumns = rTable.optSavedColumns;
		rRegion.vecCursor = { rTable.rcTable.min.x, flBottom };
		rRegion.flCurrLineHeight = 0.f;
		ItemSize({ rTable.rcTable.Width(), 0.f });
		pWindow->stLast = {};
		pWindow->stLast.uID = rTable.uID;
		pWindow->stLast.rcItem = rTable.rcTable;
		pWindow->stLast.bVisible = !IsClippedEx(rTable.rcTable);

		g.aTableStack.pop_back();
		if (pWindow->aIDStack.size() > 1)
			pWindow->aIDStack.pop_back();
		if (rTable.bChild)
			EndChild();
	}
}
