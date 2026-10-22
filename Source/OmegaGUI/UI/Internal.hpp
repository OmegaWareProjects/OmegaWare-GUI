#pragma once

#include "OmegaGUI/OmegaGUI.hpp"
#include "OmegaGUI/Input/Gamepad.hpp"

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace OmegaGUI
{
	enum class EItemFlags : std::uint32_t
	{
		None = 0,
		NoNav = 1u << 0,
		NavConsumesHorizontal = 1u << 1,
		NavConsumesVertical = 1u << 2,
		PressOnClick = 1u << 3,
		Repeat = 1u << 4,
		AllowWhenBlocked = 1u << 5,
		NoHoldVisual = 1u << 6,
		RightClick = 1u << 7,
	};
	template <> struct stEnableFlags<EItemFlags> : std::true_type {};

	struct stScrollState
	{
		float flScrollY = 0.f;
		float flTargetY = -1.f;
		float flMaxY = 0.f;
		float flContentHeight = 0.f;
		float flVisibleHeight = 0.f;
	};

	struct stColumns
	{
		int iCount = 0;
		int iCurrent = 0;
		float flStartX = 0.f;
		float flStartY = 0.f;
		float flWidth = 0.f;
		float flMaxY = 0.f;
		float flSavedLineStartX = 0.f;
	};

	struct stGroup
	{
		vec2 vecCursorStart;
		vec2 vecCursorMaxBackup;
		float flLineStartBackup = 0.f;
		float flPrevLineHeightBackup = 0.f;
		float flCurrLineHeightBackup = 0.f;
	};

	struct stSection
	{
		ID uID = 0;
		vec2 vecStart;
		float flRight = 0.f;
		float flPadding = 0.f;
		float flSavedContentMaxX = 0.f;
		std::optional<stColumns> optSavedColumns;
	};

	struct stRegion
	{
		ID uID = 0;
		rect rcClip;
		rect rcContent;
		vec2 vecCursorStart;
		vec2 vecCursor;
		vec2 vecCursorMax;
		vec2 vecCursorPrevLine;
		float flLineStartX = 0.f;
		float flIndent = 0.f;
		float flPrevLineHeight = 0.f;
		float flCurrLineHeight = 0.f;
		float flTextOffset = 0.f;
		float flPrevTextOffset = 0.f;
		stScrollState* pScroll = nullptr;
		bool bBorder = false;
		bool bScrollbar = true;
		bool bWheel = true;
		rect rcOuter;
		std::optional<stColumns> optColumns;
		std::vector<stGroup> aGroups;
	};

	struct stNavItem
	{
		ID uID = 0;
		rect rcItem;
		ID uRegionID = 0;
		rect rcRegionClip;
		EItemFlags eFlags = EItemFlags::None;
	};

	struct stLastItem
	{
		ID uID = 0;
		rect rcItem;
		bool bHovered = false;
		bool bEdited = false;
		bool bClicked = false;
		bool bVisible = false;
	};

	struct stWindow
	{
		std::string strName;
		ID uID = 0;
		EWindowFlags eFlags = EWindowFlags::None;
		vec2 vecPosition;
		vec2 vecSize;
		vec2 vecSizeFull;
		vec2 vecContentSize;
		vec2 vecSizeMin{ 0.f, 0.f };
		vec2 vecSizeMax{ 1e6f, 1e6f };
		rect rcRestore;
		rect rcTitleBar;
		rect rcMenuBar;
		rect rcInner;
		bool bCollapsed = false;
		bool bMaximized = false;
		bool bActive = false;
		bool bWasActive = false;
		bool bAppearing = false;
		bool bHiddenThisFrame = false;
		bool bSkipItems = false;
		bool bNew = true;
		bool bResizeHovered = false;
		stWindow* pParentWindow = nullptr;
		int iLastFrameActive = -10;
		int iAutoFitFrames = 0;
		std::optional<std::pair<vec2, vec2>> optAutoFitPosition;
		vec2 vecAutoFitSize;
		float flAlpha = 1.f;
		float flCollapse = 0.f;
		clDrawList drawList;
		std::vector<ID> aIDStack;
		std::vector<stRegion> aRegions;
		std::unordered_map<ID, stScrollState> mapScroll;
		std::vector<stNavItem> aNavItems;
		std::vector<stNavItem> aNavItemsPrevious;
		std::vector<float> aItemWidthStack;
		std::vector<stSection> aSections;
		std::vector<ID> aTreeStack;
		float flNextItemWidth = -1.f;
		stLastItem stLast;
		ID uNavLastID = 0;
		ID uFirstTabBarID = 0;
		ID uFirstTabBarIDPrevious = 0;
		ID uPopupID = 0;
		bool bInMenuBar = false;
		vec2 vecMenuBarCursor;
		stRegion stMenuBarSavedRegion;
		ID uMenuBarOpenMenu = 0;

		[[nodiscard]] stRegion& Region() { return aRegions.back(); }
		[[nodiscard]] ID GetID(std::string_view strText) const { return HashString(strText, aIDStack.back()); }
		[[nodiscard]] rect Rect() const { return { vecPosition, vecPosition + vecSize }; }
	};

	struct stTabBar
	{
		ID uID = 0;
		ETabBarFlags eFlags = ETabBarFlags::None;
		ID uSelected = 0;
		ID uNextSelected = 0;
		std::vector<ID> aTabs;
		std::vector<ID> aTabsPrevious;
		std::vector<float> aWidthsPrevious;
		std::vector<float> aWidths;
		rect rcBar;
		vec2 vecBarCursor;
		float flIndicatorX = 0.f;
		float flIndicatorWidth = 0.f;
		float flIndicatorY = 0.f;
		float flIndicatorHeight = 0.f;
		bool bIndicatorInitialized = false;
		float flContentStartY = 0.f;
		float flSavedLineStartX = 0.f;
		float flSavedContentMinX = 0.f;
		float flBarScrollX = 0.f;
		bool bSubmittedSelected = false;
		rect rcSelected;
		stWindow* pWindow = nullptr;
		int iLastFrame = -1;
		ID uCurrentTab = 0;
	};

	struct stTableColumn
	{
		std::string strLabel;
		ETableColumnFlags eFlags = ETableColumnFlags::None;
		float flWidthOrWeight = 0.f;
		float flX = 0.f;
		float flWidth = 0.f;
	};

	struct stTable
	{
		ID uID = 0;
		ETableFlags eFlags = ETableFlags::None;
		stWindow* pWindow = nullptr;
		bool bChild = false;
		std::vector<stTableColumn> aColumns;
		std::vector<float> aRowHeights;
		std::vector<float> aRowHeightsPrevious;
		bool bLayoutDone = false;
		int iColumnCount = 0;
		int iHeaderRow = -1;
		bool bNextRowIsHeader = false;
		int iRow = -1;
		int iColumn = -1;
		bool bInRow = false;
		bool bClipPushed = false;
		float flRowMinHeight = 0.f;
		float flRowTop = 0.f;
		rect rcTable;
		vec2 vecCellPadding;
		float flSavedLineStartX = 0.f;
		float flSavedContentMaxX = 0.f;
		std::optional<stColumns> optSavedColumns;
		stTableSortSpecs stSort;
		bool bSortInitialized = false;
	};

	struct stPopup
	{
		ID uID = 0;
		stWindow* pWindow = nullptr;
		ID uParentWindowID = 0;
		vec2 vecOpenPosition;
		rect rcAnchor;
		int iOpenFrame = 0;
		bool bModal = false;
		bool bMenu = false;
	};

	struct stTextState
	{
		ID uID = 0;
		std::string strBuffer;
		std::string strInitial;
		int iCursor = 0;
		int iSelectStart = 0;
		int iSelectEnd = 0;
		float flScrollX = 0.f;
		float flBlink = 0.f;
		EInputTextFlags eFlags = EInputTextFlags::None;
		bool bOnScreenKeyboard = false;
		bool bOskShift = false;
		bool bSubmitted = false;
		bool bJustActivated = false;
		std::string* pTarget = nullptr;
	};

	struct stToast
	{
		EToastKind eKind = EToastKind::Info;
		std::string strText;
		float flDuration = 4.f;
		float flAge = 0.f;
		float flOffsetY = -1.f;
	};

	struct stRainbowState
	{
		bool bEnabled = false;
		float flSpeed = .25f;
		float flSaturation = 1.f;
		float flValue = 1.f;
		color* pTarget = nullptr;
	};

	struct stWindowSettings
	{
		std::string strName;
		vec2 vecPosition;
		vec2 vecSize;
		bool bCollapsed = false;
		bool bHasPosition = false;
		bool bHasSize = false;
	};

	struct stNextWindow
	{
		std::optional<std::pair<vec2, vec2>> optPosition;
		ECondition ePositionCondition = ECondition::FirstUse;
		std::optional<vec2> optSize;
		ECondition eSizeCondition = ECondition::FirstUse;
		std::optional<std::pair<vec2, vec2>> optConstraints;
		std::optional<bool> optCollapsed;
		ECondition eCollapsedCondition = ECondition::FirstUse;
		bool bFocus = false;
		void Clear() { *this = {}; }
	};

	struct stContext
	{
		clRenderBackend* pBackend = nullptr;
		void* hWindow = nullptr;
		stConfig stSettings;
		stStyle stCurrentStyle = stStyle::Console();
		stStyle stBaseStyle = stStyle::Console();
		clInput clInputState;
		clGamepadManager clGamepads;
		clFontAtlas clAtlas;
		stDrawListShared stShared;
		clDrawList clBackground;
		clDrawList clForeground;
		clDrawList clOverlay;
		clWorldDrawList clWorld;
		stDrawData stData;
		vec2 vecDisplaySize{ 1280.f, 720.f };
		bool bDisplaySizeOverride = false;
		std::int64_t iLastTicks = 0;
		std::int64_t iTickFrequency = 0;
		double dTime = 0.0;
		float flDeltaTime = 1.f / 60.f;
		int iFrame = 0;
		bool bInitialized = false;
		float flPendingScale = 0.f;
		std::vector<stGlyphRange> aExtraGlyphRanges;
		bool bPendingFontRebuild = false;
		std::string strLastError;
		bool bInFrame = false;
		bool bVisible = true;
		float flVisibility = 1.f;

		std::unordered_map<ID, std::unique_ptr<stWindow>> mapWindows;
		std::vector<stWindow*> aWindowOrder;
		std::vector<stWindow*> aWindowStack;
		stWindow* pCurrentWindow = nullptr;
		stWindow* pHoveredWindow = nullptr;
		stWindow* pFocusedWindow = nullptr;
		stWindow* pMovingWindow = nullptr;
		stWindow* pResizingWindow = nullptr;
		int iResizeEdges = 0;
		vec2 vecMoveOffset;
		rect rcResizeStart;
		vec2 vecResizeMouseStart;
		stNextWindow stNext;

		ID uHoveredID = 0;
		ID uHoveredIDPrevious = 0;
		ID uActiveID = 0;
		ID uActiveIDPrevious = 0;
		stWindow* pActiveWindow = nullptr;
		bool bActiveIDJustActivated = false;
		bool bActiveIDAlive = false;
		vec2 vecActiveClickOffset;
		int iDisabledDepth = 0;
		std::vector<bool> aDisabledStack;

		ID uNavID = 0;
		stWindow* pNavWindow = nullptr;
		ID uNavActivateID = 0;
		ID uNavPendingActivate = 0;
		bool bNavVisible = false;
		int iNavAdjust = 0;
		int iNavAdjustVertical = 0;
		bool bNavRequestFocusHere = false;
		ID uNavScrollRegion = 0;
		float flNavScrollTarget = 0.f;
		bool bNavCancel = false;
		bool bGamepadWindowManip = false;

		ID uWheelRegion = 0;
		ID uWheelRegionNext = 0;
		stWindow* pWheelWindow = nullptr;
		stWindow* pWheelWindowNext = nullptr;

		std::vector<stPopup> aOpenPopups;
		std::vector<stPopup> aBeginPopupStack;

		stTextState stText;
		ID uOskReturnWindow = 0;
		rect rcStickyActive;
		bool bTextDragging = false;
		std::unordered_map<ID, float> mapAnimations;
		std::unordered_map<ID, int> mapStorage;
		std::unordered_map<ID, stTabBar> mapTabBars;
		std::vector<stTabBar*> aTabBarStack;
		std::unordered_map<ID, stTable> mapTables;
		std::vector<stTable*> aTableStack;
		std::vector<stToast> aToasts;

		std::string strTooltip;
		float flHoverTimer = 0.f;
		ID uHoverTimerID = 0;
		bool bTooltipRequested = false;

		ID uHotkeyCapture = 0;
		int iHotkeyCaptureFrame = 0;

		std::unordered_map<ID, stWindowSettings> mapSettings;
		std::unordered_map<ID, stRainbowState> mapRainbows;
		bool bSettingsDirty = false;
		float flSettingsTimer = 0.f;
		bool bSettingsLoaded = false;

		EMouseCursor eCursor = EMouseCursor::Arrow;
		EMouseCursor eCursorPrevious = EMouseCursor::Arrow;
		bool bWantsMouse = false;
		bool bWantsKeyboard = false;
		bool bWantsGamepad = false;
	};

	extern stContext* g_pContext;

	[[nodiscard]] inline stContext& Ctx() { return *g_pContext; }
	[[nodiscard]] inline const stStyle& Style() { return g_pContext->stCurrentStyle; }
	[[nodiscard]] inline color StyleColor(EColor eColor) { return g_pContext->stCurrentStyle.Get(eColor); }
	[[nodiscard]] inline stWindow* CurrentWindow() { return g_pContext->pCurrentWindow; }

	[[nodiscard]] float Animate(ID uID, float flTarget, float flSpeed = -1.f);
	[[nodiscard]] float AnimateBool(ID uID, bool bValue, float flSpeed = -1.f);
	[[nodiscard]] color WithGlobalAlpha(color col);

	stWindow* FindWindowByName(std::string_view strName);
	stWindow* FindWindowByID(ID uID);
	stWindow* CreateWindowState(std::string_view strName, EWindowFlags eFlags);
	void BringWindowToFront(stWindow* pWindow);
	void FocusWindowInternal(stWindow* pWindow);
	void MarkSettingsDirty();
	void ApplySettings(stWindow* pWindow);

	bool ItemAdd(const rect& rcItem, ID uID, EItemFlags eFlags = EItemFlags::None);
	void ItemSize(const vec2& vecSize, float flTextOffset = 0.f);
	[[nodiscard]] bool ItemHoverable(const rect& rcItem, ID uID, EItemFlags eFlags = EItemFlags::None);
	bool ButtonBehavior(const rect& rcItem, ID uID, bool* pHovered, bool* pHeld, EItemFlags eFlags = EItemFlags::None);
	void SetActiveID(ID uID, stWindow* pWindow);
	void ClearActiveID();
	void KeepAliveID(ID uID);
	[[nodiscard]] bool IsDisabled();
	[[nodiscard]] bool IsClippedEx(const rect& rcItem);
	void RenderNavHighlight(const rect& rcItem, ID uID, float flRounding = -1.f);
	void RenderFrame(const rect& rcFrame, color colFill, bool bBorder, color colBorder, float flRounding);
	void RenderText(const vec2& vecPosition, std::string_view strText, color col);
	void RenderTextClipped(const rect& rcBounds, std::string_view strText, color col, const vec2& vecAlign = {}, const rect* pClip = nullptr);
	[[nodiscard]] vec2 MeasureText(std::string_view strText, float flWrapWidth = 0.f);
	[[nodiscard]] float FrameHeight();
	[[nodiscard]] float CalcItemWidthFull(float flAvailable);
	[[nodiscard]] rect LabeledControlRect(std::string_view strLabel, float flHeight, rect& rcTotal, float& flLabelWidth);

	bool InputTextFrame(ID uID, const rect& rcFrame, std::string& rText, std::string_view strHint, EInputTextFlags eFlags, bool bNavActivated);

	void BeginRegion(stWindow* pWindow, ID uID, const rect& rcOuter, const rect& rcContent, bool bBorder, bool bScrollbar, bool bWheel);
	void EndRegion(stWindow* pWindow);
	void ScrollToRect(stWindow* pWindow, ID uRegionID, const rect& rcRegionClip, const rect& rcItem);

	bool BeginPopupEx(ID uID, EWindowFlags eFlags, std::string_view strName);
	void OpenPopupEx(ID uID, const rect& rcAnchor, bool bModal, bool bMenu = false);
	void ClosePopupsOverWindow(stWindow* pWindow);
	void ClosePopupToLevel(std::size_t uLevel);
	[[nodiscard]] bool IsPopupOpenID(ID uID);
	[[nodiscard]] color RainbowColor(const stRainbowState& rState, std::uint8_t byAlpha);
	void UpdateRainbows();

	void UpdateNavigation();
	void UpdateWindowsInteraction();
	void UpdateTabBarGamepad(int iDirection);
	void RenderToasts();
	void RenderOnScreenKeyboard();
	void RenderSoftwareCursor();
	void ProcessSettingsTimer();
}
