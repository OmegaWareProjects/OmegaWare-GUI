#include "OmegaGUI/UI/Internal.hpp"

#include <Windows.h>
#include <windowsx.h>

#include <fstream>
#include <sstream>

namespace OmegaGUI
{
	stContext* g_pContext = nullptr;

	stContext* CreateContext()
	{
		auto* pContext = new stContext();
		if (!g_pContext)
			g_pContext = pContext;
		return pContext;
	}

	void DestroyContext(stContext* pContext)
	{
		if (!pContext)
			pContext = g_pContext;
		if (!pContext)
			return;
		if (pContext == g_pContext)
			g_pContext = nullptr;
		delete pContext;
	}

	stContext* GetCurrentContext() { return g_pContext; }
	void SetCurrentContext(stContext* pContext) { g_pContext = pContext; }

	stStyle& GetStyle() { return Ctx().stCurrentStyle; }
	stConfig& GetConfig() { return Ctx().stSettings; }
	clInput& GetInput() { return Ctx().clInputState; }
	clFontAtlas& GetFonts() { return Ctx().clAtlas; }
	double GetTime() { return Ctx().dTime; }
	float GetDeltaTime() { return Ctx().flDeltaTime; }
	int GetFrameCount() { return Ctx().iFrame; }
	void SetMouseCursor(EMouseCursor eCursor) { Ctx().eCursor = eCursor; }
	EMouseCursor GetMouseCursor() { return Ctx().eCursorPrevious; }
	clDrawList& GetBackgroundDrawList() { return Ctx().clBackground; }
	clDrawList& GetForegroundDrawList() { return Ctx().clForeground; }
	clWorldDrawList& GetWorldDrawList() { return Ctx().clWorld; }
	const stDrawData& GetDrawData() { return Ctx().stData; }
	bool IsVisible() { return Ctx().bVisible; }
	bool WantsMouse() { return Ctx().bWantsMouse; }
	bool WantsKeyboard() { return Ctx().bWantsKeyboard; }
	bool WantsGamepad() { return Ctx().bWantsGamepad; }

	void SetVisible(bool bVisible)
	{
		stContext& g = Ctx();
		if (g.bVisible == bVisible)
			return;
		g.bVisible = bVisible;
		if (!bVisible)
		{
			ClearActiveID();
			g.aOpenPopups.clear();
			g.stText = {};
			g.pMovingWindow = g.pResizingWindow = nullptr;
		}
	}

	void ToggleVisible() { SetVisible(!Ctx().bVisible); }

	void SetDisplaySize(const vec2& vecSize)
	{
		Ctx().vecDisplaySize = vecSize;
		Ctx().bDisplaySizeOverride = true;
	}

	vec2 GetDisplaySize() { return Ctx().vecDisplaySize; }

	float Animate(ID uID, float flTarget, float flSpeed)
	{
		stContext& g = Ctx();
		if (!g.stCurrentStyle.bAnimations)
			return flTarget;
		auto [itEntry, bInserted] = g.mapAnimations.try_emplace(uID, flTarget);
		if (bInserted)
			return flTarget;
		float& rValue = itEntry->second;
		rValue = ApproachExp(rValue, flTarget, flSpeed > 0.f ? flSpeed : g.stCurrentStyle.flAnimationSpeed, g.flDeltaTime);
		if (std::abs(rValue - flTarget) < 1e-3f)
			rValue = flTarget;
		return rValue;
	}

	float AnimateBool(ID uID, bool bValue, float flSpeed)
	{
		return Animate(uID, bValue ? 1.f : 0.f, flSpeed);
	}

	color WithGlobalAlpha(color col)
	{
		stContext& g = Ctx();
		float flAlpha = g.pCurrentWindow ? g.pCurrentWindow->flAlpha : 1.f;
		if (g.iDisabledDepth > 0)
			flAlpha *= .45f;
		return flAlpha >= 1.f ? col : col.MultiplyAlpha(flAlpha);
	}

	std::string_view GetInitError()
	{
		return g_pContext ? std::string_view(g_pContext->strLastError) : std::string_view("no context");
	}

	bool RebuildFonts()
	{
		stContext& g = Ctx();
		const float flScale = std::max(g.stSettings.flScale, .25f);
		const float flSize = std::round(g.stSettings.flFontSize * flScale);
		if (g.pBackend && g.clAtlas.GetTexture())
		{
			g.pBackend->DestroyTexture(g.clAtlas.GetTexture());
			g.clAtlas.SetTexture(0);
		}
		g.clAtlas.Clear();
		std::vector<stGlyphRange> aRanges(clFontAtlas::DefaultRanges().begin(), clFontAtlas::DefaultRanges().end());
		for (const stGlyphRange& rExtra : g.aExtraGlyphRanges)
			if (std::ranges::none_of(clFontAtlas::DefaultRanges(), [&](const stGlyphRange& rDefault) { return rExtra.uFirst >= rDefault.uFirst && rExtra.uLast <= rDefault.uLast; }))
				aRanges.push_back(rExtra);
		clFont* pFont = nullptr;
		if (!g.stSettings.pathFont.empty())
			pFont = g.clAtlas.AddFontFromFile(g.stSettings.pathFont, flSize, aRanges);
		if (!pFont)
			pFont = g.clAtlas.AddDefaultMonospace(flSize, aRanges);
		if (!pFont)
		{
			g.strLastError = "no usable monospace font (tried pathFont, CascadiaMono, Consolas, Lucida Console, Courier New)";
			return false;
		}
		if (!g.clAtlas.Build())
		{
			g.strLastError = "font atlas build failed";
			return false;
		}
		if (g.pBackend)
		{
			g.clAtlas.SetTexture(g.pBackend->CreateTexture(g.clAtlas.GetPixelsRGBA().data(), g.clAtlas.GetWidth(), g.clAtlas.GetHeight()));
			if (!g.clAtlas.GetTexture())
			{
				g.strLastError = std::format("{} backend failed to create the {}x{} font texture", g.pBackend->GetName(), g.clAtlas.GetWidth(), g.clAtlas.GetHeight());
				return false;
			}
		}
		g.stShared.pAtlas = &g.clAtlas;
		g.stShared.pFont = pFont;
		g.stShared.flFontSize = flSize;
		g.stCurrentStyle = g.stBaseStyle;
		g.stCurrentStyle.ScaleAllSizes(flScale);
		g.stCurrentStyle.flFontSize = flSize;
		return true;
	}

	void SetGlyphRanges(std::span<const stGlyphRange> aRanges)
	{
		if (!g_pContext)
			SetCurrentContext(CreateContext());
		stContext& g = Ctx();
		const auto fnEqual = [](const stGlyphRange& rLeft, const stGlyphRange& rRight) { return rLeft.uFirst == rRight.uFirst && rLeft.uLast == rRight.uLast; };
		if (std::ranges::equal(g.aExtraGlyphRanges, aRanges, fnEqual))
			return;
		g.aExtraGlyphRanges.assign(aRanges.begin(), aRanges.end());
		g.bPendingFontRebuild = g.bInitialized;
	}

	void SetScale(float flScale)
	{
		Ctx().flPendingScale = std::max(flScale, .25f);
	}

	void SetBackend(clRenderBackend* pBackend)
	{
		stContext& g = Ctx();
		g.pBackend = pBackend;
		g.clAtlas.SetTexture(0);
		if (pBackend && g.clAtlas.IsBuilt())
			g.clAtlas.SetTexture(pBackend->CreateTexture(g.clAtlas.GetPixelsRGBA().data(), g.clAtlas.GetWidth(), g.clAtlas.GetHeight()));
	}

