#pragma once

#include "OmegaGUI/OmegaGUI.hpp"
#include "OmegaGUI/UI/Console.hpp"

#include <string>
#include <unordered_map>

struct stDemoCamera
{
	bool bClipSpaceYDown = false;
	bool bDepthMinusOneToOne = false;
};

struct stDemoProbe
{
	std::unordered_map<std::string, OmegaGUI::rect> mapRects;
	std::string strSidebarTab;
	bool bFeatureA = false;
	bool bFeatureB = false;
	std::string strName;
	int iCount = 0;
	float flFov = 0.f;
	int iTheme = 0;
	bool bShowConsole = false;
	OmegaGUI::vec2 vecPerfPosition;
	OmegaGUI::vec2 vecPerfSize;
	bool bPerfCollapsed = false;
	OmegaGUI::KeyCode uAimKey = 0;
	bool bMenuVisible = false;
	int iTableSortColumn = -1;
	bool bTableAscending = true;
	std::string strTableFirst;
	std::string strConsoleInput;
	std::string strConsoleSelection;
	std::string strConsoleText;
};

[[nodiscard]] const stDemoProbe& DemoProbe();

void DemoInitialize(std::string_view strBackendName, std::string_view strStartTab = {}, std::string_view strLanguage = {});
void DemoFrame(const stDemoCamera& stCamera);
