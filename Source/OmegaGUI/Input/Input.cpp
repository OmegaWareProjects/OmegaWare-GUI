#include "OmegaGUI/Input/Input.hpp"

#include <Windows.h>

#include <format>

namespace OmegaGUI
{
	namespace
	{
		[[nodiscard]] bool RepeatTriggered(float flPrevious, float flCurrent, float flDelay, float flRate)
		{
			if (flCurrent == 0.f)
				return true;
			if (flPrevious >= flCurrent || flCurrent < flDelay || flRate <= 0.f)
				return false;
			const int iPrevious = flPrevious < flDelay ? -1 : static_cast<int>((flPrevious - flDelay) / flRate);
			const int iCurrent = static_cast<int>((flCurrent - flDelay) / flRate);
			return iCurrent > iPrevious;
		}

		constexpr KeyCode aMouseKeys[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
	}

	std::string_view GamepadButtonName(EGamepadButton eButton, EGamepadType eType)
	{
		const bool bPlayStation = eType == EGamepadType::DualShock4 || eType == EGamepadType::DualSense;
		switch (eButton)
		{
		case EGamepadButton::FaceDown: return bPlayStation ? "Cross" : "A";
		case EGamepadButton::FaceRight: return bPlayStation ? "Circle" : "B";
		case EGamepadButton::FaceLeft: return bPlayStation ? "Square" : "X";
		case EGamepadButton::FaceUp: return bPlayStation ? "Triangle" : "Y";
		case EGamepadButton::DPadUp: return "D-Up";
		case EGamepadButton::DPadDown: return "D-Down";
		case EGamepadButton::DPadLeft: return "D-Left";
		case EGamepadButton::DPadRight: return "D-Right";
		case EGamepadButton::LeftShoulder: return bPlayStation ? "L1" : "LB";
		case EGamepadButton::RightShoulder: return bPlayStation ? "R1" : "RB";
		case EGamepadButton::LeftTrigger: return bPlayStation ? "L2" : "LT";
		case EGamepadButton::RightTrigger: return bPlayStation ? "R2" : "RT";
		case EGamepadButton::LeftStick: return bPlayStation ? "L3" : "LS";
		case EGamepadButton::RightStick: return bPlayStation ? "R3" : "RS";
		case EGamepadButton::Start: return bPlayStation ? "Options" : "Menu";
		case EGamepadButton::Back: return eType == EGamepadType::DualSense ? "Create" : bPlayStation ? "Share" : "View";
		case EGamepadButton::Guide: return bPlayStation ? "PS" : "Guide";
		case EGamepadButton::Touchpad: return "Touchpad";
		case EGamepadButton::LeftStickUp: return "LS-Up";
		case EGamepadButton::LeftStickDown: return "LS-Down";
		case EGamepadButton::LeftStickLeft: return "LS-Left";
		case EGamepadButton::LeftStickRight: return "LS-Right";
		default: return "?";
		}
	}

	std::string KeyName(KeyCode uKey, EGamepadType eType)
	{
		if (uKey == 0)
			return "none";
		if (uKey >= kKeyGamepadBase && uKey < kKeyCount)
			return std::string(GamepadButtonName(static_cast<EGamepadButton>(uKey - kKeyGamepadBase), eType == EGamepadType::None ? EGamepadType::Xbox : eType));

		switch (uKey)
		{
		case VK_LBUTTON: return "Mouse1";
		case VK_RBUTTON: return "Mouse2";
		case VK_MBUTTON: return "Mouse3";
		case VK_XBUTTON1: return "Mouse4";
		case VK_XBUTTON2: return "Mouse5";
		default: break;
		}

		UINT uScanCode = MapVirtualKeyW(uKey, MAPVK_VK_TO_VSC);
		switch (uKey)
		{
		case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
		case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
		case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
		case VK_RCONTROL: case VK_RMENU:
			uScanCode |= 0x100;
			break;
		default:
			break;
		}
		wchar_t szName[64]{};
		const int iLength = GetKeyNameTextW(static_cast<LONG>(uScanCode << 16), szName, 64);
		if (iLength <= 0)
			return std::format("0x{:02X}", uKey);
		std::string strResult;
		for (int i = 0; i < iLength; ++i)
			strResult.push_back(szName[i] < 0x80 ? static_cast<char>(szName[i]) : '?');
		return strResult;
	}

	void clInput::AddMousePositionEvent(const vec2& vecPosition)
	{
		m_aEvents.push_back({ EEventType::MousePosition, vecPosition });
	}

