#pragma once

#include "Demo/DemoUI.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

class clSelfTest
{
public:
	explicit clSelfTest(std::function<void(const std::string&)> fnScreenshot);

	void BeforeFrame();
	[[nodiscard]] bool IsDone() const { return m_uStep >= m_aSteps.size(); }
	[[nodiscard]] int Failures() const { return m_iFailures; }
	bool WriteReport(const std::filesystem::path& pathReport) const;

private:
	struct stStep
	{
		std::function<void()> fnAction;
		int iWaitFrames = 3;
	};

	void Add(std::function<void()> fnAction, int iWaitFrames = 3);
	void Check(std::string strName, std::function<bool(const stDemoProbe&)> fnCondition, std::function<std::string(const stDemoProbe&)> fnDetail = {});
	void Click(std::string strTarget, OmegaGUI::vec2 vecRelative = { .5f, .5f });
	void ClickAt(std::function<OmegaGUI::vec2()> fnPosition);
	void Drag(std::function<OmegaGUI::vec2()> fnFrom, OmegaGUI::vec2 vecDelta);
	void Key(OmegaGUI::KeyCode uKey, bool bCtrl = false);
	void Type(std::string strText);
	void Pad(OmegaGUI::EGamepadButton eButton);

	std::vector<stStep> m_aSteps;
	std::size_t m_uStep = 0;
	int m_iWait = 5;
	int m_iFailures = 0;
	std::vector<std::string> m_aLog;
	std::function<void(const std::string&)> m_fnScreenshot;
	OmegaGUI::stGamepadState m_stPad;
};
