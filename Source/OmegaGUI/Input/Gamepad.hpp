#pragma once

#include "OmegaGUI/Input/Input.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace OmegaGUI
{
	class clGamepadManager
	{
	public:
		clGamepadManager();
		~clGamepadManager();
		clGamepadManager(const clGamepadManager&) = delete;
		clGamepadManager& operator=(const clGamepadManager&) = delete;

		bool Initialize(bool bEnableXInput = true, bool bEnablePlayStation = true);
		void Shutdown();

		[[nodiscard]] stGamepadState Poll();
		void SetRumble(float flLow, float flHigh);

		float flStickDeadzone = .18f;
		float flTriggerThreshold = .35f;

	private:
		struct stHidDevice;

		void HidThread();
		void ScanHidDevices();
		[[nodiscard]] bool PollXInput(stGamepadState& rState);

		void* m_hXInput = nullptr;
		void* m_pfnXInputGetState = nullptr;
		void* m_pfnXInputSetState = nullptr;
		std::uint32_t m_uXInputSlot = 0;
		int m_iXInputRescanCountdown = 0;

		std::atomic<bool> m_bRunning = false;
		std::thread m_threadHid;
		std::mutex m_mutexHid;
		std::vector<std::unique_ptr<stHidDevice>> m_aHidDevices;
		stGamepadState m_stHidState;
		bool m_bInitialized = false;
	};
}