	bool Initialize(clRenderBackend* pBackend, void* hWindow, const stConfig& stSettings)
	{
		if (!g_pContext)
			SetCurrentContext(CreateContext());
		stContext& g = Ctx();
		g.pBackend = pBackend;
		g.hWindow = hWindow;
		g.stSettings = stSettings;
		g.bVisible = stSettings.bStartVisible;
		g.stBaseStyle.aColors = g.stCurrentStyle.aColors;

		LARGE_INTEGER stFrequency{}, stCounter{};
		QueryPerformanceFrequency(&stFrequency);
		QueryPerformanceCounter(&stCounter);
		g.iTickFrequency = stFrequency.QuadPart;
		g.iLastTicks = stCounter.QuadPart;

		if (!RebuildFonts())
			return false;
		g.clGamepads.Initialize(stSettings.bEnableXInput, stSettings.bEnablePlayStation);
		if (!stSettings.pathSettings.empty())
			LoadSettings(stSettings.pathSettings);
		g.bInitialized = true;
		return true;
	}

	void Shutdown()
	{
		if (!g_pContext)
			return;
		stContext& g = Ctx();
		if (!g.stSettings.pathSettings.empty())
			SaveSettings(g.stSettings.pathSettings);
		g.clGamepads.Shutdown();
		if (g.pBackend && g.clAtlas.GetTexture())
			g.pBackend->DestroyTexture(g.clAtlas.GetTexture());
		g.clAtlas.SetTexture(0);
		g.bInitialized = false;
		DestroyContext(g_pContext);
	}

	bool ProcessWin32Message(void* hWindow, std::uint32_t uMessage, std::uintptr_t uWParam, std::intptr_t iLParam)
	{
		if (!g_pContext)
			return false;
		stContext& g = Ctx();
		clInput& rInput = g.clInputState;
		const HWND hWnd = static_cast<HWND>(hWindow);
		const bool bBlockAll = g.bVisible && g.stSettings.bBlockInputWhenVisible;

		auto MouseButtonFromMessage = [&](UINT uMsg) -> std::optional<EMouseButton> {
			switch (uMsg)
			{
			case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK: return EMouseButton::Left;
			case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK: return EMouseButton::Right;
			case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK: return EMouseButton::Middle;
			case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK: return GET_XBUTTON_WPARAM(uWParam) == XBUTTON1 ? EMouseButton::X1 : EMouseButton::X2;
			default: return std::nullopt;
			}
		};

		switch (uMessage)
		{
		case WM_MOUSEMOVE:
			rInput.AddMousePositionEvent({ static_cast<float>(GET_X_LPARAM(iLParam)), static_cast<float>(GET_Y_LPARAM(iLParam)) });
			return bBlockAll || g.bWantsMouse;
		case WM_MOUSELEAVE:
			rInput.AddMousePositionEvent({ -1e6f, -1e6f });
			return false;
		case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
			if (const auto optButton = MouseButtonFromMessage(uMessage))
			{
				rInput.AddMouseButtonEvent(*optButton, true);
				if (hWnd && GetCapture() == nullptr && (bBlockAll || g.bWantsMouse))
					SetCapture(hWnd);
			}
			return bBlockAll || g.bWantsMouse;
		case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: case WM_XBUTTONUP:
			if (const auto optButton = MouseButtonFromMessage(uMessage))
			{
				rInput.AddMouseButtonEvent(*optButton, false);
				if (hWnd && GetCapture() == hWnd)
					ReleaseCapture();
			}
			return bBlockAll || g.bWantsMouse;
		case WM_MOUSEWHEEL:
			rInput.AddMouseWheelEvent(static_cast<float>(GET_WHEEL_DELTA_WPARAM(uWParam)) / WHEEL_DELTA);
			return bBlockAll || g.bWantsMouse;
		case WM_MOUSEHWHEEL:
			rInput.AddMouseWheelEvent(0.f, static_cast<float>(GET_WHEEL_DELTA_WPARAM(uWParam)) / WHEEL_DELTA);
			return bBlockAll || g.bWantsMouse;
		case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_KEYUP: case WM_SYSKEYUP:
		{
			const bool bDown = uMessage == WM_KEYDOWN || uMessage == WM_SYSKEYDOWN;
			const auto uKey = static_cast<KeyCode>(uWParam & 0xFF);
			rInput.AddKeyEvent(uKey, bDown);
			if (uKey == VK_SHIFT || uKey == VK_CONTROL || uKey == VK_MENU)
			{
				const bool bExtended = (HIWORD(iLParam) & KF_EXTENDED) != 0;
				if (uKey == VK_SHIFT)
					rInput.AddKeyEvent(static_cast<KeyCode>(MapVirtualKeyW((static_cast<UINT>(iLParam) >> 16) & 0xFF, MAPVK_VSC_TO_VK_EX)), bDown);
				else if (uKey == VK_CONTROL)
					rInput.AddKeyEvent(bExtended ? VK_RCONTROL : VK_LCONTROL, bDown);
				else
					rInput.AddKeyEvent(bExtended ? VK_RMENU : VK_LMENU, bDown);
			}
			if (uKey == g.stSettings.uToggleKey)
			{
				if (bDown && (HIWORD(iLParam) & KF_REPEAT) == 0)
					ToggleVisible();
				return true;
			}
			return bBlockAll || g.bWantsKeyboard;
		}
		case WM_CHAR:
		{
			static wchar_t s_chHighSurrogate = 0;
			const auto chValue = static_cast<wchar_t>(uWParam);
			if (IS_HIGH_SURROGATE(chValue))
				s_chHighSurrogate = chValue;
			else if (IS_LOW_SURROGATE(chValue) && s_chHighSurrogate)
			{
				rInput.AddCharacterEvent(0x10000 + ((static_cast<char32_t>(s_chHighSurrogate) - 0xD800) << 10) + (chValue - 0xDC00));
				s_chHighSurrogate = 0;
			}
			else
				rInput.AddCharacterEvent(chValue);
			return bBlockAll || g.bWantsKeyboard;
		}
		case WM_SETFOCUS:
			rInput.AddFocusEvent(true);
			return false;
		case WM_KILLFOCUS:
			rInput.AddFocusEvent(false);
			return false;
		case WM_SETCURSOR:
			if (g.bVisible && LOWORD(iLParam) == HTCLIENT && (g.bWantsMouse || bBlockAll))
			{
				LPCWSTR szCursor = IDC_ARROW;
				switch (g.eCursorPrevious)
				{
				case EMouseCursor::TextInput: szCursor = IDC_IBEAM; break;
				case EMouseCursor::Hand: szCursor = IDC_HAND; break;
				case EMouseCursor::ResizeAll: szCursor = IDC_SIZEALL; break;
				case EMouseCursor::ResizeEW: szCursor = IDC_SIZEWE; break;
				case EMouseCursor::ResizeNS: szCursor = IDC_SIZENS; break;
				case EMouseCursor::ResizeNWSE: szCursor = IDC_SIZENWSE; break;
				case EMouseCursor::ResizeNESW: szCursor = IDC_SIZENESW; break;
				case EMouseCursor::NotAllowed: szCursor = IDC_NO; break;
				default: break;
				}
				SetCursor(g.eCursorPrevious == EMouseCursor::Hidden || g.stSettings.bDrawSoftwareCursor ? nullptr : LoadCursorW(nullptr, szCursor));
				return true;
			}
			return false;
		case WM_INPUT:
			return bBlockAll;
		default:
			return false;
		}
	}

