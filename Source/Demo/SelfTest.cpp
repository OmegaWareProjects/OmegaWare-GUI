#include "Demo/SelfTest.hpp"

#ifdef OMEGAGUI_DEMO_LOCALIZATION
#include "OmegaLocalization/OmegaLocalization.hpp"
#endif

#include <Windows.h>

#include <format>
#include <fstream>
#include <memory>

namespace
{
	[[nodiscard]] std::string ReadClipboard()
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
}


using namespace OmegaGUI;

namespace
{
	[[nodiscard]] bool Near(float flA, float flB, float flTolerance = 3.f)
	{
		return std::abs(flA - flB) <= flTolerance;
	}

	[[nodiscard]] std::optional<rect> FindRect(const std::string& strName)
	{
		const auto& rRects = DemoProbe().mapRects;
		const auto itRect = rRects.find(strName);
		if (itRect == rRects.end() || itRect->second.IsEmpty())
			return std::nullopt;
		return itRect->second;
	}
}

clSelfTest::clSelfTest(std::function<void(const std::string&)> fnScreenshot)
	: m_fnScreenshot(std::move(fnScreenshot))
{
	m_stPad.bConnected = true;
	m_stPad.eType = EGamepadType::Xbox;
	Add([this] { GetInput().SetGamepadState(m_stPad); }, 2);

	Click("tab_widgets");
	Check("mouse click switches sidebar tab", [](const stDemoProbe& rProbe) { return rProbe.strSidebarTab == "widgets"; }, [](const stDemoProbe& rProbe) { return rProbe.strSidebarTab; });

	Click("cb_feature_b");
	Check("mouse click toggles checkbox", [](const stDemoProbe& rProbe) { return rProbe.bFeatureB; });

	Click("input_name", { .8f, .5f });
	Key('A', true);
	Type("tester");
	Key(VK_RETURN);
	Check("text input with select-all and typing", [](const stDemoProbe& rProbe) { return rProbe.strName == "tester"; }, [](const stDemoProbe& rProbe) { return rProbe.strName; });

	Pad(EGamepadButton::FaceDown);
	Add([this] { m_fnScreenshot("selftest_osk"); }, 3);
	Pad(EGamepadButton::DPadDown);
	Pad(EGamepadButton::FaceDown);
	Add([this] { m_fnScreenshot("selftest_osk_typed"); }, 3);
	Pad(EGamepadButton::Start);
	Check("controller on-screen keyboard types a key", [](const stDemoProbe& rProbe) { return rProbe.strName == "tester1"; }, [](const stDemoProbe& rProbe) { return rProbe.strName; });

	Click("cb_feature_a");
	Check("mouse click clears checkbox a", [](const stDemoProbe& rProbe) { return !rProbe.bFeatureA; });
	Pad(EGamepadButton::FaceDown);
	Check("controller A activates focused checkbox", [](const stDemoProbe& rProbe) { return rProbe.bFeatureA; });
	Pad(EGamepadButton::DPadDown);
	Pad(EGamepadButton::FaceDown);
	Check("controller d-pad moves focus down then A toggles", [](const stDemoProbe& rProbe) { return !rProbe.bFeatureB; });
	Add([this] { m_fnScreenshot("selftest_nav"); }, 3);

	Pad(EGamepadButton::RightShoulder);
	Check("controller RB cycles to next tab", [](const stDemoProbe& rProbe) { return rProbe.strSidebarTab == "settings"; }, [](const stDemoProbe& rProbe) { return rProbe.strSidebarTab; });
	Pad(EGamepadButton::LeftShoulder);
	Check("controller LB cycles to previous tab", [](const stDemoProbe& rProbe) { return rProbe.strSidebarTab == "widgets"; }, [](const stDemoProbe& rProbe) { return rProbe.strSidebarTab; });

	auto pCountBefore = std::make_shared<int>(0);
	Add([pCountBefore] { *pCountBefore = DemoProbe().iCount; }, 1);
	Click("slider_int", { .82f, .5f });
	Check("mouse click sets slider value", [pCountBefore](const stDemoProbe& rProbe) { return rProbe.iCount != *pCountBefore && rProbe.iCount > 50; }, [](const stDemoProbe& rProbe) { return std::to_string(rProbe.iCount); });

	Click("console_input", { .5f, .5f });
	Type("fov 45");
	Key(VK_RETURN);
	Check("console command executes", [](const stDemoProbe& rProbe) { return Near(rProbe.flFov, 45.f, .01f); }, [](const stDemoProbe& rProbe) { return std::format("{}", rProbe.flFov); });
	Check("console ends each command with a blank line", [](const stDemoProbe& rProbe) { return rProbe.strConsoleText.ends_with("\r\n"); });
	Click("console_input", { .5f, .5f });
	Type("theme a");
	Key(VK_TAB);
	Check("console tab completes command arguments", [](const stDemoProbe& rProbe) { return rProbe.strConsoleInput == "theme amber "; }, [](const stDemoProbe& rProbe) { return rProbe.strConsoleInput; });
	Key('A', true);
	Key(VK_BACK);
	auto pClipboard = std::make_shared<std::string>();
	Add([pClipboard] { *pClipboard = ReadClipboard(); }, 1);
	Drag([] { const rect rcLog = FindRect("console_log").value_or(rect{}); return rcLog.min + vec2{ 12.f, 20.f }; }, { 160.f, 0.f });
	Key('C', true);
	Check("console drag selects log text", [](const stDemoProbe& rProbe) { return !rProbe.strConsoleSelection.empty(); });
	Check("console ctrl+c copies the selection", [](const stDemoProbe& rProbe) { return !rProbe.strConsoleSelection.empty() && ReadClipboard() == rProbe.strConsoleSelection; }, [](const stDemoProbe&) { return ReadClipboard(); });
	Add([pClipboard] { SetClipboardText(*pClipboard); }, 1);
	Add([this] { m_fnScreenshot("selftest_console"); }, 3);

	Click("tab_settings");
	Click("combo_theme", { .9f, .5f });
	Add([this] { m_fnScreenshot("selftest_combo"); }, 3);
	Key(VK_DOWN);
	Key(VK_RETURN);
	Check("combo popup keyboard selection", [](const stDemoProbe& rProbe) { return rProbe.iTheme == 1; }, [](const stDemoProbe& rProbe) { return std::to_string(rProbe.iTheme); });

	Click("tab_aim");
	Click("hotkey_aim", { .9f, .5f });
	Key(VK_F5);
	Check("hotkey captures next key", [](const stDemoProbe& rProbe) { return rProbe.uAimKey == VK_F5; }, [](const stDemoProbe& rProbe) { return std::to_string(rProbe.uAimKey); });

	auto pPerfBefore = std::make_shared<rect>();
	Add([] { FocusWindow("perf"); }, 2);
	Add([pPerfBefore] { *pPerfBefore = FindRect("perf_window").value_or(rect{}); GetInput().AddKeyEvent(VK_SHIFT, true); }, 1);
	Drag([] { const rect rcPerf = FindRect("perf_window").value_or(rect{}); return vec2{ rcPerf.Center().x, rcPerf.min.y + 16.f }; }, { -150.f, 60.f });
	Add([] { GetInput().AddKeyEvent(VK_SHIFT, false); }, 1);
	Check("title bar drag moves window", [pPerfBefore](const stDemoProbe& rProbe) { return Near(rProbe.vecPerfPosition.x, pPerfBefore->min.x - 150.f) && Near(rProbe.vecPerfPosition.y, pPerfBefore->min.y + 60.f); },
		[pPerfBefore](const stDemoProbe& rProbe) { return std::format("{},{} from {},{}", rProbe.vecPerfPosition.x, rProbe.vecPerfPosition.y, pPerfBefore->min.x, pPerfBefore->min.y); });

	Add([pPerfBefore] { *pPerfBefore = FindRect("perf_window").value_or(rect{}); }, 1);
	Drag([] { const rect rcPerf = FindRect("perf_window").value_or(rect{}); return rcPerf.max - vec2{ 3.f, 3.f }; }, { 40.f, 30.f });
	Check("corner drag resizes window", [pPerfBefore](const stDemoProbe& rProbe) { return Near(rProbe.vecPerfSize.x, pPerfBefore->Width() + 40.f) && Near(rProbe.vecPerfSize.y, pPerfBefore->Height() + 30.f); },
		[](const stDemoProbe& rProbe) { return std::format("{}x{}", rProbe.vecPerfSize.x, rProbe.vecPerfSize.y); });

	ClickAt([] { const rect rcPerf = FindRect("perf_window").value_or(rect{}); return rcPerf.min + vec2{ 35.f, 16.f }; });
	Add([] {}, 10);
	Check("yellow control collapses window", [](const stDemoProbe& rProbe) { return rProbe.bPerfCollapsed; });

	Add([] {}, 90);
	ClickAt([] { const rect rcPerf = FindRect("perf_window").value_or(rect{}); return rcPerf.min + vec2{ 35.f, 16.f }; });
	Add([] {}, 10);
	Check("yellow control expands a collapsed window", [](const stDemoProbe& rProbe) { return !rProbe.bPerfCollapsed; });

	Add([] { FocusWindow("console"); }, 2);
	ClickAt([] { const rect rcConsole = FindRect("console_window").value_or(rect{}); return rcConsole.min + vec2{ 16.f, 16.f }; });
	Check("red control closes window", [](const stDemoProbe& rProbe) { return !rProbe.bShowConsole; });

	ClickAt([] { const rect rcMain = FindRect("main_window").value_or(rect{}); return rcMain.min + vec2{ 16.f, 16.f }; });
	Add([] {}, 20);
	Check("closing the main window hides the menu", [](const stDemoProbe& rProbe) { return !rProbe.bMenuVisible; });
	Add([] { ProcessWin32Message(nullptr, WM_KEYDOWN, GetConfig().uToggleKey, 0); ProcessWin32Message(nullptr, WM_KEYUP, GetConfig().uToggleKey, 0); }, 20);
	Check("toggle key brings the main window back", [](const stDemoProbe& rProbe) { return rProbe.bMenuVisible && FindRect("main_window").has_value(); });
	Click("tab_widgets");
	Click("link_about");
	Add([] {}, 30);
	Check("modal opens centered", [](const stDemoProbe& rProbe) {
		const auto itModal = rProbe.mapRects.find("about_modal");
		return itModal != rProbe.mapRects.end() && Near(itModal->second.Center().x, GetDisplaySize().x * .5f) && Near(itModal->second.Center().y, GetDisplaySize().y * .5f);
	}, [](const stDemoProbe& rProbe) {
		const auto itModal = rProbe.mapRects.find("about_modal");
		return itModal == rProbe.mapRects.end() ? std::string("not open") : std::format("{},{}", itModal->second.Center().x, itModal->second.Center().y);
	});
	Click("about_close");
	Add([] {}, 10);

	Click("tab_visuals");
	Click("visuals_list");
	Add([] {}, 5);
	Check("table sorts by its default column", [](const stDemoProbe& rProbe) { return rProbe.iTableSortColumn == 0 && rProbe.bTableAscending && rProbe.strTableFirst == "bot_alpha"; }, [](const stDemoProbe& rProbe) { return std::format("{} {} {}", rProbe.iTableSortColumn, rProbe.bTableAscending, rProbe.strTableFirst); });
	Add([this] { m_fnScreenshot("selftest_table"); }, 3);
	ClickAt([] { const rect rcTable = FindRect("table_entities").value_or(rect{}); return rcTable.min + vec2{ 12.f, 8.f }; });
	Check("clicking the sorted header reverses the order", [](const stDemoProbe& rProbe) { return rProbe.iTableSortColumn == 0 && !rProbe.bTableAscending && rProbe.strTableFirst == "bot_delta"; }, [](const stDemoProbe& rProbe) { return std::format("{} {} {}", rProbe.iTableSortColumn, rProbe.bTableAscending, rProbe.strTableFirst); });
	Click("table_header_2");
	Check("clicking another header sorts by it", [](const stDemoProbe& rProbe) { return rProbe.iTableSortColumn == 2 && rProbe.bTableAscending; }, [](const stDemoProbe& rProbe) { return std::format("{} {}", rProbe.iTableSortColumn, rProbe.bTableAscending); });
	Check("table header spans the table width", [](const stDemoProbe& rProbe) {
		const auto rcTable = FindRect("table_entities");
		const auto rcHeader = FindRect("table_header_2");
		return rcTable && rcHeader && Near(rcHeader->max.x, rcTable->max.x, 1.f) && rcTable->Height() > rcHeader->Height() * 4.f;
	});
#ifdef OMEGAGUI_DEMO_LOCALIZATION
	Add([] { OmegaLocalization::SetLocale("de"); }, 4);
	Check("other languages keep the glyphs of their names", [](const stDemoProbe&) {
		const clFont* pFont = GetFonts().GetFont();
		return pFont && pFont->FindGlyph(U'Р') && pFont->FindGlyph(U'й');
	});
	Add([] { OmegaLocalization::SetLocale("en"); }, 4);
#endif
	Add([this] { m_fnScreenshot("selftest_end"); }, 3);
}

