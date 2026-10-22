#pragma once

#include "OmegaGUI/Core/Color.hpp"
#include "OmegaGUI/Core/Flags.hpp"
#include "OmegaGUI/Core/Hash.hpp"
#include "OmegaGUI/Input/Input.hpp"
#include "OmegaGUI/Render/Backend.hpp"
#include "OmegaGUI/Render/DrawList.hpp"
#include "OmegaGUI/Render/Font.hpp"
#include "OmegaGUI/Render/WorldDrawList.hpp"
#include "OmegaGUI/UI/Style.hpp"

#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace OmegaGUI
{
	struct stContext;

	enum class EWindowFlags : std::uint32_t
	{
		None = 0,
		NoTitleBar = 1u << 0,
		NoResize = 1u << 1,
		NoMove = 1u << 2,
		NoScrollbar = 1u << 3,
		NoScrollWithMouse = 1u << 4,
		NoCollapse = 1u << 5,
		NoMaximize = 1u << 6,
		NoBackground = 1u << 7,
		NoBringToFront = 1u << 8,
		AlwaysAutoResize = 1u << 9,
		NoSavedSettings = 1u << 10,
		NoNav = 1u << 11,
		NoInputs = 1u << 12,
		MenuBar = 1u << 13,
		DotGrid = 1u << 14,
		AlwaysVisible = 1u << 15,
		NoSnap = 1u << 16,
		CloseHidesMenu = 1u << 17,
		Popup = 1u << 24,
		Tooltip = 1u << 25,
		Modal = 1u << 26,
		Overlay = NoTitleBar | NoResize | NoMove | NoScrollbar | NoBackground | NoSavedSettings | NoNav | NoInputs | AlwaysVisible | NoBringToFront,
	};
	template <> struct stEnableFlags<EWindowFlags> : std::true_type {};

	enum class ETabBarFlags : std::uint32_t
	{
		None = 0,
		Vertical = 1u << 0,
		Stretch = 1u << 1,
		NoGamepadCycle = 1u << 2,
	};
	template <> struct stEnableFlags<ETabBarFlags> : std::true_type {};

	enum class ETableFlags : std::uint32_t
	{
		None = 0,
		Borders = 1u << 0,
		RowBackground = 1u << 1,
		Sortable = 1u << 2,
		HighlightHoveredRow = 1u << 3,
	};
	template <> struct stEnableFlags<ETableFlags> : std::true_type {};

	enum class ETableColumnFlags : std::uint32_t
	{
		None = 0,
		WidthFixed = 1u << 0,
		NoSort = 1u << 1,
		DefaultSort = 1u << 2,
		DefaultSortDescending = 1u << 3,
	};
	template <> struct stEnableFlags<ETableColumnFlags> : std::true_type {};

	struct stTableSortSpecs
	{
		int iColumn = -1;
		bool bAscending = true;
		bool bDirty = false;
	};

	enum class EInputTextFlags : std::uint32_t
	{
		None = 0,
		Password = 1u << 0,
		EnterReturnsTrue = 1u << 1,
		KeepFocusOnEnter = 1u << 2,
		CharsDecimal = 1u << 3,
		CharsHex = 1u << 4,
		ReadOnly = 1u << 5,
		NoOnScreenKeyboard = 1u << 6,
	};
	template <> struct stEnableFlags<EInputTextFlags> : std::true_type {};

	enum class ECondition : std::uint8_t
	{
		Always,
		FirstUse,
		Appearing,
	};

	enum class EButtonStyle : std::uint8_t
	{
		Primary,
		Secondary,
		Danger,
		Ghost,
	};

	enum class EToastKind : std::uint8_t
	{
		Info,
		Success,
		Warning,
		Error,
	};

	enum class EHotkeyMode : std::uint8_t
	{
		AlwaysOn,
		Hold,
		Toggle,
	};

	struct stHotkey
	{
		KeyCode uKey = 0;
		EHotkeyMode eMode = EHotkeyMode::Toggle;
		bool bNotify = false;
		bool bActive = false;
	};

	struct stHotkeyText
	{
		std::string_view strAlwaysOn = "always";
		std::string_view strHold = "hold";
		std::string_view strToggle = "toggle";
		std::string_view strNotify = "notify when toggled";
		std::string_view strHint = "right-click to change the mode";
	};

	struct stConfig
	{
		std::filesystem::path pathSettings;
		float flSettingsSaveInterval = 3.f;
		KeyCode uToggleKey = 0x2D;
		bool bToggleWithGamepad = true;
		bool bStartVisible = true;
		bool bKeyboardNavigation = true;
		bool bGamepadNavigation = true;
		bool bEnableXInput = true;
		bool bEnablePlayStation = true;
		bool bManualGamepad = false;
		bool bBlockInputWhenVisible = true;
		bool bDrawSoftwareCursor = false;
		bool bPollCursorPosition = true;
		float flFontSize = 15.f;
		float flScale = 1.f;
		std::filesystem::path pathFont;
	};

	[[nodiscard]] stContext* CreateContext();
	void DestroyContext(stContext* pContext = nullptr);
	[[nodiscard]] stContext* GetCurrentContext();
	void SetCurrentContext(stContext* pContext);

	bool Initialize(clRenderBackend* pBackend, void* hWindow, const stConfig& stSettings = {});
	void Shutdown();
	[[nodiscard]] std::string_view GetInitError();
	void SetBackend(clRenderBackend* pBackend);
	bool RebuildFonts();
	void SetGlyphRanges(std::span<const stGlyphRange> aRanges);
	void SetScale(float flScale);

	[[nodiscard]] stStyle& GetStyle();
	[[nodiscard]] stConfig& GetConfig();
	[[nodiscard]] clInput& GetInput();
	[[nodiscard]] clFontAtlas& GetFonts();

	bool ProcessWin32Message(void* hWindow, std::uint32_t uMessage, std::uintptr_t uWParam, std::intptr_t iLParam);
	void SetDisplaySize(const vec2& vecSize);
	[[nodiscard]] vec2 GetDisplaySize();

	void NewFrame();
	void Render();
	void EndFrame();
	[[nodiscard]] const stDrawData& GetDrawData();
	void RenderWorld(const stWorldPass& stPass = {});

	[[nodiscard]] bool IsVisible();
	void SetVisible(bool bVisible);
	void ToggleVisible();
	[[nodiscard]] bool WantsMouse();
	[[nodiscard]] bool WantsKeyboard();
	[[nodiscard]] bool WantsGamepad();
	[[nodiscard]] double GetTime();
	[[nodiscard]] float GetDeltaTime();
	[[nodiscard]] int GetFrameCount();
	void SetMouseCursor(EMouseCursor eCursor);
	[[nodiscard]] EMouseCursor GetMouseCursor();

	[[nodiscard]] clDrawList& GetBackgroundDrawList();
	[[nodiscard]] clDrawList& GetForegroundDrawList();
	[[nodiscard]] clDrawList& GetWindowDrawList();
	[[nodiscard]] clWorldDrawList& GetWorldDrawList();

	void SetNextWindowPos(const vec2& vecPosition, ECondition eCondition = ECondition::FirstUse, const vec2& vecPivot = {});
	void SetNextWindowSize(const vec2& vecSize, ECondition eCondition = ECondition::FirstUse);
	void SetNextWindowSizeConstraints(const vec2& vecMin, const vec2& vecMax);
	void SetNextWindowCollapsed(bool bCollapsed, ECondition eCondition = ECondition::FirstUse);
	void SetNextWindowFocus();
	bool Begin(std::string_view strName, bool* pOpen = nullptr, EWindowFlags eFlags = EWindowFlags::None);
	void End();
	bool BeginChild(std::string_view strID, const vec2& vecSize = {}, bool bBorder = true, EWindowFlags eFlags = EWindowFlags::None);
	void EndChild();
	[[nodiscard]] vec2 GetWindowPos();
	[[nodiscard]] vec2 GetWindowSize();
	[[nodiscard]] bool IsWindowFocused();
	[[nodiscard]] bool IsWindowHovered();
	[[nodiscard]] bool IsWindowCollapsed();
	void SetWindowPos(std::string_view strName, const vec2& vecPosition);
	void SetWindowSize(std::string_view strName, const vec2& vecSize);
	void SetWindowCollapsed(std::string_view strName, bool bCollapsed);
	void FocusWindow(std::string_view strName);

	[[nodiscard]] vec2 GetContentRegionAvail();
	[[nodiscard]] vec2 GetCursorPos();
	void SetCursorPos(const vec2& vecPosition);
	[[nodiscard]] vec2 GetCursorScreenPos();
	void SetCursorScreenPos(const vec2& vecPosition);
	[[nodiscard]] float GetScrollY();
	[[nodiscard]] float GetScrollMaxY();
	void SetScrollY(float flScroll);
	void SetClipboardText(std::string_view strText);
	[[nodiscard]] std::string GetClipboardText();
	void SetScrollHereY(float flRatio = .5f);

	void SameLine(float flOffsetFromStart = 0.f, float flSpacing = -1.f);
	void NewLine();
	void Spacing();
	void Dummy(const vec2& vecSize);
	void Indent(float flWidth = 0.f);
	void Unindent(float flWidth = 0.f);
	void BeginGroup();
	void EndGroup();
	void BeginColumns(int iCount);
	void NextColumn();
	void EndColumns();
	void PushItemWidth(float flWidth);
	void PopItemWidth();
	void SetNextItemWidth(float flWidth);
	[[nodiscard]] float CalcItemWidth();
	void AlignTextToFramePadding();
	[[nodiscard]] float GetFrameHeight();
	[[nodiscard]] float GetTextLineHeight();
	[[nodiscard]] vec2 CalcTextSize(std::string_view strText, float flWrapWidth = 0.f);
	void BeginDisabled(bool bDisabled = true);
	void EndDisabled();

	void PushID(std::string_view strID);
	void PushID(int iID);
	void PushID(const void* pID);
	void PopID();
	[[nodiscard]] ID GetID(std::string_view strID);

	void TextUnformatted(std::string_view strText);
	void TextColored(color col, std::string_view strText);
	void TextMuted(std::string_view strText);
	void TextDisabled(std::string_view strText);
	void TextWrapped(std::string_view strText);
	void LabelText(std::string_view strLabel, std::string_view strValue);
	void BulletText(std::string_view strText);
	void Title(std::string_view strText);
	void Header(std::string_view strText);
	void SubHeader(std::string_view strText);
	void Tag(std::string_view strText, color col = {});
	void Breadcrumbs(std::span<const std::string_view> aParts);
	void Separator();
	void SeparatorText(std::string_view strLabel);
	bool BeginSection(std::string_view strLabel);
	void EndSection();
	void Alert(EToastKind eKind, std::string_view strText);
	void KeyHint(KeyCode uKey, std::string_view strAction);
	void ControllerHints();
	void ControllerLegend();

	template <typename... Args>
	void Text(std::format_string<Args...> strFormat, Args&&... args)
	{
		TextUnformatted(std::format(strFormat, std::forward<Args>(args)...));
	}

	bool Button(std::string_view strLabel, const vec2& vecSize = {}, EButtonStyle eStyle = EButtonStyle::Primary);
	bool SmallButton(std::string_view strLabel, EButtonStyle eStyle = EButtonStyle::Secondary);
	bool InvisibleButton(std::string_view strID, const vec2& vecSize);
	bool TextLink(std::string_view strLabel);
	bool Checkbox(std::string_view strLabel, bool* pValue);
	bool Toggle(std::string_view strLabel, bool* pValue);
	bool RadioButton(std::string_view strLabel, bool bActive);
	bool RadioButton(std::string_view strLabel, int* pValue, int iButtonValue);
	bool SliderFloat(std::string_view strLabel, float* pValue, float flMin, float flMax, std::string_view strFormat = "{:.2f}", float flNavStep = 0.f);
	bool SliderInt(std::string_view strLabel, int* pValue, int iMin, int iMax, std::string_view strFormat = "{}");
	bool InputInt(std::string_view strLabel, int* pValue, int iStep = 1, int iMin = INT32_MIN, int iMax = INT32_MAX);
	bool InputFloat(std::string_view strLabel, float* pValue, float flStep = .1f, std::string_view strFormat = "{:.2f}");
	bool BeginCombo(std::string_view strLabel, std::string_view strPreview);
	void EndCombo();
	bool Combo(std::string_view strLabel, int* pCurrent, std::span<const std::string_view> aItems);
	bool Selectable(std::string_view strLabel, bool bSelected = false, const vec2& vecSize = {});
	bool InputText(std::string_view strLabel, std::string* pText, std::string_view strHint = {}, EInputTextFlags eFlags = EInputTextFlags::None);
	bool Hotkey(std::string_view strLabel, KeyCode* pKey);
	bool Hotkey(std::string_view strLabel, stHotkey* pHotkey, const stHotkeyText& stText = {});
	bool UpdateHotkey(stHotkey& rHotkey);
	bool ColorEdit(std::string_view strLabel, color* pColor, bool bAlpha = true);
	void ProgressBar(float flFraction, const vec2& vecSize = {}, std::string_view strOverlay = {});
	void PlotLines(std::string_view strLabel, std::span<const float> aValues, float flMin = 0.f, float flMax = 0.f, const vec2& vecSize = {});
	void PlotHistogram(std::string_view strLabel, std::span<const float> aValues, float flMin = 0.f, float flMax = 0.f, const vec2& vecSize = {});
	bool CollapsingHeader(std::string_view strLabel, bool bDefaultOpen = false);
	bool TreeNode(std::string_view strLabel, bool bDefaultOpen = false);
	void TreePop();
	void Image(TextureHandle hTexture, const vec2& vecSize, const vec2& vecUVMin = { 0.f, 0.f }, const vec2& vecUVMax = { 1.f, 1.f }, color colTint = Colors::White);

	void SetTooltip(std::string_view strText);
	void ItemTooltip(std::string_view strText);
	bool BeginTooltip();
	void EndTooltip();

	[[nodiscard]] bool IsItemHovered();
	[[nodiscard]] bool IsItemActive();
	[[nodiscard]] bool IsItemClicked();
	[[nodiscard]] bool IsItemFocused();
	[[nodiscard]] bool IsItemEdited();
	[[nodiscard]] ID GetItemID();
	[[nodiscard]] vec2 GetItemRectMin();
	[[nodiscard]] vec2 GetItemRectMax();
	void SetKeyboardFocusHere();
	void SetInputTextValue(ID uID, std::string_view strText);

	void OpenPopup(std::string_view strID);
	bool BeginPopup(std::string_view strID);
	bool BeginPopupModal(std::string_view strName, bool* pOpen = nullptr);
	bool BeginPopupContextItem(std::string_view strID = {});
	void EndPopup();
	void CloseCurrentPopup();
	[[nodiscard]] bool IsPopupOpen(std::string_view strID);
	[[nodiscard]] bool IsAnyPopupOpen();

	bool BeginMenuBar();
	void EndMenuBar();
	bool BeginMenu(std::string_view strLabel, bool bEnabled = true);
	void EndMenu();
	bool MenuItem(std::string_view strLabel, std::string_view strShortcut = {}, bool* pSelected = nullptr, bool bEnabled = true);

	bool BeginTable(std::string_view strID, int iColumns, ETableFlags eFlags = ETableFlags::None, const vec2& vecOuterSize = {});
	void EndTable();
	void TableSetupColumn(std::string_view strLabel, ETableColumnFlags eFlags = ETableColumnFlags::None, float flWidthOrWeight = 0.f);
	void TableHeadersRow();
	void TableNextRow(float flMinHeight = 0.f);
	bool TableNextColumn();
	bool TableSetColumnIndex(int iColumn);
	[[nodiscard]] int TableGetColumnIndex();
	[[nodiscard]] int TableGetRowIndex();
	[[nodiscard]] stTableSortSpecs* TableGetSortSpecs();

	bool BeginTabBar(std::string_view strID, ETabBarFlags eFlags = ETabBarFlags::None);
	void EndTabBar();
	bool BeginTabItem(std::string_view strLabel, bool* pOpen = nullptr);
	void EndTabItem();
	void SetTabItemSelected(std::string_view strTabBarID, std::string_view strLabel);
	void TabBarSection(std::string_view strLabel);

	void PushToast(EToastKind eKind, std::string_view strText, float flDuration = 4.f);

	bool LoadSettings(const std::filesystem::path& pathFile = {});
	bool SaveSettings(const std::filesystem::path& pathFile = {});
}