	stWindow* FindWindowByID(ID uID)
	{
		const auto itWindow = Ctx().mapWindows.find(uID);
		return itWindow != Ctx().mapWindows.end() ? itWindow->second.get() : nullptr;
	}

	stWindow* FindWindowByName(std::string_view strName)
	{
		return FindWindowByID(HashString(strName));
	}

	void BringWindowToFront(stWindow* pWindow)
	{
		stContext& g = Ctx();
		if (!pWindow || HasFlag(pWindow->eFlags, EWindowFlags::NoBringToFront))
			return;
		auto itWindow = std::ranges::find(g.aWindowOrder, pWindow);
		if (itWindow == g.aWindowOrder.end() || itWindow + 1 == g.aWindowOrder.end())
			return;
		g.aWindowOrder.erase(itWindow);
		g.aWindowOrder.push_back(pWindow);
	}

	void FocusWindowInternal(stWindow* pWindow)
	{
		stContext& g = Ctx();
		if (g.pFocusedWindow == pWindow)
		{
			BringWindowToFront(pWindow);
			return;
		}
		g.pFocusedWindow = pWindow;
		if (pWindow && !HasFlag(pWindow->eFlags, EWindowFlags::NoNav))
		{
			if (g.pNavWindow && g.pNavWindow != pWindow)
				g.pNavWindow->uNavLastID = g.uNavID;
			g.pNavWindow = pWindow;
			g.uNavID = pWindow->uNavLastID;
		}
		BringWindowToFront(pWindow);
	}

	void FocusWindow(std::string_view strName)
	{
		if (stWindow* pWindow = FindWindowByName(strName))
			FocusWindowInternal(pWindow);
		else
			Ctx().stNext.bFocus = true;
	}

	void MarkSettingsDirty()
	{
		stContext& g = Ctx();
		if (!g.bSettingsDirty)
			g.flSettingsTimer = g.stSettings.flSettingsSaveInterval;
		g.bSettingsDirty = true;
	}

	void ApplySettings(stWindow* pWindow)
	{
		stContext& g = Ctx();
		const auto itSettings = g.mapSettings.find(pWindow->uID);
		if (itSettings == g.mapSettings.end())
			return;
		const stWindowSettings& rSettings = itSettings->second;
		if (rSettings.bHasPosition)
			pWindow->vecPosition = rSettings.vecPosition;
		if (rSettings.bHasSize)
			pWindow->vecSize = pWindow->vecSizeFull = rSettings.vecSize;
		pWindow->bCollapsed = rSettings.bCollapsed;
		pWindow->flCollapse = rSettings.bCollapsed ? 1.f : 0.f;
	}

	bool LoadSettings(const std::filesystem::path& pathFile)
	{
		stContext& g = Ctx();
		const std::filesystem::path pathSource = pathFile.empty() ? g.stSettings.pathSettings : pathFile;
		std::ifstream fileSettings(pathSource);
		if (!fileSettings)
			return false;

		stWindowSettings* pCurrent = nullptr;
		stRainbowState* pRainbow = nullptr;
		std::string strLine;
		while (std::getline(fileSettings, strLine))
		{
			if (!strLine.empty() && strLine.back() == '\r')
				strLine.pop_back();
			if (strLine.starts_with("[Window][") && strLine.ends_with("]"))
			{
				const std::string strName = strLine.substr(9, strLine.size() - 10);
				pCurrent = &g.mapSettings[HashString(strName)];
				pCurrent->strName = strName;
				pRainbow = nullptr;
				continue;
			}
			unsigned int uRainbowID = 0;
			if (sscanf_s(strLine.c_str(), "[Rainbow][%x]", &uRainbowID) == 1)
			{
				pRainbow = &g.mapRainbows[static_cast<ID>(uRainbowID)];
				pCurrent = nullptr;
				continue;
			}
			float flX = 0.f, flY = 0.f;
			int iValue = 0;
			if (pRainbow)
			{
				if (sscanf_s(strLine.c_str(), "Enabled=%d", &iValue) == 1)
					pRainbow->bEnabled = iValue != 0;
				else if (sscanf_s(strLine.c_str(), "Speed=%f", &flX) == 1)
					pRainbow->flSpeed = flX;
				else if (sscanf_s(strLine.c_str(), "Saturation=%f", &flX) == 1)
					pRainbow->flSaturation = flX;
				else if (sscanf_s(strLine.c_str(), "Value=%f", &flX) == 1)
					pRainbow->flValue = flX;
				continue;
			}
			if (!pCurrent)
				continue;
			if (sscanf_s(strLine.c_str(), "Pos=%f,%f", &flX, &flY) == 2)
			{
				pCurrent->vecPosition = { flX, flY };
				pCurrent->bHasPosition = true;
			}
			else if (sscanf_s(strLine.c_str(), "Size=%f,%f", &flX, &flY) == 2)
			{
				pCurrent->vecSize = { flX, flY };
				pCurrent->bHasSize = true;
			}
			else if (sscanf_s(strLine.c_str(), "Collapsed=%d", &iValue) == 1)
				pCurrent->bCollapsed = iValue != 0;
		}
		for (auto& [uID, pWindow] : g.mapWindows)
			ApplySettings(pWindow.get());
		g.bSettingsLoaded = true;
		return true;
	}

	bool SaveSettings(const std::filesystem::path& pathFile)
	{
		stContext& g = Ctx();
		const std::filesystem::path pathTarget = pathFile.empty() ? g.stSettings.pathSettings : pathFile;
		if (pathTarget.empty())
			return false;

		for (auto& [uID, pWindow] : g.mapWindows)
		{
			if (HasFlag(pWindow->eFlags, EWindowFlags::NoSavedSettings) || HasFlag(pWindow->eFlags, EWindowFlags::Popup) || HasFlag(pWindow->eFlags, EWindowFlags::Tooltip))
				continue;
			stWindowSettings& rSettings = g.mapSettings[uID];
			rSettings.strName = pWindow->strName;
			rSettings.vecPosition = pWindow->bMaximized ? pWindow->rcRestore.min : pWindow->vecPosition;
			rSettings.vecSize = pWindow->bMaximized ? pWindow->rcRestore.Size() : pWindow->vecSizeFull;
			rSettings.bCollapsed = pWindow->bCollapsed;
			rSettings.bHasPosition = rSettings.bHasSize = true;
		}

		std::error_code errCode;
		if (pathTarget.has_parent_path())
			std::filesystem::create_directories(pathTarget.parent_path(), errCode);
		std::ofstream fileSettings(pathTarget, std::ios::trunc);
		if (!fileSettings)
			return false;
		for (const auto& [uID, rSettings] : g.mapSettings)
		{
			fileSettings << "[Window][" << rSettings.strName << "]\n";
			if (rSettings.bHasPosition)
				fileSettings << std::format("Pos={:.0f},{:.0f}\n", rSettings.vecPosition.x, rSettings.vecPosition.y);
			if (rSettings.bHasSize)
				fileSettings << std::format("Size={:.0f},{:.0f}\n", rSettings.vecSize.x, rSettings.vecSize.y);
			fileSettings << "Collapsed=" << (rSettings.bCollapsed ? 1 : 0) << "\n\n";
		}
		for (const auto& [uID, rRainbow] : g.mapRainbows)
		{
			if (!rRainbow.bEnabled)
				continue;
			fileSettings << std::format("[Rainbow][{:08X}]\nEnabled=1\nSpeed={:.3f}\nSaturation={:.3f}\nValue={:.3f}\n\n", uID, rRainbow.flSpeed, rRainbow.flSaturation, rRainbow.flValue);
		}
		g.bSettingsDirty = false;
		return true;
	}