void clSelfTest::Add(std::function<void()> fnAction, int iWaitFrames)
{
	m_aSteps.push_back({ std::move(fnAction), iWaitFrames });
}

void clSelfTest::Check(std::string strName, std::function<bool(const stDemoProbe&)> fnCondition, std::function<std::string(const stDemoProbe&)> fnDetail)
{
	Add([this, strName, fnCondition, fnDetail] {
		const bool bPassed = fnCondition(DemoProbe());
		if (!bPassed)
			++m_iFailures;
		std::string strLine = std::format("[{}] {}", bPassed ? "pass" : "FAIL", strName);
		if (!bPassed && fnDetail)
			strLine += std::format(" (got: {})", fnDetail(DemoProbe()));
		m_aLog.push_back(strLine);
	}, 1);
}

void clSelfTest::ClickAt(std::function<vec2()> fnPosition)
{
	Add([fnPosition] {
		const vec2 vecPosition = fnPosition();
		clInput& rInput = GetInput();
		rInput.AddMousePositionEvent(vecPosition);
		rInput.AddMouseButtonEvent(EMouseButton::Left, true);
		rInput.AddMouseButtonEvent(EMouseButton::Left, false);
	});
}

void clSelfTest::Click(std::string strTarget, vec2 vecRelative)
{
	Add([this, strTarget, vecRelative] {
		const auto optRect = FindRect(strTarget);
		if (!optRect)
		{
			++m_iFailures;
			m_aLog.push_back(std::format("[FAIL] target '{}' was not on screen", strTarget));
			return;
		}
		const vec2 vecPosition = optRect->min + optRect->Size() * vecRelative;
		clInput& rInput = GetInput();
		rInput.AddMousePositionEvent(vecPosition);
		rInput.AddMouseButtonEvent(EMouseButton::Left, true);
		rInput.AddMouseButtonEvent(EMouseButton::Left, false);
	});
}

