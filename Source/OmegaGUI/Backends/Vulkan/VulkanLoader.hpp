#pragma once

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

#define OMEGAGUI_VK_GLOBAL_FUNCTIONS(X) \
	X(vkCreateInstance) \
	X(vkEnumerateInstanceExtensionProperties) \
	X(vkEnumerateInstanceLayerProperties)

#define OMEGAGUI_VK_INSTANCE_FUNCTIONS(X) \
	X(vkDestroyInstance) \
	X(vkEnumeratePhysicalDevices) \
	X(vkGetPhysicalDeviceProperties) \
	X(vkGetPhysicalDeviceQueueFamilyProperties) \
	X(vkGetPhysicalDeviceMemoryProperties) \
	X(vkEnumerateDeviceExtensionProperties) \
	X(vkCreateDevice) \
	X(vkGetDeviceProcAddr) \
	X(vkDestroySurfaceKHR) \
	X(vkGetPhysicalDeviceSurfaceSupportKHR) \
	X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
	X(vkGetPhysicalDeviceSurfaceFormatsKHR) \
	X(vkCreateWin32SurfaceKHR)

#define OMEGAGUI_VK_DEVICE_FUNCTIONS(X) \
	X(vkDestroyDevice) \
	X(vkGetDeviceQueue) \
	X(vkDeviceWaitIdle) \
	X(vkQueueSubmit) \
	X(vkQueueWaitIdle) \
	X(vkCreateBuffer) \
	X(vkDestroyBuffer) \
	X(vkGetBufferMemoryRequirements) \
	X(vkBindBufferMemory) \
	X(vkCreateImage) \
	X(vkDestroyImage) \
	X(vkGetImageMemoryRequirements) \
	X(vkBindImageMemory) \
	X(vkCreateImageView) \
	X(vkDestroyImageView) \
	X(vkAllocateMemory) \
	X(vkFreeMemory) \
	X(vkMapMemory) \
	X(vkUnmapMemory) \
	X(vkFlushMappedMemoryRanges) \
	X(vkCreateSampler) \
	X(vkDestroySampler) \
	X(vkCreateDescriptorSetLayout) \
	X(vkDestroyDescriptorSetLayout) \
	X(vkCreateDescriptorPool) \
	X(vkDestroyDescriptorPool) \
	X(vkAllocateDescriptorSets) \
	X(vkFreeDescriptorSets) \
	X(vkUpdateDescriptorSets) \
	X(vkCreatePipelineLayout) \
	X(vkDestroyPipelineLayout) \
	X(vkCreateShaderModule) \
	X(vkDestroyShaderModule) \
	X(vkCreateGraphicsPipelines) \
	X(vkDestroyPipeline) \
	X(vkCreateRenderPass) \
	X(vkDestroyRenderPass) \
	X(vkCreateFramebuffer) \
	X(vkDestroyFramebuffer) \
	X(vkCreateCommandPool) \
	X(vkDestroyCommandPool) \
	X(vkResetCommandPool) \
	X(vkAllocateCommandBuffers) \
	X(vkFreeCommandBuffers) \
	X(vkBeginCommandBuffer) \
	X(vkEndCommandBuffer) \
	X(vkCreateSemaphore) \
	X(vkDestroySemaphore) \
	X(vkCreateFence) \
	X(vkDestroyFence) \
	X(vkWaitForFences) \
	X(vkResetFences) \
	X(vkCmdPipelineBarrier) \
	X(vkCmdCopyBufferToImage) \
	X(vkCmdBeginRenderPass) \
	X(vkCmdEndRenderPass) \
	X(vkCmdBindPipeline) \
	X(vkCmdBindDescriptorSets) \
	X(vkCmdBindVertexBuffers) \
	X(vkCmdBindIndexBuffer) \
	X(vkCmdSetViewport) \
	X(vkCmdSetScissor) \
	X(vkCmdPushConstants) \
	X(vkCmdDrawIndexed) \
	X(vkCreateSwapchainKHR) \
	X(vkDestroySwapchainKHR) \
	X(vkGetSwapchainImagesKHR) \
	X(vkAcquireNextImageKHR) \
	X(vkQueuePresentKHR)

namespace OmegaGUI::Vulkan
{
	struct stFunctions
	{
		PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
#define OMEGAGUI_VK_DECLARE(Name) PFN_##Name Name = nullptr;
		OMEGAGUI_VK_GLOBAL_FUNCTIONS(OMEGAGUI_VK_DECLARE)
		OMEGAGUI_VK_INSTANCE_FUNCTIONS(OMEGAGUI_VK_DECLARE)
		OMEGAGUI_VK_DEVICE_FUNCTIONS(OMEGAGUI_VK_DECLARE)
#undef OMEGAGUI_VK_DECLARE

		bool LoadGlobal(PFN_vkGetInstanceProcAddr pfnGetInstanceProcAddr = nullptr);
		bool LoadInstance(VkInstance hInstance);
		bool LoadDevice(VkDevice hDevice);
	};
}