	void ProcessSettingsTimer()
	{
		stContext& g = Ctx();
		if (!g.bSettingsDirty || g.stSettings.pathSettings.empty())
			return;
		g.flSettingsTimer -= g.flDeltaTime;
		if (g.flSettingsTimer <= 0.f)
			SaveSettings(g.stSettings.pathSettings);
	}

	color RainbowColor(const stRainbowState& rState, std::uint8_t byAlpha)
	{
		const double dHue = std::fmod(Ctx().dTime * static_cast<double>(rState.flSpeed), 1.0);
		return color::FromHSV(static_cast<float>(dHue < 0.0 ? dHue + 1.0 : dHue), rState.flSaturation, rState.flValue, byAlpha / 255.f);
	}

	void UpdateRainbows()
	{
		for (auto& [uID, rState] : Ctx().mapRainbows)
			if (rState.bEnabled && rState.pTarget)
				*rState.pTarget = RainbowColor(rState, rState.pTarget->A());
	}

	void NewFrame()
	{
		stContext& g = Ctx();
		if (g.bInFrame)
			EndFrame();

		if (g.flPendingScale > 0.f)
		{
			g.stSettings.flScale = g.flPendingScale;
			g.flPendingScale = 0.f;
			g.stBaseStyle.aColors = g.stCurrentStyle.aColors;
			g.stBaseStyle.eWindowControls = g.stCurrentStyle.eWindowControls;
			g.stBaseStyle.bDotGrid = g.stCurrentStyle.bDotGrid;
			g.stBaseStyle.bConsolePrefixes = g.stCurrentStyle.bConsolePrefixes;
			g.stBaseStyle.bAnimations = g.stCurrentStyle.bAnimations;
			g.bPendingFontRebuild = true;
		}
		if (g.bPendingFontRebuild)
		{
			g.bPendingFontRebuild = false;
			RebuildFonts();
		}

		LARGE_INTEGER stCounter{};
		QueryPerformanceCounter(&stCounter);
		g.flDeltaTime = g.iTickFrequency ? static_cast<float>(static_cast<double>(stCounter.QuadPart - g.iLastTicks) / static_cast<double>(g.iTickFrequency)) : 1.f / 60.f;
		g.flDeltaTime = std::clamp(g.flDeltaTime, 1e-5f, .25f);
		g.iLastTicks = stCounter.QuadPart;
		g.dTime += g.flDeltaTime;
		++g.iFrame;
		UpdateRainbows();

		const HWND hWnd = static_cast<HWND>(g.hWindow);
		if (hWnd && !g.bDisplaySizeOverride)
		{
			RECT stClient{};
			GetClientRect(hWnd, &stClient);
			g.vecDisplaySize = { static_cast<float>(stClient.right - stClient.left), static_cast<float>(stClient.bottom - stClient.top) };
		}
		if (hWnd && g.stSettings.bPollCursorPosition && GetForegroundWindow() == hWnd)
		{
			POINT stCursor{};
			if (GetCursorPos(&stCursor) && ScreenToClient(hWnd, &stCursor))
				g.clInputState.AddMousePositionEvent({ static_cast<float>(stCursor.x), static_cast<float>(stCursor.y) });
		}

		const bool bGamepadEnabled = g.stSettings.bEnableXInput || g.stSettings.bEnablePlayStation;
		if (!g.stSettings.bManualGamepad)
			g.clInputState.SetGamepadState(bGamepadEnabled && (!hWnd || GetForegroundWindow() == hWnd) ? g.clGamepads.Poll() : stGamepadState{});
		g.clInputState.NewFrame(g.flDeltaTime);
		clInput& rInput = g.clInputState;

		if (g.stSettings.bToggleWithGamepad)
		{
			const bool bComboDown = rInput.IsGamepadDown(EGamepadButton::Back) && rInput.IsGamepadDown(EGamepadButton::Start);
			if (bComboDown && (rInput.IsGamepadPressed(EGamepadButton::Back) || rInput.IsGamepadPressed(EGamepadButton::Start)))
				ToggleVisible();
		}

		g.stShared.rcFullClip = { { 0.f, 0.f }, g.vecDisplaySize };
		g.stShared.flFontSize = g.stCurrentStyle.flFontSize;
		g.clBackground.Reset(&g.stShared);
		g.clForeground.Reset(&g.stShared);
		g.clOverlay.Reset(&g.stShared);
		g.clWorld.Reset(&g.clAtlas, g.stShared.pFont, g.stCurrentStyle.flFontSize);

		g.flVisibility = Animate(HashString("##GlobalVisibility"), g.bVisible ? 1.f : 0.f, 18.f);
		g.eCursorPrevious = g.eCursor;
		g.eCursor = EMouseCursor::Arrow;

		g.uHoveredIDPrevious = g.uHoveredID;
		g.uHoveredID = 0;
		if (g.uHoveredIDPrevious && g.uHoveredIDPrevious == g.uHoverTimerID)
			g.flHoverTimer += g.flDeltaTime;
		else
		{
			g.uHoverTimerID = g.uHoveredIDPrevious;
			g.flHoverTimer = 0.f;
		}
		g.uActiveIDPrevious = g.uActiveID;
		if (g.uActiveID && !g.bActiveIDAlive && !g.bActiveIDJustActivated)
			ClearActiveID();
		if (g.stText.uID && g.uActiveID != g.stText.uID)
			g.stText = {};
		if (g.uHotkeyCapture && g.uActiveID != g.uHotkeyCapture)
			g.uHotkeyCapture = 0;
		g.bActiveIDAlive = false;
		g.bActiveIDJustActivated = false;

		g.uWheelRegion = g.uWheelRegionNext;
		g.pWheelWindow = g.pWheelWindowNext;
		g.uWheelRegionNext = 0;
		g.pWheelWindowNext = nullptr;

		g.pHoveredWindow = nullptr;
		if (g.bVisible && rInput.IsMouseValid())
		{
			std::size_t uModalFloor = 0;
			for (std::size_t i = 0; i < g.aWindowOrder.size(); ++i)
				if (HasFlag(g.aWindowOrder[i]->eFlags, EWindowFlags::Modal) && g.aWindowOrder[i]->bWasActive)
					uModalFloor = i;
			for (std::size_t i = g.aWindowOrder.size(); i-- > uModalFloor;)
			{
				stWindow* pWindow = g.aWindowOrder[i];
				if (!pWindow->bWasActive || pWindow->bHiddenThisFrame || HasFlag(pWindow->eFlags, EWindowFlags::NoInputs))
					continue;
				const rect rcHit = pWindow->Rect().Expanded(HasFlag(pWindow->eFlags, EWindowFlags::NoResize) ? 0.f : g.stCurrentStyle.flResizeBorder * .5f);
				if (rcHit.Contains(rInput.GetMousePosition()))
				{
					g.pHoveredWindow = pWindow;
					break;
				}
			}
		}

		for (auto& [uID, pWindow] : g.mapWindows)
		{
			pWindow->bWasActive = pWindow->bActive;
			pWindow->bActive = false;
			pWindow->aNavItemsPrevious.swap(pWindow->aNavItems);
			pWindow->aNavItems.clear();
			pWindow->uFirstTabBarIDPrevious = pWindow->uFirstTabBarID;
			pWindow->uFirstTabBarID = 0;
		}

		if (g.bVisible)
		{
			UpdateWindowsInteraction();
			UpdateNavigation();
		}
		else
		{
			g.uNavActivateID = 0;
			g.iNavAdjust = g.iNavAdjustVertical = 0;
		}

		g.aWindowStack.clear();
		g.pCurrentWindow = nullptr;
		g.bTooltipRequested = false;
		g.bNavRequestFocusHere = false;
		g.bInFrame = true;
		ProcessSettingsTimer();
	}

