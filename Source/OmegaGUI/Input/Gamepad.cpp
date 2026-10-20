#include "OmegaGUI/Input/Gamepad.hpp"

#include <Windows.h>
#include <SetupAPI.h>
#include <Xinput.h>
#include <hidsdi.h>
#include <hidpi.h>

#include <chrono>
#include <optional>
#include <string>

namespace OmegaGUI
{
	namespace
	{
		using FnXInputGetState = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
		using FnXInputSetState = DWORD(WINAPI*)(DWORD, XINPUT_VIBRATION*);

		constexpr USHORT uSonyVendor = 0x054C;

		enum class EHidKind : std::uint8_t
		{
			DualShock4,
			DualSense,
		};

		[[nodiscard]] std::optional<EHidKind> ClassifySony(USHORT uProduct)
		{
			switch (uProduct)
			{
			case 0x05C4: case 0x09CC: case 0x0BA0: return EHidKind::DualShock4;
			case 0x0CE6: case 0x0DF2: return EHidKind::DualSense;
			default: return std::nullopt;
			}
		}

		[[nodiscard]] float NormalizeAxis(std::uint8_t byValue)
		{
			return std::clamp((static_cast<float>(byValue) - 127.5f) / 127.5f, -1.f, 1.f);
		}

		[[nodiscard]] vec2 ApplyRadialDeadzone(vec2 vecStick, float flDeadzone)
		{
			const float flLength = vecStick.Length();
			if (flLength <= flDeadzone)
				return {};
			const float flScaled = std::min((flLength - flDeadzone) / (1.f - flDeadzone), 1.f);
			return vecStick * (flScaled / flLength);
		}

		void ApplyHat(stGamepadState& rState, std::uint8_t byHat)
		{
			auto Set = [&](EGamepadButton eButton, bool bValue) { rState.aButtons[static_cast<std::size_t>(eButton)] = bValue; };
			Set(EGamepadButton::DPadUp, byHat == 0 || byHat == 1 || byHat == 7);
			Set(EGamepadButton::DPadRight, byHat == 1 || byHat == 2 || byHat == 3);
			Set(EGamepadButton::DPadDown, byHat == 3 || byHat == 4 || byHat == 5);
			Set(EGamepadButton::DPadLeft, byHat == 5 || byHat == 6 || byHat == 7);
		}

		[[nodiscard]] bool ParseSonyReport(EHidKind eKind, const std::uint8_t* pReport, std::size_t uLength, float flDeadzone, float flTriggerThreshold, stGamepadState& rState)
		{
			if (uLength < 10)
				return false;

			std::size_t uSticks = 1;
			std::size_t uButtons = 5;
			std::size_t uTriggers = 8;
			if (eKind == EHidKind::DualSense)
			{
				if (pReport[0] == 0x01 && uLength >= 64)
				{
					uTriggers = 5;
					uButtons = 8;
				}
				else if (pReport[0] == 0x31 && uLength >= 64)
				{
					uSticks = 2;
					uTriggers = 6;
					uButtons = 9;
				}
				else if (pReport[0] != 0x01)
					return false;
			}
			else
			{
				if (pReport[0] == 0x11 && uLength >= 64)
				{
					uSticks = 3;
					uButtons = 7;
					uTriggers = 10;
				}
				else if (pReport[0] != 0x01)
					return false;
			}
			if (uButtons + 2 >= uLength || uTriggers + 1 >= uLength)
				return false;

			const std::uint8_t byFace = pReport[uButtons];
			const std::uint8_t byShoulders = pReport[uButtons + 1];
			const std::uint8_t bySystem = pReport[uButtons + 2];

			rState = {};
			rState.bConnected = true;
			rState.eType = eKind == EHidKind::DualSense ? EGamepadType::DualSense : EGamepadType::DualShock4;
			rState.vecLeftStick = ApplyRadialDeadzone({ NormalizeAxis(pReport[uSticks]), -NormalizeAxis(pReport[uSticks + 1]) }, flDeadzone);
			rState.vecRightStick = ApplyRadialDeadzone({ NormalizeAxis(pReport[uSticks + 2]), -NormalizeAxis(pReport[uSticks + 3]) }, flDeadzone);
			rState.flLeftTrigger = pReport[uTriggers] / 255.f;
			rState.flRightTrigger = pReport[uTriggers + 1] / 255.f;

			auto Set = [&](EGamepadButton eButton, bool bValue) { rState.aButtons[static_cast<std::size_t>(eButton)] = bValue; };
			ApplyHat(rState, byFace & 0x0F);
			Set(EGamepadButton::FaceLeft, byFace & 0x10);
			Set(EGamepadButton::FaceDown, byFace & 0x20);
			Set(EGamepadButton::FaceRight, byFace & 0x40);
			Set(EGamepadButton::FaceUp, byFace & 0x80);
			Set(EGamepadButton::LeftShoulder, byShoulders & 0x01);
			Set(EGamepadButton::RightShoulder, byShoulders & 0x02);
			Set(EGamepadButton::LeftTrigger, rState.flLeftTrigger > flTriggerThreshold);
			Set(EGamepadButton::RightTrigger, rState.flRightTrigger > flTriggerThreshold);
			Set(EGamepadButton::Back, byShoulders & 0x10);
			Set(EGamepadButton::Start, byShoulders & 0x20);
			Set(EGamepadButton::LeftStick, byShoulders & 0x40);
			Set(EGamepadButton::RightStick, byShoulders & 0x80);
			Set(EGamepadButton::Guide, bySystem & 0x01);
			Set(EGamepadButton::Touchpad, bySystem & 0x02);
			return true;
		}
	}

