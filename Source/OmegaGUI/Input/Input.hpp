#pragma once

#include "OmegaGUI/Core/Math.hpp"

#include <array>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace OmegaGUI
{
	enum class EGamepadButton : std::uint8_t
	{
		FaceDown,
		FaceRight,
		FaceLeft,
		FaceUp,
		DPadUp,
		DPadDown,
		DPadLeft,
		DPadRight,
		LeftShoulder,
		RightShoulder,
		LeftTrigger,
		RightTrigger,
		LeftStick,
		RightStick,
		Start,
		Back,
		Guide,
		Touchpad,
		LeftStickUp,
		LeftStickDown,
		LeftStickLeft,
		LeftStickRight,
		Count,
	};

	enum class EGamepadType : std::uint8_t
	{
		None,
		Xbox,
		DualShock4,
		DualSense,
	};

	struct stGamepadState
	{
		bool bConnected = false;
		EGamepadType eType = EGamepadType::None;
		std::array<bool, static_cast<std::size_t>(EGamepadButton::Count)> aButtons{};
		vec2 vecLeftStick;
		vec2 vecRightStick;
		float flLeftTrigger = 0.f;
		float flRightTrigger = 0.f;

		[[nodiscard]] bool IsDown(EGamepadButton eButton) const { return aButtons[static_cast<std::size_t>(eButton)]; }
	};

	enum class EMouseCursor : std::uint8_t
	{
		Arrow,
		TextInput,
		Hand,
		ResizeAll,
		ResizeEW,
		ResizeNS,
		ResizeNWSE,
		ResizeNESW,
		NotAllowed,
		Hidden,
	};

	enum class EMouseButton : std::uint8_t
	{
		Left,
		Right,
		Middle,
		X1,
		X2,
		Count,
	};

	using KeyCode = std::uint16_t;

	inline constexpr KeyCode kKeyGamepadBase = 0x200;
	inline constexpr KeyCode kKeyCount = kKeyGamepadBase + static_cast<KeyCode>(EGamepadButton::Count);

	[[nodiscard]] constexpr KeyCode GamepadKey(EGamepadButton eButton) { return static_cast<KeyCode>(kKeyGamepadBase + static_cast<KeyCode>(eButton)); }
	[[nodiscard]] std::string KeyName(KeyCode uKey, EGamepadType eType = EGamepadType::Xbox);
	[[nodiscard]] std::string_view GamepadButtonName(EGamepadButton eButton, EGamepadType eType);

	class clInput
	{
	public:
		void AddMousePositionEvent(const vec2& vecPosition);
		void AddMouseButtonEvent(EMouseButton eButton, bool bDown);
		void AddMouseWheelEvent(float flVertical, float flHorizontal = 0.f);
		void AddKeyEvent(KeyCode uKey, bool bDown);
		void AddCharacterEvent(char32_t uCodepoint);
		void AddFocusEvent(bool bFocused);
		void SetGamepadState(const stGamepadState& stState);

		void NewFrame(float flDeltaTime);

		[[nodiscard]] bool IsKeyDown(KeyCode uKey) const;
		[[nodiscard]] bool IsKeyPressed(KeyCode uKey, bool bRepeat = false) const;
		[[nodiscard]] bool IsKeyReleased(KeyCode uKey) const;
		[[nodiscard]] float GetKeyDownDuration(KeyCode uKey) const;

		[[nodiscard]] bool IsMouseDown(EMouseButton eButton) const { return m_aMouse[static_cast<std::size_t>(eButton)].bDown; }
		[[nodiscard]] bool IsMouseClicked(EMouseButton eButton) const { return m_aMouse[static_cast<std::size_t>(eButton)].bClicked; }
		[[nodiscard]] bool IsMouseReleased(EMouseButton eButton) const { return m_aMouse[static_cast<std::size_t>(eButton)].bReleased; }
		[[nodiscard]] bool IsMouseDoubleClicked(EMouseButton eButton) const { return m_aMouse[static_cast<std::size_t>(eButton)].bDoubleClicked; }
		[[nodiscard]] vec2 GetMouseClickedPosition(EMouseButton eButton) const { return m_aMouse[static_cast<std::size_t>(eButton)].vecClickedPosition; }
		[[nodiscard]] bool IsMouseDragging(EMouseButton eButton, float flThreshold = 3.f) const;

		[[nodiscard]] const vec2& GetMousePosition() const { return m_vecMousePosition; }
		[[nodiscard]] vec2 GetMouseDelta() const { return m_vecMouseDelta; }
		[[nodiscard]] bool IsMouseValid() const { return m_vecMousePosition.x > -1e5f; }
		[[nodiscard]] float GetMouseWheel() const { return m_flWheel; }
		[[nodiscard]] float GetMouseWheelHorizontal() const { return m_flWheelHorizontal; }
		[[nodiscard]] bool HasMouseMoved() const { return m_bMouseMoved; }

		[[nodiscard]] const std::u32string& GetCharacters() const { return m_strCharacters; }
		[[nodiscard]] bool IsCtrlDown() const;
		[[nodiscard]] bool IsShiftDown() const;
		[[nodiscard]] bool IsAltDown() const;
		[[nodiscard]] bool IsFocused() const { return m_bFocused; }

		[[nodiscard]] const stGamepadState& GetGamepad() const { return m_stGamepad; }
		[[nodiscard]] bool IsGamepadDown(EGamepadButton eButton) const { return IsKeyDown(GamepadKey(eButton)); }
		[[nodiscard]] bool IsGamepadPressed(EGamepadButton eButton, bool bRepeat = false) const { return IsKeyPressed(GamepadKey(eButton), bRepeat); }
		[[nodiscard]] bool WasGamepadUsedLast() const { return m_bGamepadUsedLast; }
		[[nodiscard]] bool WasKeyboardUsedLast() const { return m_bKeyboardUsedLast; }
		[[nodiscard]] KeyCode GetLastPressedKey() const { return m_uLastPressedKey; }

		[[nodiscard]] double GetTime() const { return m_dTime; }
		[[nodiscard]] float GetDeltaTime() const { return m_flDeltaTime; }

		float flKeyRepeatDelay = .30f;
		float flKeyRepeatRate = .06f;
		float flDoubleClickTime = .30f;
		float flDoubleClickDistance = 6.f;
		float flStickDeadzone = .35f;

	private:
		enum class EEventType : std::uint8_t
		{
			MousePosition,
			MouseButton,
			MouseWheel,
			Key,
			Character,
			Focus,
		};

		struct stEvent
		{
			EEventType eType;
			vec2 vecValue;
			std::uint32_t uValue = 0;
			bool bDown = false;
		};

		struct stMouseButtonState
		{
			bool bDown = false;
			bool bClicked = false;
			bool bReleased = false;
			bool bDoubleClicked = false;
			double dClickedTime = -10.0;
			vec2 vecClickedPosition;
			float flDownDuration = -1.f;
		};

		void ApplyGamepad();

		std::deque<stEvent> m_aEvents;
		std::array<bool, kKeyCount> m_aKeyDown{};
		std::array<float, kKeyCount> m_aKeyDuration{};
		std::array<float, kKeyCount> m_aKeyDurationPrevious{};
		std::array<bool, kKeyCount> m_aKeyReleased{};
		std::array<stMouseButtonState, static_cast<std::size_t>(EMouseButton::Count)> m_aMouse{};
		std::array<bool, static_cast<std::size_t>(EMouseButton::Count)> m_aMouseRawDown{};
		vec2 m_vecMousePosition{ -1e6f, -1e6f };
		vec2 m_vecMousePositionPrevious{ -1e6f, -1e6f };
		vec2 m_vecMouseDelta;
		float m_flWheel = 0.f;
		float m_flWheelHorizontal = 0.f;
		bool m_bMouseMoved = false;
		std::u32string m_strCharacters;
		bool m_bFocused = true;
		stGamepadState m_stGamepad;
		bool m_bGamepadUsedLast = false;
		bool m_bKeyboardUsedLast = false;
		KeyCode m_uLastPressedKey = 0;
		double m_dTime = 0.0;
		float m_flDeltaTime = 1.f / 60.f;
		bool m_bInitializedDurations = false;
	};
}