	void UpdateWindowsInteraction()
	{
		stContext& g = Ctx();
		clInput& rInput = g.clInputState;
		const bool bAnyClick = rInput.IsMouseClicked(EMouseButton::Left) || rInput.IsMouseClicked(EMouseButton::Right) || rInput.IsMouseClicked(EMouseButton::Middle);
		const bool bStickyActive = g.uActiveID && (g.uActiveID == g.stText.uID || g.uActiveID == g.uHotkeyCapture);
		if (bAnyClick && bStickyActive && !g.stText.bOnScreenKeyboard && !g.rcStickyActive.Contains(rInput.GetMousePosition()))
		{
			ClearActiveID();
			g.stText = {};
			g.uHotkeyCapture = 0;
		}
		if (bAnyClick && !g.uActiveID)
		{
			if (g.pHoveredWindow)
			{
				FocusWindowInternal(g.pHoveredWindow);
				ClosePopupsOverWindow(g.pHoveredWindow);
			}
			else
			{
				g.pFocusedWindow = nullptr;
				ClosePopupsOverWindow(nullptr);
			}
			g.bNavVisible = false;
		}

		if (g.pMovingWindow)
		{
			if (rInput.IsMouseDown(EMouseButton::Left))
			{
				stWindow* pWindow = g.pMovingWindow;
				vec2 vecTarget = rInput.GetMousePosition() - g.vecMoveOffset;
				if (pWindow->bMaximized)
				{
					pWindow->bMaximized = false;
					const vec2 vecRestoreSize = pWindow->rcRestore.Size();
					g.vecMoveOffset.x = std::clamp(g.vecMoveOffset.x, 0.f, vecRestoreSize.x - 40.f);
					pWindow->vecSize = pWindow->vecSizeFull = vecRestoreSize;
					vecTarget = rInput.GetMousePosition() - g.vecMoveOffset;
				}
				if (!HasFlag(pWindow->eFlags, EWindowFlags::NoSnap) && !rInput.IsShiftDown())
				{
					const float flSnap = g.stCurrentStyle.flSnapDistance;
					const vec2 vecSize = pWindow->vecSize;
					auto SnapAxis = [&](float& rValue, float flExtent, float flEdge) {
						if (std::abs(rValue - flEdge) < flSnap)
							rValue = flEdge;
						else if (std::abs(rValue + flExtent - flEdge) < flSnap)
							rValue = flEdge - flExtent;
					};
					SnapAxis(vecTarget.x, vecSize.x, 0.f);
					SnapAxis(vecTarget.x, vecSize.x, g.vecDisplaySize.x);
					SnapAxis(vecTarget.y, vecSize.y, 0.f);
					SnapAxis(vecTarget.y, vecSize.y, g.vecDisplaySize.y);
					for (const stWindow* pOther : g.aWindowOrder)
					{
						if (pOther == pWindow || !pOther->bWasActive || HasFlag(pOther->eFlags, EWindowFlags::Popup | EWindowFlags::Tooltip | EWindowFlags::NoInputs))
							continue;
						const rect rcOther = pOther->Rect();
						const bool bOverlapY = vecTarget.y < rcOther.max.y + flSnap && vecTarget.y + vecSize.y > rcOther.min.y - flSnap;
						const bool bOverlapX = vecTarget.x < rcOther.max.x + flSnap && vecTarget.x + vecSize.x > rcOther.min.x - flSnap;
						if (bOverlapY)
						{
							SnapAxis(vecTarget.x, vecSize.x, rcOther.min.x);
							SnapAxis(vecTarget.x, vecSize.x, rcOther.max.x);
						}
						if (bOverlapX)
						{
							SnapAxis(vecTarget.y, vecSize.y, rcOther.min.y);
							SnapAxis(vecTarget.y, vecSize.y, rcOther.max.y);
						}
					}
				}
				if (pWindow->vecPosition != vecTarget)
				{
					pWindow->vecPosition = vecTarget;
					MarkSettingsDirty();
				}
				g.eCursor = EMouseCursor::ResizeAll;
			}
			else
				g.pMovingWindow = nullptr;
		}

		if (g.pResizingWindow)
		{
			if (rInput.IsMouseDown(EMouseButton::Left))
			{
				stWindow* pWindow = g.pResizingWindow;
				const vec2 vecDelta = rInput.GetMousePosition() - g.vecResizeMouseStart;
				rect rcNew = g.rcResizeStart;
				if (g.iResizeEdges & 1) rcNew.min.x += vecDelta.x;
				if (g.iResizeEdges & 2) rcNew.max.x += vecDelta.x;
				if (g.iResizeEdges & 4) rcNew.min.y += vecDelta.y;
				if (g.iResizeEdges & 8) rcNew.max.y += vecDelta.y;
				const vec2 vecMin = Max(g.stCurrentStyle.vecWindowMinSize, pWindow->vecSizeMin);
				const vec2 vecMax = pWindow->vecSizeMax;
				if (rcNew.Width() < vecMin.x) { if (g.iResizeEdges & 1) rcNew.min.x = rcNew.max.x - vecMin.x; else rcNew.max.x = rcNew.min.x + vecMin.x; }
				if (rcNew.Height() < vecMin.y) { if (g.iResizeEdges & 4) rcNew.min.y = rcNew.max.y - vecMin.y; else rcNew.max.y = rcNew.min.y + vecMin.y; }
				if (rcNew.Width() > vecMax.x) { if (g.iResizeEdges & 1) rcNew.min.x = rcNew.max.x - vecMax.x; else rcNew.max.x = rcNew.min.x + vecMax.x; }
				if (rcNew.Height() > vecMax.y) { if (g.iResizeEdges & 4) rcNew.min.y = rcNew.max.y - vecMax.y; else rcNew.max.y = rcNew.min.y + vecMax.y; }
				pWindow->vecPosition = rcNew.min;
				pWindow->vecSize = pWindow->vecSizeFull = rcNew.Size();
				pWindow->bMaximized = false;
				MarkSettingsDirty();
				static constexpr EMouseCursor s_aCursors[16] = {
					EMouseCursor::Arrow, EMouseCursor::ResizeEW, EMouseCursor::ResizeEW, EMouseCursor::Arrow,
					EMouseCursor::ResizeNS, EMouseCursor::ResizeNWSE, EMouseCursor::ResizeNESW, EMouseCursor::Arrow,
					EMouseCursor::ResizeNS, EMouseCursor::ResizeNESW, EMouseCursor::ResizeNWSE, EMouseCursor::Arrow,
					EMouseCursor::Arrow, EMouseCursor::Arrow, EMouseCursor::Arrow, EMouseCursor::Arrow,
				};
				g.eCursor = s_aCursors[g.iResizeEdges & 15];
			}
			else
				g.pResizingWindow = nullptr;
		}
	}

	void ClosePopupToLevel(std::size_t uLevel)
	{
		stContext& g = Ctx();
		if (uLevel >= g.aOpenPopups.size())
			return;
		g.aOpenPopups.resize(uLevel);
		if (g.uActiveID && g.pActiveWindow && HasFlag(g.pActiveWindow->eFlags, EWindowFlags::Popup) && !std::ranges::any_of(g.aOpenPopups, [&](const stPopup& rPopup) { return rPopup.pWindow == g.pActiveWindow; }))
			ClearActiveID();
	}