	void clInput::AddMouseButtonEvent(EMouseButton eButton, bool bDown)
	{
		m_aEvents.push_back({ EEventType::MouseButton, {}, static_cast<std::uint32_t>(eButton), bDown });
	}

	void clInput::AddMouseWheelEvent(float flVertical, float flHorizontal)
	{
		m_aEvents.push_back({ EEventType::MouseWheel, { flHorizontal, flVertical } });
	}

	void clInput::AddKeyEvent(KeyCode uKey, bool bDown)
	{
		if (uKey == 0 || uKey >= kKeyGamepadBase)
			return;
		m_aEvents.push_back({ EEventType::Key, {}, uKey, bDown });
	}

	void clInput::AddCharacterEvent(char32_t uCodepoint)
	{
		if (uCodepoint == 0)
			return;
		m_aEvents.push_back({ EEventType::Character, {}, static_cast<std::uint32_t>(uCodepoint) });
	}

	void clInput::AddFocusEvent(bool bFocused)
	{
		m_aEvents.push_back({ EEventType::Focus, {}, 0, bFocused });
	}

	void clInput::SetGamepadState(const stGamepadState& stState)
	{
		m_stGamepad = stState;
	}

	void clInput::ApplyGamepad()
	{
		stGamepadState& rPad = m_stGamepad;
		if (rPad.bConnected)
		{
			auto SetDerived = [&](EGamepadButton eButton, bool bValue) { rPad.aButtons[static_cast<std::size_t>(eButton)] = bValue; };
			SetDerived(EGamepadButton::LeftStickUp, rPad.vecLeftStick.y > flStickDeadzone);
			SetDerived(EGamepadButton::LeftStickDown, rPad.vecLeftStick.y < -flStickDeadzone);
			SetDerived(EGamepadButton::LeftStickLeft, rPad.vecLeftStick.x < -flStickDeadzone);
			SetDerived(EGamepadButton::LeftStickRight, rPad.vecLeftStick.x > flStickDeadzone);
		}
		for (std::size_t i = 0; i < static_cast<std::size_t>(EGamepadButton::Count); ++i)
			m_aKeyDown[kKeyGamepadBase + i] = rPad.bConnected && rPad.aButtons[i];
	}