	struct clGamepadManager::stHidDevice
	{
		std::wstring strPath;
		HANDLE hFile = INVALID_HANDLE_VALUE;
		HANDLE hEvent = nullptr;
		OVERLAPPED stOverlapped{};
		std::vector<std::uint8_t> aBuffer;
		EHidKind eKind = EHidKind::DualShock4;
		bool bPending = false;
		bool bFailed = false;

		~stHidDevice()
		{
			if (hFile != INVALID_HANDLE_VALUE)
			{
				CancelIoEx(hFile, nullptr);
				CloseHandle(hFile);
			}
			if (hEvent)
				CloseHandle(hEvent);
		}

		bool IssueRead()
		{
			ResetEvent(hEvent);
			stOverlapped = {};
			stOverlapped.hEvent = hEvent;
			if (ReadFile(hFile, aBuffer.data(), static_cast<DWORD>(aBuffer.size()), nullptr, &stOverlapped) || GetLastError() == ERROR_IO_PENDING)
			{
				bPending = true;
				return true;
			}
			bFailed = true;
			return false;
		}
	};

	clGamepadManager::clGamepadManager() = default;

	clGamepadManager::~clGamepadManager()
	{
		Shutdown();
	}

	bool clGamepadManager::Initialize(bool bEnableXInput, bool bEnablePlayStation)
	{
		if (m_bInitialized)
			return true;
		if (bEnableXInput)
		{
			for (const wchar_t* szName : { L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll" })
				if (HMODULE hModule = LoadLibraryW(szName))
				{
					m_hXInput = hModule;
					m_pfnXInputGetState = reinterpret_cast<void*>(GetProcAddress(hModule, "XInputGetState"));
					m_pfnXInputSetState = reinterpret_cast<void*>(GetProcAddress(hModule, "XInputSetState"));
					break;
				}
		}
		if (bEnablePlayStation)
		{
			m_bRunning = true;
			m_threadHid = std::thread(&clGamepadManager::HidThread, this);
		}
		m_bInitialized = true;
		return true;
	}

	void clGamepadManager::Shutdown()
	{
		m_bRunning = false;
		if (m_threadHid.joinable())
			m_threadHid.join();
		{
			std::scoped_lock lockHid(m_mutexHid);
			m_aHidDevices.clear();
		}
		if (m_hXInput)
		{
			FreeLibrary(static_cast<HMODULE>(m_hXInput));
			m_hXInput = nullptr;
			m_pfnXInputGetState = m_pfnXInputSetState = nullptr;
		}
		m_bInitialized = false;
	}

	bool clGamepadManager::PollXInput(stGamepadState& rState)
	{
		if (!m_pfnXInputGetState)
			return false;
		const auto pfnGetState = reinterpret_cast<FnXInputGetState>(m_pfnXInputGetState);

		XINPUT_STATE stXInput{};
		if (pfnGetState(m_uXInputSlot, &stXInput) != ERROR_SUCCESS)
		{
			if (--m_iXInputRescanCountdown > 0)
				return false;
			m_iXInputRescanCountdown = 60;
			bool bFound = false;
			for (DWORD uSlot = 0; uSlot < XUSER_MAX_COUNT; ++uSlot)
				if (pfnGetState(uSlot, &stXInput) == ERROR_SUCCESS)
				{
					m_uXInputSlot = uSlot;
					bFound = true;
					break;
				}
			if (!bFound)
				return false;
		}

		const XINPUT_GAMEPAD& rPad = stXInput.Gamepad;
		rState = {};
		rState.bConnected = true;
		rState.eType = EGamepadType::Xbox;
		rState.vecLeftStick = ApplyRadialDeadzone({ rPad.sThumbLX / 32767.f, rPad.sThumbLY / 32767.f }, flStickDeadzone);
		rState.vecRightStick = ApplyRadialDeadzone({ rPad.sThumbRX / 32767.f, rPad.sThumbRY / 32767.f }, flStickDeadzone);
		rState.flLeftTrigger = rPad.bLeftTrigger / 255.f;
		rState.flRightTrigger = rPad.bRightTrigger / 255.f;

		auto Set = [&](EGamepadButton eButton, WORD uMask) { rState.aButtons[static_cast<std::size_t>(eButton)] = (rPad.wButtons & uMask) != 0; };
		Set(EGamepadButton::FaceDown, XINPUT_GAMEPAD_A);
		Set(EGamepadButton::FaceRight, XINPUT_GAMEPAD_B);
		Set(EGamepadButton::FaceLeft, XINPUT_GAMEPAD_X);
		Set(EGamepadButton::FaceUp, XINPUT_GAMEPAD_Y);
		Set(EGamepadButton::DPadUp, XINPUT_GAMEPAD_DPAD_UP);
		Set(EGamepadButton::DPadDown, XINPUT_GAMEPAD_DPAD_DOWN);
		Set(EGamepadButton::DPadLeft, XINPUT_GAMEPAD_DPAD_LEFT);
		Set(EGamepadButton::DPadRight, XINPUT_GAMEPAD_DPAD_RIGHT);
		Set(EGamepadButton::LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER);
		Set(EGamepadButton::RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER);
		Set(EGamepadButton::LeftStick, XINPUT_GAMEPAD_LEFT_THUMB);
		Set(EGamepadButton::RightStick, XINPUT_GAMEPAD_RIGHT_THUMB);
		Set(EGamepadButton::Start, XINPUT_GAMEPAD_START);
		Set(EGamepadButton::Back, XINPUT_GAMEPAD_BACK);
		rState.aButtons[static_cast<std::size_t>(EGamepadButton::LeftTrigger)] = rState.flLeftTrigger > flTriggerThreshold;
		rState.aButtons[static_cast<std::size_t>(EGamepadButton::RightTrigger)] = rState.flRightTrigger > flTriggerThreshold;
		return true;
	}

	stGamepadState clGamepadManager::Poll()
	{
		stGamepadState stState;
		if (PollXInput(stState))
			return stState;
		std::scoped_lock lockHid(m_mutexHid);
		return m_stHidState;
	}

	void clGamepadManager::SetRumble(float flLow, float flHigh)
	{
		if (!m_pfnXInputSetState)
			return;
		XINPUT_VIBRATION stVibration{ static_cast<WORD>(std::clamp(flLow, 0.f, 1.f) * 65535.f), static_cast<WORD>(std::clamp(flHigh, 0.f, 1.f) * 65535.f) };
		reinterpret_cast<FnXInputSetState>(m_pfnXInputSetState)(m_uXInputSlot, &stVibration);
	}

	void clGamepadManager::ScanHidDevices()
	{
		GUID guidHid{};
		HidD_GetHidGuid(&guidHid);
		HDEVINFO hDeviceInfo = SetupDiGetClassDevsW(&guidHid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
		if (hDeviceInfo == INVALID_HANDLE_VALUE)
			return;

		SP_DEVICE_INTERFACE_DATA stInterface{ sizeof(SP_DEVICE_INTERFACE_DATA) };
		for (DWORD uIndex = 0; SetupDiEnumDeviceInterfaces(hDeviceInfo, nullptr, &guidHid, uIndex, &stInterface); ++uIndex)
		{
			DWORD uRequired = 0;
			SetupDiGetDeviceInterfaceDetailW(hDeviceInfo, &stInterface, nullptr, 0, &uRequired, nullptr);
			if (uRequired == 0)
				continue;
			std::vector<std::uint8_t> aDetail(uRequired);
			auto* pDetail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(aDetail.data());
			pDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
			if (!SetupDiGetDeviceInterfaceDetailW(hDeviceInfo, &stInterface, pDetail, uRequired, nullptr, nullptr))
				continue;

			const std::wstring strPath = pDetail->DevicePath;
			bool bKnown = false;
			{
				std::scoped_lock lockHid(m_mutexHid);
				bKnown = std::ranges::any_of(m_aHidDevices, [&](const auto& pDevice) { return pDevice->strPath == strPath; });
			}
			if (bKnown)
				continue;

			HANDLE hFile = CreateFileW(strPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
			if (hFile == INVALID_HANDLE_VALUE)
				hFile = CreateFileW(strPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
			if (hFile == INVALID_HANDLE_VALUE)
				continue;

			HIDD_ATTRIBUTES stAttributes{ sizeof(HIDD_ATTRIBUTES) };
			const auto optKind = HidD_GetAttributes(hFile, &stAttributes) && stAttributes.VendorID == uSonyVendor ? ClassifySony(stAttributes.ProductID) : std::nullopt;
			if (!optKind)
			{
				CloseHandle(hFile);
				continue;
			}

			USHORT uReportLength = 64;
			PHIDP_PREPARSED_DATA pPreparsed = nullptr;
			if (HidD_GetPreparsedData(hFile, &pPreparsed))
			{
				HIDP_CAPS stCaps{};
				if (HidP_GetCaps(pPreparsed, &stCaps) == HIDP_STATUS_SUCCESS && stCaps.InputReportByteLength > 0)
					uReportLength = stCaps.InputReportByteLength;
				HidD_FreePreparsedData(pPreparsed);
			}

			auto pDevice = std::make_unique<stHidDevice>();
			pDevice->strPath = strPath;
			pDevice->hFile = hFile;
			pDevice->hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
			pDevice->aBuffer.resize(uReportLength);
			pDevice->eKind = *optKind;
			if (pDevice->IssueRead())
			{
				std::scoped_lock lockHid(m_mutexHid);
				m_aHidDevices.push_back(std::move(pDevice));
			}
		}
		SetupDiDestroyDeviceInfoList(hDeviceInfo);
	}

	void clGamepadManager::HidThread()
	{
		auto timeNextScan = std::chrono::steady_clock::now();
		while (m_bRunning)
		{
			if (std::chrono::steady_clock::now() >= timeNextScan)
			{
				ScanHidDevices();
				timeNextScan = std::chrono::steady_clock::now() + std::chrono::seconds(2);
			}

			std::vector<HANDLE> aEvents;
			std::vector<stHidDevice*> aDevices;
			{
				std::scoped_lock lockHid(m_mutexHid);
				for (const auto& pDevice : m_aHidDevices)
					if (pDevice->bPending)
					{
						aEvents.push_back(pDevice->hEvent);
						aDevices.push_back(pDevice.get());
					}
			}
			if (aEvents.empty())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			const DWORD uWait = WaitForMultipleObjects(static_cast<DWORD>(aEvents.size()), aEvents.data(), FALSE, 100);
			if (uWait >= WAIT_OBJECT_0 + aEvents.size())
				continue;

			stHidDevice* pDevice = aDevices[uWait - WAIT_OBJECT_0];
			DWORD uRead = 0;
			pDevice->bPending = false;
			if (GetOverlappedResult(pDevice->hFile, &pDevice->stOverlapped, &uRead, FALSE))
			{
				stGamepadState stParsed;
				if (ParseSonyReport(pDevice->eKind, pDevice->aBuffer.data(), uRead, flStickDeadzone, flTriggerThreshold, stParsed))
				{
					std::scoped_lock lockHid(m_mutexHid);
					m_stHidState = stParsed;
				}
				pDevice->IssueRead();
			}
			else
				pDevice->bFailed = true;

			if (pDevice->bFailed)
			{
				std::scoped_lock lockHid(m_mutexHid);
				std::erase_if(m_aHidDevices, [&](const auto& pEntry) { return pEntry.get() == pDevice; });
				if (m_aHidDevices.empty())
					m_stHidState = {};
			}
		}
	}
}