	void ClosePopupsOverWindow(stWindow* pWindow)
	{
		stContext& g = Ctx();
		if (g.aOpenPopups.empty())
			return;
		if (!pWindow)
		{
			if (!g.aOpenPopups.back().bModal)
			{
				std::size_t uKeep = 0;
				for (std::size_t i = 0; i < g.aOpenPopups.size(); ++i)
					if (g.aOpenPopups[i].bModal)
						uKeep = i + 1;
				ClosePopupToLevel(uKeep);
			}
			return;
		}
		for (std::size_t i = 0; i < g.aOpenPopups.size(); ++i)
			if (g.aOpenPopups[i].pWindow == pWindow)
			{
				ClosePopupToLevel(i + 1);
				return;
			}
		const vec2 vecMouse = g.clInputState.GetMousePosition();
		std::size_t uKeep = 0;
		for (std::size_t i = 0; i < g.aOpenPopups.size(); ++i)
			if (g.aOpenPopups[i].bModal || (g.aOpenPopups[i].uParentWindowID == pWindow->uID && g.aOpenPopups[i].rcAnchor.Contains(vecMouse)))
				uKeep = i + 1;
		if (uKeep < g.aOpenPopups.size() && !g.aOpenPopups[uKeep].bModal)
			ClosePopupToLevel(uKeep);
	}

	void UpdateNavigation()
	{
		stContext& g = Ctx();
		clInput& rInput = g.clInputState;
		g.uNavActivateID = 0;
		g.iNavAdjust = 0;
		g.iNavAdjustVertical = 0;
		g.bNavCancel = false;

		if (g.uNavPendingActivate)
		{
			g.uNavActivateID = g.uNavPendingActivate;
			g.uNavPendingActivate = 0;
		}

		const bool bTextActive = g.stText.uID != 0 && !g.stText.bOnScreenKeyboard;
		const bool bKeyboard = g.stSettings.bKeyboardNavigation && !bTextActive && g.uHotkeyCapture == 0;
		const bool bGamepad = g.stSettings.bGamepadNavigation && rInput.GetGamepad().bConnected && g.uHotkeyCapture == 0;

		auto Pressed = [&](KeyCode uKey, EGamepadButton eButton, bool bRepeat) {
			return (bKeyboard && uKey && rInput.IsKeyPressed(uKey, bRepeat)) || (bGamepad && rInput.IsGamepadPressed(eButton, bRepeat));
		};

		g.bGamepadWindowManip = bGamepad && rInput.IsGamepadDown(EGamepadButton::Back) && !rInput.IsGamepadDown(EGamepadButton::Start);
		if (g.bGamepadWindowManip && g.pNavWindow && g.pNavWindow->bWasActive)
		{
			stWindow* pWindow = g.pNavWindow;
			const stGamepadState& rPad = rInput.GetGamepad();
			const float flSpeed = 900.f * g.flDeltaTime;
			if (!HasFlag(pWindow->eFlags, EWindowFlags::NoMove) && rPad.vecLeftStick.LengthSqr() > 0.f)
			{
				pWindow->vecPosition += vec2{ rPad.vecLeftStick.x, -rPad.vecLeftStick.y } * flSpeed;
				pWindow->bMaximized = false;
				MarkSettingsDirty();
			}
			if (!HasFlag(pWindow->eFlags, EWindowFlags::NoResize) && rPad.vecRightStick.LengthSqr() > 0.f)
			{
				pWindow->vecSize = Max(pWindow->vecSize + vec2{ rPad.vecRightStick.x, -rPad.vecRightStick.y } * flSpeed, Max(g.stCurrentStyle.vecWindowMinSize, pWindow->vecSizeMin));
				pWindow->vecSizeFull = pWindow->vecSize;
				pWindow->bMaximized = false;
				MarkSettingsDirty();
			}
			g.bNavVisible = true;
			return;
		}

		if (bGamepad && (rInput.IsGamepadPressed(EGamepadButton::LeftTrigger) || rInput.IsGamepadPressed(EGamepadButton::RightTrigger)) ||
			(bKeyboard && rInput.IsCtrlDown() && rInput.IsKeyPressed(VK_TAB)))
		{
			const bool bBackward = rInput.IsGamepadPressed(EGamepadButton::LeftTrigger) || rInput.IsShiftDown();
			std::vector<stWindow*> aCandidates;
			for (stWindow* pWindow : g.aWindowOrder)
				if (pWindow->bWasActive && !HasFlag(pWindow->eFlags, EWindowFlags::NoNav | EWindowFlags::Popup | EWindowFlags::Tooltip | EWindowFlags::NoInputs))
					aCandidates.push_back(pWindow);
			std::ranges::sort(aCandidates, {}, [](const stWindow* pWindow) { return pWindow->uID; });
			if (!aCandidates.empty())
			{
				auto itCurrent = std::ranges::find(aCandidates, g.pNavWindow);
				std::size_t uIndex = itCurrent == aCandidates.end() ? 0 : static_cast<std::size_t>(itCurrent - aCandidates.begin());
				uIndex = bBackward ? (uIndex + aCandidates.size() - 1) % aCandidates.size() : (uIndex + 1) % aCandidates.size();
				FocusWindowInternal(aCandidates[uIndex]);
				g.bNavVisible = true;
			}
		}

		if (!g.pNavWindow || !g.pNavWindow->bWasActive)
		{
			g.pNavWindow = nullptr;
			const bool bAnyNavInput = Pressed(VK_DOWN, EGamepadButton::DPadDown, false) || Pressed(VK_UP, EGamepadButton::DPadUp, false) ||
				Pressed(0, EGamepadButton::LeftStickDown, false) || Pressed(0, EGamepadButton::LeftStickUp, false) || Pressed(0, EGamepadButton::FaceDown, false);
			if (bAnyNavInput)
			{
				stWindow* pCandidate = g.pFocusedWindow && g.pFocusedWindow->bWasActive ? g.pFocusedWindow : nullptr;
				for (auto it = g.aWindowOrder.rbegin(); !pCandidate && it != g.aWindowOrder.rend(); ++it)
					if ((*it)->bWasActive && !HasFlag((*it)->eFlags, EWindowFlags::NoNav | EWindowFlags::Tooltip | EWindowFlags::NoInputs))
						pCandidate = *it;
				if (pCandidate)
				{
					FocusWindowInternal(pCandidate);
					if (!pCandidate->aNavItemsPrevious.empty())
						g.uNavID = pCandidate->aNavItemsPrevious.front().uID;
					g.bNavVisible = true;
				}
			}
			return;
		}

		stWindow* pWindow = g.pNavWindow;
		const std::vector<stNavItem>& aItems = pWindow->aNavItemsPrevious;
		const auto itCurrent = std::ranges::find(aItems, g.uNavID, &stNavItem::uID);
		if (itCurrent == aItems.end() && !aItems.empty() && g.uNavID != 0 && !g.uActiveID)
			g.uNavID = 0;
		const stNavItem* pCurrent = itCurrent != aItems.end() ? &*itCurrent : nullptr;

		if (Pressed(VK_ESCAPE, EGamepadButton::FaceRight, false))
		{
			g.bNavCancel = true;
			g.bNavVisible = true;
			if (!g.aOpenPopups.empty())
			{
				const ID uParent = g.aOpenPopups.back().uParentWindowID;
				ClosePopupToLevel(g.aOpenPopups.size() - 1);
				if (stWindow* pParent = FindWindowByID(uParent))
					FocusWindowInternal(pParent);
			}
			else if (g.uActiveID)
				ClearActiveID();
			return;
		}

		if (bGamepad && !HasFlag(pWindow->eFlags, EWindowFlags::NoNav))
		{
			if (rInput.IsGamepadPressed(EGamepadButton::LeftShoulder, true))
				UpdateTabBarGamepad(-1);
			else if (rInput.IsGamepadPressed(EGamepadButton::RightShoulder, true))
				UpdateTabBarGamepad(1);

			const vec2 vecScroll = rInput.GetGamepad().vecRightStick;
			if (std::abs(vecScroll.y) > 0.f)
			{
				g.uWheelRegion = pWindow->uID;
				g.pWheelWindow = pWindow;
				if (pCurrent)
				{
					g.uWheelRegion = pCurrent->uRegionID;
				}
				auto itScroll = pWindow->mapScroll.find(g.uWheelRegion);
				if (itScroll != pWindow->mapScroll.end())
				{
					stScrollState& rScroll = itScroll->second;
					rScroll.flTargetY = std::clamp((rScroll.flTargetY >= 0.f ? rScroll.flTargetY : rScroll.flScrollY) - vecScroll.y * 1100.f * g.flDeltaTime, 0.f, rScroll.flMaxY);
				}
				g.uWheelRegion = 0;
				g.pWheelWindow = nullptr;
			}
		}

		if (Pressed(VK_RETURN, EGamepadButton::FaceDown, false) || (bKeyboard && rInput.IsKeyPressed(VK_SPACE)))
		{
			if (pCurrent)
			{
				g.uNavActivateID = pCurrent->uID;
				g.bNavVisible = true;
			}
			else if (!aItems.empty())
			{
				g.uNavID = aItems.front().uID;
				g.bNavVisible = true;
			}
		}

		const bool bConsumesHorizontal = pCurrent && HasFlag(pCurrent->eFlags, EItemFlags::NavConsumesHorizontal);
		const bool bConsumesVertical = pCurrent && HasFlag(pCurrent->eFlags, EItemFlags::NavConsumesVertical) && g.uActiveID == pCurrent->uID;
		int iDirection = -1;
		const bool bLeft = Pressed(VK_LEFT, EGamepadButton::DPadLeft, true) || Pressed(0, EGamepadButton::LeftStickLeft, true);
		const bool bRight = Pressed(VK_RIGHT, EGamepadButton::DPadRight, true) || Pressed(0, EGamepadButton::LeftStickRight, true);
		const bool bUp = Pressed(VK_UP, EGamepadButton::DPadUp, true) || Pressed(0, EGamepadButton::LeftStickUp, true);
		const bool bDown = Pressed(VK_DOWN, EGamepadButton::DPadDown, true) || Pressed(0, EGamepadButton::LeftStickDown, true) || (bKeyboard && !rInput.IsCtrlDown() && rInput.IsKeyPressed(VK_TAB, true));

		if (bConsumesHorizontal && (bLeft || bRight))
		{
			g.iNavAdjust = bLeft ? -1 : 1;
			g.bNavVisible = true;
		}
		else if (bLeft)
			iDirection = 0;
		else if (bRight)
			iDirection = 1;

		if (bConsumesVertical && (bUp || bDown))
		{
			g.iNavAdjustVertical = bUp ? -1 : 1;
			g.bNavVisible = true;
		}
		else if (bUp)
			iDirection = 2;
		else if (bDown)
			iDirection = 3;

		if (iDirection < 0 || aItems.empty())
			return;

		g.bNavVisible = true;
		if (!pCurrent)
		{
			g.uNavID = iDirection == 2 ? aItems.back().uID : aItems.front().uID;
			return;
		}

		const vec2 vecFrom = pCurrent->rcItem.Center();
		const stNavItem* pBest = nullptr;
		float flBestScore = 1e30f;
		for (const stNavItem& rItem : aItems)
		{
			if (rItem.uID == pCurrent->uID)
				continue;
			const rect& rcItem = rItem.rcItem;
			const vec2 vecTo = rcItem.Center();
			float flPrimary = 0.f;
			float flSecondary = 0.f;
			bool bLaneOverlap = false;
			switch (iDirection)
			{
			case 0:
				if (rcItem.max.x > pCurrent->rcItem.min.x + 1.f) continue;
				flPrimary = vecFrom.x - vecTo.x;
				flSecondary = std::abs(vecTo.y - vecFrom.y);
				bLaneOverlap = rcItem.min.y < pCurrent->rcItem.max.y && rcItem.max.y > pCurrent->rcItem.min.y;
				break;
			case 1:
				if (rcItem.min.x < pCurrent->rcItem.max.x - 1.f) continue;
				flPrimary = vecTo.x - vecFrom.x;
				flSecondary = std::abs(vecTo.y - vecFrom.y);
				bLaneOverlap = rcItem.min.y < pCurrent->rcItem.max.y && rcItem.max.y > pCurrent->rcItem.min.y;
				break;
			case 2:
				if (rcItem.max.y > pCurrent->rcItem.min.y + 1.f) continue;
				flPrimary = vecFrom.y - vecTo.y;
				flSecondary = std::max(0.f, std::max(rcItem.min.x - pCurrent->rcItem.max.x, pCurrent->rcItem.min.x - rcItem.max.x));
				bLaneOverlap = rcItem.min.x < pCurrent->rcItem.max.x && rcItem.max.x > pCurrent->rcItem.min.x;
				break;
			default:
				if (rcItem.min.y < pCurrent->rcItem.max.y - 1.f) continue;
				flPrimary = vecTo.y - vecFrom.y;
				flSecondary = std::max(0.f, std::max(rcItem.min.x - pCurrent->rcItem.max.x, pCurrent->rcItem.min.x - rcItem.max.x));
				bLaneOverlap = rcItem.min.x < pCurrent->rcItem.max.x && rcItem.max.x > pCurrent->rcItem.min.x;
				break;
			}
			const float flScore = flPrimary + flSecondary * (bLaneOverlap ? .5f : 3.f) + (bLaneOverlap ? 0.f : 40.f);
			if (flScore < flBestScore)
			{
				flBestScore = flScore;
				pBest = &rItem;
			}
		}
		if (pBest)
		{
			g.uNavID = pBest->uID;
			ScrollToRect(pWindow, pBest->uRegionID, pBest->rcRegionClip, pBest->rcItem);
		}
	}

