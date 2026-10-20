#include "OmegaGUI/Backends/Vulkan/VulkanLoader.hpp"

#include <Windows.h>

namespace OmegaGUI::Vulkan
{
	bool stFunctions::LoadGlobal(PFN_vkGetInstanceProcAddr pfnGetInstanceProcAddr)
	{
		vkGetInstanceProcAddr = pfnGetInstanceProcAddr;
		if (!vkGetInstanceProcAddr)
		{
			HMODULE hModule = GetModuleHandleW(L"vulkan-1.dll");
			if (!hModule)
				hModule = LoadLibraryW(L"vulkan-1.dll");
			if (!hModule)
				return false;
			vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(reinterpret_cast<void*>(GetProcAddress(hModule, "vkGetInstanceProcAddr")));
		}
		if (!vkGetInstanceProcAddr)
			return false;
#define OMEGAGUI_VK_LOAD(Name) Name = reinterpret_cast<PFN_##Name>(vkGetInstanceProcAddr(VK_NULL_HANDLE, #Name));
		OMEGAGUI_VK_GLOBAL_FUNCTIONS(OMEGAGUI_VK_LOAD)
#undef OMEGAGUI_VK_LOAD
		return vkCreateInstance != nullptr;
	}

	bool stFunctions::LoadInstance(VkInstance hInstance)
	{
		if (!vkGetInstanceProcAddr && !LoadGlobal())
			return false;
#define OMEGAGUI_VK_LOAD(Name) Name = reinterpret_cast<PFN_##Name>(vkGetInstanceProcAddr(hInstance, #Name));
		OMEGAGUI_VK_INSTANCE_FUNCTIONS(OMEGAGUI_VK_LOAD)
#undef OMEGAGUI_VK_LOAD
		return vkGetDeviceProcAddr != nullptr && vkGetPhysicalDeviceMemoryProperties != nullptr;
	}

	bool stFunctions::LoadDevice(VkDevice hDevice)
	{
		if (!vkGetDeviceProcAddr)
			return false;
#define OMEGAGUI_VK_LOAD(Name) Name = reinterpret_cast<PFN_##Name>(vkGetDeviceProcAddr(hDevice, #Name));
		OMEGAGUI_VK_DEVICE_FUNCTIONS(OMEGAGUI_VK_LOAD)
#undef OMEGAGUI_VK_LOAD
		return vkCreateGraphicsPipelines != nullptr && vkCmdDrawIndexed != nullptr && vkAllocateDescriptorSets != nullptr;
	}
}