void clSelfTest::Drag(std::function<vec2()> fnFrom, vec2 vecDelta)
{
	auto pStart = std::make_shared<vec2>();
	Add([fnFrom, pStart] {
		*pStart = fnFrom();
		GetInput().AddMousePositionEvent(*pStart);
		GetInput().AddMouseButtonEvent(EMouseButton::Left, true);
	}, 2);
	for (int i = 1; i <= 5; ++i)
		Add([pStart, vecDelta, i] { GetInput().AddMousePositionEvent(*pStart + vecDelta * (static_cast<float>(i) / 5.f)); }, 1);
	Add([] { GetInput().AddMouseButtonEvent(EMouseButton::Left, false); });
}

void clSelfTest::Key(KeyCode uKey, bool bCtrl)
{
	Add([uKey, bCtrl] {
		clInput& rInput = GetInput();
		if (bCtrl)
			rInput.AddKeyEvent(VK_CONTROL, true);
		rInput.AddKeyEvent(uKey, true);
		rInput.AddKeyEvent(uKey, false);
		if (bCtrl)
			rInput.AddKeyEvent(VK_CONTROL, false);
	});
}

void clSelfTest::Type(std::string strText)
{
	Add([strText] {
		for (const char chLetter : strText)
			GetInput().AddCharacterEvent(static_cast<char32_t>(chLetter));
	});
}

void clSelfTest::Pad(EGamepadButton eButton)
{
	Add([this, eButton] {
		m_stPad.aButtons[static_cast<std::size_t>(eButton)] = true;
		GetInput().SetGamepadState(m_stPad);
	}, 1);
	Add([this, eButton] {
		m_stPad.aButtons[static_cast<std::size_t>(eButton)] = false;
		GetInput().SetGamepadState(m_stPad);
	});
}

void clSelfTest::BeforeFrame()
{
	if (m_iWait > 0)
	{
		--m_iWait;
		return;
	}
	if (IsDone())
		return;
	const stStep& rStep = m_aSteps[m_uStep++];
	rStep.fnAction();
	m_iWait = rStep.iWaitFrames;
}

bool clSelfTest::WriteReport(const std::filesystem::path& pathReport) const
{
	std::ofstream fileReport(pathReport, std::ios::trunc);
	if (!fileReport)
		return false;
	for (const std::string& strLine : m_aLog)
		fileReport << strLine << '\n';
	fileReport << std::format("{} failure(s)\n", m_iFailures);
	return true;
}