	void ScrollToRect(stWindow* pWindow, ID uRegionID, const rect& rcRegionClip, const rect& rcItem)
	{
		auto itScroll = pWindow->mapScroll.find(uRegionID);
		if (itScroll == pWindow->mapScroll.end())
			return;
		stScrollState& rScroll = itScroll->second;
		const float flMargin = Style().vecItemSpacing.y * 2.f;
		float flScroll = rScroll.flTargetY >= 0.f ? rScroll.flTargetY : rScroll.flScrollY;
		if (rcItem.min.y < rcRegionClip.min.y + flMargin)
			flScroll -= rcRegionClip.min.y + flMargin - rcItem.min.y;
		else if (rcItem.max.y > rcRegionClip.max.y - flMargin)
			flScroll += rcItem.max.y - (rcRegionClip.max.y - flMargin);
		rScroll.flTargetY = std::clamp(flScroll, 0.f, rScroll.flMaxY);
	}

	void PushToast(EToastKind eKind, std::string_view strText, float flDuration)
	{
		stContext& g = Ctx();
		g.aToasts.push_back({ eKind, std::string(strText), flDuration });
		if (g.aToasts.size() > 6)
			g.aToasts.erase(g.aToasts.begin());
	}

	void RenderToasts()
	{
		stContext& g = Ctx();
		const stStyle& rStyle = g.stCurrentStyle;
		clDrawList& rList = g.clOverlay;
		const float flWidth = std::min(380.f * g.stSettings.flScale, g.vecDisplaySize.x - 32.f);
		float flY = g.vecDisplaySize.y - 16.f;

		for (auto it = g.aToasts.rbegin(); it != g.aToasts.rend(); ++it)
		{
			stToast& rToast = *it;
			rToast.flAge += g.flDeltaTime;
			const float flIn = std::clamp(rToast.flAge / .18f, 0.f, 1.f);
			const float flOut = std::clamp((rToast.flDuration - rToast.flAge) / .25f, 0.f, 1.f);
			const float flAlpha = flIn * flOut;

			std::string_view strPrefix = "info: ";
			color colKind = rStyle.Get(EColor::Accent);
			switch (rToast.eKind)
			{
			case EToastKind::Success: strPrefix = "ok: "; colKind = rStyle.Get(EColor::Success); break;
			case EToastKind::Warning: strPrefix = "warn: "; colKind = rStyle.Get(EColor::Warning); break;
			case EToastKind::Error: strPrefix = "error: "; colKind = rStyle.Get(EColor::Danger); break;
			default: break;
			}

			const vec2 vecPadding{ 14.f * g.stSettings.flScale, 10.f * g.stSettings.flScale };
			const float flPrefixWidth = MeasureText(strPrefix).x;
			const vec2 vecText = MeasureText(rToast.strText, flWidth - vecPadding.x * 2.f - flPrefixWidth);
			const float flHeight = vecText.y + vecPadding.y * 2.f;
			const float flTargetY = flY - flHeight;
			if (rToast.flOffsetY < 0.f)
				rToast.flOffsetY = flTargetY;
			rToast.flOffsetY = ApproachExp(rToast.flOffsetY, flTargetY, 14.f, g.flDeltaTime);
			const float flSlide = (1.f - flIn) * 30.f;
			const vec2 vecMin{ g.vecDisplaySize.x - 16.f - flWidth + flSlide, rToast.flOffsetY };
			const vec2 vecMax{ vecMin.x + flWidth, vecMin.y + flHeight };

			rList.AddShadowRect(vecMin, vecMax, rStyle.Get(EColor::Shadow).MultiplyAlpha(flAlpha), rStyle.flShadowSize * .6f, rStyle.flRoundingSmall, { 0.f, 6.f });
			rList.AddRectFilled(vecMin, vecMax, rStyle.Get(EColor::Surface).MultiplyAlpha(flAlpha), rStyle.flRoundingSmall);
			rList.AddRectFilled(vecMin, vecMax, colKind.WithAlpha(18).MultiplyAlpha(flAlpha), rStyle.flRoundingSmall);
			rList.AddRect(vecMin, vecMax, rStyle.Get(EColor::Border).MultiplyAlpha(flAlpha), rStyle.flRoundingSmall);
			rList.AddRectFilled(vecMin, { vecMin.x + 3.f, vecMax.y }, colKind.MultiplyAlpha(flAlpha), rStyle.flRoundingSmall, ECorner::Left);
			rList.AddText(vecMin + vecPadding, colKind.MultiplyAlpha(flAlpha), strPrefix);
			rList.AddText(vecMin + vecPadding + vec2{ flPrefixWidth, 0.f }, rStyle.Get(EColor::Text).MultiplyAlpha(flAlpha), rToast.strText, 0.f, flWidth - vecPadding.x * 2.f - flPrefixWidth);

			const float flProgress = std::clamp(1.f - rToast.flAge / rToast.flDuration, 0.f, 1.f);
			rList.AddRectFilled({ vecMin.x + 3.f, vecMax.y - 2.f }, { vecMin.x + 3.f + (flWidth - 3.f) * flProgress, vecMax.y }, colKind.WithAlpha(120).MultiplyAlpha(flAlpha));

			flY = flTargetY - 8.f;
		}
		std::erase_if(g.aToasts, [](const stToast& rToast) { return rToast.flAge >= rToast.flDuration; });
	}