	void clInput::NewFrame(float flDeltaTime)
	{
		m_flDeltaTime = std::max(flDeltaTime, 1e-5f);
		m_dTime += m_flDeltaTime;
		if (!m_bInitializedDurations)
		{
			m_aKeyDuration.fill(-1.f);
			m_aKeyDurationPrevious.fill(-1.f);
			m_bInitializedDurations = true;
		}

		m_flWheel = m_flWheelHorizontal = 0.f;
		m_strCharacters.clear();

		std::array<bool, kKeyCount> aKeyChanged{};
		std::array<bool, static_cast<std::size_t>(EMouseButton::Count)> aMouseChanged{};
		bool bAnyMouseChanged = false;
		while (!m_aEvents.empty())
		{
			const stEvent& rEvent = m_aEvents.front();
			bool bStop = false;
			switch (rEvent.eType)
			{
			case EEventType::MousePosition:
				if (bAnyMouseChanged)
					bStop = true;
				else
					m_vecMousePosition = rEvent.vecValue;
				break;
			case EEventType::MouseButton:
				if (aMouseChanged[rEvent.uValue])
					bStop = true;
				else
				{
					aMouseChanged[rEvent.uValue] = true;
					bAnyMouseChanged = true;
					m_aMouseRawDown[rEvent.uValue] = rEvent.bDown;
					m_aKeyDown[aMouseKeys[rEvent.uValue]] = rEvent.bDown;
				}
				break;
			case EEventType::MouseWheel:
				m_flWheel += rEvent.vecValue.y;
				m_flWheelHorizontal += rEvent.vecValue.x;
				break;
			case EEventType::Key:
				if (aKeyChanged[rEvent.uValue])
					bStop = true;
				else
				{
					aKeyChanged[rEvent.uValue] = true;
					m_aKeyDown[rEvent.uValue] = rEvent.bDown;
				}
				break;
			case EEventType::Character:
				m_strCharacters.push_back(static_cast<char32_t>(rEvent.uValue));
				break;
			case EEventType::Focus:
				m_bFocused = rEvent.bDown;
				if (!m_bFocused)
				{
					for (std::size_t i = 0; i < kKeyGamepadBase; ++i)
						m_aKeyDown[i] = false;
					m_aMouseRawDown.fill(false);
				}
				break;
			}
			if (bStop)
				break;
			m_aEvents.pop_front();
		}

		ApplyGamepad();

		m_vecMouseDelta = IsMouseValid() && m_vecMousePositionPrevious.x > -1e5f ? m_vecMousePosition - m_vecMousePositionPrevious : vec2{};
		m_bMouseMoved = m_vecMouseDelta.LengthSqr() > 0.f;
		m_vecMousePositionPrevious = m_vecMousePosition;

		for (std::size_t i = 0; i < kKeyCount; ++i)
		{
			m_aKeyDurationPrevious[i] = m_aKeyDuration[i];
			m_aKeyReleased[i] = !m_aKeyDown[i] && m_aKeyDuration[i] >= 0.f;
			m_aKeyDuration[i] = m_aKeyDown[i] ? (m_aKeyDuration[i] < 0.f ? 0.f : m_aKeyDuration[i] + m_flDeltaTime) : -1.f;
			if (m_aKeyDuration[i] == 0.f)
			{
				m_uLastPressedKey = static_cast<KeyCode>(i);
				const bool bGamepad = i >= kKeyGamepadBase;
				const bool bMouse = std::ranges::find(aMouseKeys, static_cast<KeyCode>(i)) != std::end(aMouseKeys);
				if (bGamepad)
				{
					m_bGamepadUsedLast = true;
					m_bKeyboardUsedLast = false;
				}
				else if (!bMouse)
				{
					m_bKeyboardUsedLast = true;
					m_bGamepadUsedLast = false;
				}
			}
		}
		if (m_bMouseMoved || m_flWheel != 0.f)
			m_bGamepadUsedLast = m_bKeyboardUsedLast = false;

		for (std::size_t i = 0; i < m_aMouse.size(); ++i)
		{
			stMouseButtonState& rButton = m_aMouse[i];
			const bool bWasDown = rButton.bDown;
			rButton.bDown = m_aMouseRawDown[i];
			rButton.bClicked = rButton.bDown && !bWasDown;
			rButton.bReleased = !rButton.bDown && bWasDown;
			rButton.bDoubleClicked = false;
			rButton.flDownDuration = rButton.bDown ? (bWasDown ? rButton.flDownDuration + m_flDeltaTime : 0.f) : -1.f;
			if (rButton.bClicked)
			{
				if (m_dTime - rButton.dClickedTime < flDoubleClickTime && (m_vecMousePosition - rButton.vecClickedPosition).Length() < flDoubleClickDistance)
				{
					rButton.bDoubleClicked = true;
					rButton.dClickedTime = -10.0;
				}
				else
					rButton.dClickedTime = m_dTime;
				rButton.vecClickedPosition = m_vecMousePosition;
				m_bGamepadUsedLast = m_bKeyboardUsedLast = false;
			}
		}
	}

	bool clInput::IsKeyDown(KeyCode uKey) const
	{
		return uKey < kKeyCount && m_aKeyDown[uKey];
	}

	bool clInput::IsKeyPressed(KeyCode uKey, bool bRepeat) const
	{
		if (uKey >= kKeyCount)
			return false;
		if (!bRepeat)
			return m_aKeyDuration[uKey] == 0.f;
		return m_aKeyDuration[uKey] >= 0.f && RepeatTriggered(m_aKeyDurationPrevious[uKey], m_aKeyDuration[uKey], flKeyRepeatDelay, flKeyRepeatRate);
	}

	bool clInput::IsKeyReleased(KeyCode uKey) const
	{
		return uKey < kKeyCount && m_aKeyReleased[uKey];
	}

	float clInput::GetKeyDownDuration(KeyCode uKey) const
	{
		return uKey < kKeyCount ? m_aKeyDuration[uKey] : -1.f;
	}

	bool clInput::IsMouseDragging(EMouseButton eButton, float flThreshold) const
	{
		const stMouseButtonState& rButton = m_aMouse[static_cast<std::size_t>(eButton)];
		return rButton.bDown && (m_vecMousePosition - rButton.vecClickedPosition).Length() >= flThreshold;
	}

	bool clInput::IsCtrlDown() const
	{
		return IsKeyDown(VK_CONTROL) || IsKeyDown(VK_LCONTROL) || IsKeyDown(VK_RCONTROL);
	}

	bool clInput::IsShiftDown() const
	{
		return IsKeyDown(VK_SHIFT) || IsKeyDown(VK_LSHIFT) || IsKeyDown(VK_RSHIFT);
	}

	bool clInput::IsAltDown() const
	{
		return IsKeyDown(VK_MENU) || IsKeyDown(VK_LMENU) || IsKeyDown(VK_RMENU);
	}
}