	void RenderSoftwareCursor()
	{
		stContext& g = Ctx();
		if (!g.stSettings.bDrawSoftwareCursor || !g.bVisible || !g.clInputState.IsMouseValid() || g.eCursor == EMouseCursor::Hidden)
			return;
		const vec2 vecPosition = g.clInputState.GetMousePosition().Floor();
		const float flScale = g.stSettings.flScale;
		clDrawList& rList = g.clOverlay;
		const color colFill = g.stCurrentStyle.Get(EColor::Accent);
		const color colOutline = g.stCurrentStyle.Get(EColor::Background);
		if (g.eCursor == EMouseCursor::TextInput)
		{
			rList.AddRectFilled(vecPosition - vec2{ 1.f, 9.f * flScale }, vecPosition + vec2{ 2.f, 9.f * flScale }, colOutline);
			rList.AddRectFilled(vecPosition - vec2{ 0.f, 8.f * flScale }, vecPosition + vec2{ 1.f, 8.f * flScale }, colFill);
			return;
		}
		const vec2 aArrow[] = {
			vecPosition,
			vecPosition + vec2{ 0.f, 17.f } * flScale,
			vecPosition + vec2{ 4.5f, 12.5f } * flScale,
			vecPosition + vec2{ 7.5f, 19.f } * flScale,
			vecPosition + vec2{ 10.f, 18.f } * flScale,
			vecPosition + vec2{ 7.f, 11.5f } * flScale,
			vecPosition + vec2{ 12.5f, 11.5f } * flScale,
		};
		rList.AddTriangleFilled(aArrow[0], aArrow[1], aArrow[6], colFill);
		rList.AddQuadFilled(aArrow[2], aArrow[3], aArrow[4], aArrow[5], colFill);
		rList.AddPolyline(aArrow, colOutline, true, 1.5f);
	}

	void EndFrame()
	{
		stContext& g = Ctx();
		if (!g.bInFrame)
			return;
		if (g.bVisible)
			RenderOnScreenKeyboard();
		while (!g.aWindowStack.empty())
			End();
		g.bInFrame = false;
	}

	void Render()
	{
		stContext& g = Ctx();
		EndFrame();

		RenderToasts();
		RenderSoftwareCursor();

		g.stData.Clear();
		g.stData.vecDisplayPosition = {};
		g.stData.vecDisplaySize = g.vecDisplaySize;
		g.stData.Add(&g.clBackground);
		for (stWindow* pWindow : g.aWindowOrder)
			if (pWindow->bActive && !pWindow->bHiddenThisFrame)
				g.stData.Add(&pWindow->drawList);
		g.stData.Add(&g.clForeground);
		g.stData.Add(&g.clOverlay);

		bool bAnyWindowHovered = g.pHoveredWindow != nullptr;
		g.bWantsMouse = g.bVisible && (bAnyWindowHovered || g.uActiveID != 0 || g.pMovingWindow || g.pResizingWindow || !g.aOpenPopups.empty());
		g.bWantsKeyboard = g.bVisible && (g.stText.uID != 0 || g.uHotkeyCapture != 0 || (g.stSettings.bKeyboardNavigation && g.pNavWindow && g.bNavVisible));
		g.bWantsGamepad = g.bVisible && g.stSettings.bGamepadNavigation;

		if (g.pBackend && g.clAtlas.GetTexture())
			g.pBackend->RenderDrawData(g.stData);
	}

	void RenderWorld(const stWorldPass& stPass)
	{
		stContext& g = Ctx();
		if (g.pBackend && !g.clWorld.IsEmpty())
			g.pBackend->RenderWorld(g.clWorld, stPass);
	}
}
