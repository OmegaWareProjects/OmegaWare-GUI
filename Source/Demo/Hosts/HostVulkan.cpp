#if OMEGAGUI_BACKEND_VULKAN

#include "Demo/Host.hpp"
#include "OmegaGUI/Backends/Vulkan/BackendVulkan.hpp"

#include <array>
#include <vector>

namespace
{
	constexpr std::uint32_t uFramesInFlight = 2;

	class clHostVulkan final : public clHost
	{
	public:
		~clHostVulkan() override
		{
			if (!m_hDevice)
			{
				if (m_hInstance && m_vk.vkDestroyInstance)
					m_vk.vkDestroyInstance(m_hInstance, nullptr);
				return;
			}
			m_vk.vkDeviceWaitIdle(m_hDevice);
			m_clBackend.Shutdown();
			DestroySwapchain();
			for (stFrame& rFrame : m_aFrames)
			{
				m_vk.vkDestroySemaphore(m_hDevice, rFrame.hImageReady, nullptr);
				m_vk.vkDestroyFence(m_hDevice, rFrame.hFence, nullptr);
			}
			for (VkSemaphore hSemaphore : m_aRenderDone)
				m_vk.vkDestroySemaphore(m_hDevice, hSemaphore, nullptr);
			m_vk.vkDestroyCommandPool(m_hDevice, m_hCommandPool, nullptr);
			m_vk.vkDestroyRenderPass(m_hDevice, m_hRenderPass, nullptr);
			m_vk.vkDestroyDevice(m_hDevice, nullptr);
			m_vk.vkDestroySurfaceKHR(m_hInstance, m_hSurface, nullptr);
			m_vk.vkDestroyInstance(m_hInstance, nullptr);
		}

		bool Create(HWND hWindow, int iWidth, int iHeight) override
		{
			if (!m_vk.LoadGlobal())
				return false;
			const char* aInstanceExtensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
			VkApplicationInfo stApplication{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
			stApplication.pApplicationName = "OmegaGUI Demo";
			stApplication.apiVersion = VK_API_VERSION_1_1;
			VkInstanceCreateInfo stInstance{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
			stInstance.pApplicationInfo = &stApplication;
			stInstance.enabledExtensionCount = 2;
			stInstance.ppEnabledExtensionNames = aInstanceExtensions;
			if (m_vk.vkCreateInstance(&stInstance, nullptr, &m_hInstance) != VK_SUCCESS || !m_vk.LoadInstance(m_hInstance))
				return false;

			VkWin32SurfaceCreateInfoKHR stSurface{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
			stSurface.hinstance = GetModuleHandleW(nullptr);
			stSurface.hwnd = hWindow;
			if (m_vk.vkCreateWin32SurfaceKHR(m_hInstance, &stSurface, nullptr, &m_hSurface) != VK_SUCCESS)
				return false;

			std::uint32_t uDeviceCount = 0;
			m_vk.vkEnumeratePhysicalDevices(m_hInstance, &uDeviceCount, nullptr);
			std::vector<VkPhysicalDevice> aDevices(uDeviceCount);
			m_vk.vkEnumeratePhysicalDevices(m_hInstance, &uDeviceCount, aDevices.data());
			for (const VkPhysicalDevice hCandidate : aDevices)
			{
				std::uint32_t uFamilyCount = 0;
				m_vk.vkGetPhysicalDeviceQueueFamilyProperties(hCandidate, &uFamilyCount, nullptr);
				std::vector<VkQueueFamilyProperties> aFamilies(uFamilyCount);
				m_vk.vkGetPhysicalDeviceQueueFamilyProperties(hCandidate, &uFamilyCount, aFamilies.data());
				for (std::uint32_t i = 0; i < uFamilyCount; ++i)
				{
					VkBool32 bPresent = VK_FALSE;
					m_vk.vkGetPhysicalDeviceSurfaceSupportKHR(hCandidate, i, m_hSurface, &bPresent);
					if ((aFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && bPresent)
					{
						VkPhysicalDeviceProperties stProperties{};
						m_vk.vkGetPhysicalDeviceProperties(hCandidate, &stProperties);
						const bool bDiscrete = stProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
						if (!m_hPhysicalDevice || bDiscrete)
						{
							m_hPhysicalDevice = hCandidate;
							m_uQueueFamily = i;
						}
						break;
					}
				}
			}
			if (!m_hPhysicalDevice)
				return false;

			const float flPriority = 1.f;
			VkDeviceQueueCreateInfo stQueue{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
			stQueue.queueFamilyIndex = m_uQueueFamily;
			stQueue.queueCount = 1;
			stQueue.pQueuePriorities = &flPriority;
			const char* aDeviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
			VkDeviceCreateInfo stDevice{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
			stDevice.queueCreateInfoCount = 1;
			stDevice.pQueueCreateInfos = &stQueue;
			stDevice.enabledExtensionCount = 1;
			stDevice.ppEnabledExtensionNames = aDeviceExtensions;
			if (m_vk.vkCreateDevice(m_hPhysicalDevice, &stDevice, nullptr, &m_hDevice) != VK_SUCCESS || !m_vk.LoadDevice(m_hDevice))
				return false;
			m_vk.vkGetDeviceQueue(m_hDevice, m_uQueueFamily, 0, &m_hQueue);
			m_vk.vkGetPhysicalDeviceMemoryProperties(m_hPhysicalDevice, &m_stMemory);

			std::uint32_t uFormatCount = 0;
			m_vk.vkGetPhysicalDeviceSurfaceFormatsKHR(m_hPhysicalDevice, m_hSurface, &uFormatCount, nullptr);
			std::vector<VkSurfaceFormatKHR> aFormats(uFormatCount);
			m_vk.vkGetPhysicalDeviceSurfaceFormatsKHR(m_hPhysicalDevice, m_hSurface, &uFormatCount, aFormats.data());
			m_stFormat = aFormats.empty() ? VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR } : aFormats.front();
			for (const VkSurfaceFormatKHR& rFormat : aFormats)
				if (rFormat.format == VK_FORMAT_B8G8R8A8_UNORM || rFormat.format == VK_FORMAT_R8G8B8A8_UNORM)
				{
					m_stFormat = rFormat;
					break;
				}

			CreateRenderPass();
			VkCommandPoolCreateInfo stPool{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			stPool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			stPool.queueFamilyIndex = m_uQueueFamily;
			m_vk.vkCreateCommandPool(m_hDevice, &stPool, nullptr, &m_hCommandPool);
			for (stFrame& rFrame : m_aFrames)
			{
				VkCommandBufferAllocateInfo stAllocate{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
				stAllocate.commandPool = m_hCommandPool;
				stAllocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				stAllocate.commandBufferCount = 1;
				m_vk.vkAllocateCommandBuffers(m_hDevice, &stAllocate, &rFrame.hCommand);
				VkSemaphoreCreateInfo stSemaphore{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
				m_vk.vkCreateSemaphore(m_hDevice, &stSemaphore, nullptr, &rFrame.hImageReady);
				VkFenceCreateInfo stFence{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
				stFence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
				m_vk.vkCreateFence(m_hDevice, &stFence, nullptr, &rFrame.hFence);
			}
			if (!CreateSwapchain(iWidth, iHeight))
				return false;

			OmegaGUI::stVulkanConfig stConfig;
			stConfig.pfnGetInstanceProcAddr = m_vk.vkGetInstanceProcAddr;
			stConfig.hInstance = m_hInstance;
			stConfig.hPhysicalDevice = m_hPhysicalDevice;
			stConfig.hDevice = m_hDevice;
			stConfig.uQueueFamily = m_uQueueFamily;
			stConfig.hQueue = m_hQueue;
			stConfig.hRenderPass = m_hRenderPass;
			stConfig.bWorldHasDepth = true;
			stConfig.uFramesInFlight = uFramesInFlight;
			return m_clBackend.Initialize(stConfig);
		}

		void Resize(int iWidth, int iHeight) override
		{
			m_vk.vkDeviceWaitIdle(m_hDevice);
			DestroySwapchain();
			CreateSwapchain(iWidth, iHeight);
		}

		void BeginFrame(const float aClearColor[4]) override
		{
			stFrame& rFrame = m_aFrames[m_uFrame];
			m_vk.vkWaitForFences(m_hDevice, 1, &rFrame.hFence, VK_TRUE, UINT64_MAX);
			VkResult eResult = m_vk.vkAcquireNextImageKHR(m_hDevice, m_hSwapchain, UINT64_MAX, rFrame.hImageReady, VK_NULL_HANDLE, &m_uImage);
			if (eResult == VK_ERROR_OUT_OF_DATE_KHR)
			{
				Resize(static_cast<int>(m_stExtent.width), static_cast<int>(m_stExtent.height));
				eResult = m_vk.vkAcquireNextImageKHR(m_hDevice, m_hSwapchain, UINT64_MAX, rFrame.hImageReady, VK_NULL_HANDLE, &m_uImage);
			}
			m_vk.vkResetFences(m_hDevice, 1, &rFrame.hFence);

			VkCommandBufferBeginInfo stBegin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			stBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			m_vk.vkBeginCommandBuffer(rFrame.hCommand, &stBegin);
			std::array<VkClearValue, 2> aClear{};
			aClear[0].color = { { aClearColor[0], aClearColor[1], aClearColor[2], aClearColor[3] } };
			aClear[1].depthStencil = { 1.f, 0 };
			VkRenderPassBeginInfo stPass{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			stPass.renderPass = m_hRenderPass;
			stPass.framebuffer = m_aImages[m_uImage].hFramebuffer;
			stPass.renderArea = { { 0, 0 }, m_stExtent };
			stPass.clearValueCount = 2;
			stPass.pClearValues = aClear.data();
			m_vk.vkCmdBeginRenderPass(rFrame.hCommand, &stPass, VK_SUBPASS_CONTENTS_INLINE);
			m_clBackend.BeginFrame(rFrame.hCommand, m_stExtent);
		}

		void EndFrame() override
		{
			stFrame& rFrame = m_aFrames[m_uFrame];
			m_vk.vkCmdEndRenderPass(rFrame.hCommand);
			m_vk.vkEndCommandBuffer(rFrame.hCommand);
			const VkPipelineStageFlags eWait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			VkSubmitInfo stSubmit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
			stSubmit.waitSemaphoreCount = 1;
			stSubmit.pWaitSemaphores = &rFrame.hImageReady;
			stSubmit.pWaitDstStageMask = &eWait;
			stSubmit.commandBufferCount = 1;
			stSubmit.pCommandBuffers = &rFrame.hCommand;
			stSubmit.signalSemaphoreCount = 1;
			stSubmit.pSignalSemaphores = &m_aRenderDone[m_uImage];
			m_vk.vkQueueSubmit(m_hQueue, 1, &stSubmit, rFrame.hFence);
			VkPresentInfoKHR stPresent{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
			stPresent.waitSemaphoreCount = 1;
			stPresent.pWaitSemaphores = &m_aRenderDone[m_uImage];
			stPresent.swapchainCount = 1;
			stPresent.pSwapchains = &m_hSwapchain;
			stPresent.pImageIndices = &m_uImage;
			m_vk.vkQueuePresentKHR(m_hQueue, &stPresent);
			m_uFrame = (m_uFrame + 1) % uFramesInFlight;
		}

		OmegaGUI::clRenderBackend* Backend() override { return &m_clBackend; }
		bool ClipSpaceYDown() const override { return true; }

	private:
		struct stFrame
		{
			VkCommandBuffer hCommand = VK_NULL_HANDLE;
			VkSemaphore hImageReady = VK_NULL_HANDLE;
			VkFence hFence = VK_NULL_HANDLE;
		};

		struct stImage
		{
			VkImage hImage = VK_NULL_HANDLE;
			VkImageView hView = VK_NULL_HANDLE;
			VkFramebuffer hFramebuffer = VK_NULL_HANDLE;
		};

		[[nodiscard]] std::uint32_t FindMemoryType(std::uint32_t uBits, VkMemoryPropertyFlags eFlags) const
		{
			for (std::uint32_t i = 0; i < m_stMemory.memoryTypeCount; ++i)
				if ((uBits & (1u << i)) && (m_stMemory.memoryTypes[i].propertyFlags & eFlags) == eFlags)
					return i;
			return 0;
		}

		void CreateRenderPass()
		{
			std::array<VkAttachmentDescription, 2> aAttachments{};
			aAttachments[0].format = m_stFormat.format;
			aAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			aAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			aAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			aAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			aAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			aAttachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			aAttachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			aAttachments[1].format = VK_FORMAT_D32_SFLOAT;
			aAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
			aAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			aAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			aAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			aAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			aAttachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			aAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			const VkAttachmentReference stColor{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			const VkAttachmentReference stDepth{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
			VkSubpassDescription stSubpass{};
			stSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			stSubpass.colorAttachmentCount = 1;
			stSubpass.pColorAttachments = &stColor;
			stSubpass.pDepthStencilAttachment = &stDepth;
			VkSubpassDependency stDependency{};
			stDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			stDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			stDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			stDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			VkRenderPassCreateInfo stInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
			stInfo.attachmentCount = 2;
			stInfo.pAttachments = aAttachments.data();
			stInfo.subpassCount = 1;
			stInfo.pSubpasses = &stSubpass;
			stInfo.dependencyCount = 1;
			stInfo.pDependencies = &stDependency;
			m_vk.vkCreateRenderPass(m_hDevice, &stInfo, nullptr, &m_hRenderPass);
		}

		bool CreateSwapchain(int iWidth, int iHeight)
		{
			VkSurfaceCapabilitiesKHR stCapabilities{};
			m_vk.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_hPhysicalDevice, m_hSurface, &stCapabilities);
			m_stExtent = stCapabilities.currentExtent.width != UINT32_MAX ? stCapabilities.currentExtent : VkExtent2D{ static_cast<std::uint32_t>(iWidth), static_cast<std::uint32_t>(iHeight) };
			if (m_stExtent.width == 0 || m_stExtent.height == 0)
				return false;
			VkSwapchainCreateInfoKHR stInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
			stInfo.surface = m_hSurface;
			stInfo.minImageCount = std::max(stCapabilities.minImageCount, 2u);
			if (stCapabilities.maxImageCount)
				stInfo.minImageCount = std::min(stInfo.minImageCount, stCapabilities.maxImageCount);
			stInfo.imageFormat = m_stFormat.format;
			stInfo.imageColorSpace = m_stFormat.colorSpace;
			stInfo.imageExtent = m_stExtent;
			stInfo.imageArrayLayers = 1;
			stInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			stInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			stInfo.preTransform = stCapabilities.currentTransform;
			stInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			stInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
			stInfo.clipped = VK_TRUE;
			if (m_vk.vkCreateSwapchainKHR(m_hDevice, &stInfo, nullptr, &m_hSwapchain) != VK_SUCCESS)
				return false;

			VkImageCreateInfo stDepth{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			stDepth.imageType = VK_IMAGE_TYPE_2D;
			stDepth.format = VK_FORMAT_D32_SFLOAT;
			stDepth.extent = { m_stExtent.width, m_stExtent.height, 1 };
			stDepth.mipLevels = 1;
			stDepth.arrayLayers = 1;
			stDepth.samples = VK_SAMPLE_COUNT_1_BIT;
			stDepth.tiling = VK_IMAGE_TILING_OPTIMAL;
			stDepth.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			m_vk.vkCreateImage(m_hDevice, &stDepth, nullptr, &m_hDepthImage);
			VkMemoryRequirements stRequirements{};
			m_vk.vkGetImageMemoryRequirements(m_hDevice, m_hDepthImage, &stRequirements);
			VkMemoryAllocateInfo stAllocate{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			stAllocate.allocationSize = stRequirements.size;
			stAllocate.memoryTypeIndex = FindMemoryType(stRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			m_vk.vkAllocateMemory(m_hDevice, &stAllocate, nullptr, &m_hDepthMemory);
			m_vk.vkBindImageMemory(m_hDevice, m_hDepthImage, m_hDepthMemory, 0);
			VkImageViewCreateInfo stDepthView{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			stDepthView.image = m_hDepthImage;
			stDepthView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			stDepthView.format = VK_FORMAT_D32_SFLOAT;
			stDepthView.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
			m_vk.vkCreateImageView(m_hDevice, &stDepthView, nullptr, &m_hDepthView);

			std::uint32_t uCount = 0;
			m_vk.vkGetSwapchainImagesKHR(m_hDevice, m_hSwapchain, &uCount, nullptr);
			std::vector<VkImage> aImages(uCount);
			m_vk.vkGetSwapchainImagesKHR(m_hDevice, m_hSwapchain, &uCount, aImages.data());
			m_aImages.resize(uCount);
			for (std::uint32_t i = 0; i < uCount; ++i)
			{
				stImage& rImage = m_aImages[i];
				rImage.hImage = aImages[i];
				VkImageViewCreateInfo stView{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
				stView.image = rImage.hImage;
				stView.viewType = VK_IMAGE_VIEW_TYPE_2D;
				stView.format = m_stFormat.format;
				stView.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
				m_vk.vkCreateImageView(m_hDevice, &stView, nullptr, &rImage.hView);
				const VkImageView aViews[] = { rImage.hView, m_hDepthView };
				VkFramebufferCreateInfo stFramebuffer{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
				stFramebuffer.renderPass = m_hRenderPass;
				stFramebuffer.attachmentCount = 2;
				stFramebuffer.pAttachments = aViews;
				stFramebuffer.width = m_stExtent.width;
				stFramebuffer.height = m_stExtent.height;
				stFramebuffer.layers = 1;
				m_vk.vkCreateFramebuffer(m_hDevice, &stFramebuffer, nullptr, &rImage.hFramebuffer);
			}
			while (m_aRenderDone.size() < uCount)
			{
				VkSemaphoreCreateInfo stSemaphore{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
				VkSemaphore hSemaphore = VK_NULL_HANDLE;
				m_vk.vkCreateSemaphore(m_hDevice, &stSemaphore, nullptr, &hSemaphore);
				m_aRenderDone.push_back(hSemaphore);
			}
			return true;
		}

		void DestroySwapchain()
		{
			for (stImage& rImage : m_aImages)
			{
				m_vk.vkDestroyFramebuffer(m_hDevice, rImage.hFramebuffer, nullptr);
				m_vk.vkDestroyImageView(m_hDevice, rImage.hView, nullptr);
			}
			m_aImages.clear();
			if (m_hDepthView) m_vk.vkDestroyImageView(m_hDevice, m_hDepthView, nullptr);
			if (m_hDepthImage) m_vk.vkDestroyImage(m_hDevice, m_hDepthImage, nullptr);
			if (m_hDepthMemory) m_vk.vkFreeMemory(m_hDevice, m_hDepthMemory, nullptr);
			if (m_hSwapchain) m_vk.vkDestroySwapchainKHR(m_hDevice, m_hSwapchain, nullptr);
			m_hDepthView = VK_NULL_HANDLE;
			m_hDepthImage = VK_NULL_HANDLE;
			m_hDepthMemory = VK_NULL_HANDLE;
			m_hSwapchain = VK_NULL_HANDLE;
		}

		OmegaGUI::Vulkan::stFunctions m_vk;
		VkInstance m_hInstance = VK_NULL_HANDLE;
		VkSurfaceKHR m_hSurface = VK_NULL_HANDLE;
		VkPhysicalDevice m_hPhysicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceMemoryProperties m_stMemory{};
		VkDevice m_hDevice = VK_NULL_HANDLE;
		std::uint32_t m_uQueueFamily = 0;
		VkQueue m_hQueue = VK_NULL_HANDLE;
		VkSurfaceFormatKHR m_stFormat{};
		VkRenderPass m_hRenderPass = VK_NULL_HANDLE;
		VkSwapchainKHR m_hSwapchain = VK_NULL_HANDLE;
		VkExtent2D m_stExtent{};
		VkImage m_hDepthImage = VK_NULL_HANDLE;
		VkDeviceMemory m_hDepthMemory = VK_NULL_HANDLE;
		VkImageView m_hDepthView = VK_NULL_HANDLE;
		std::vector<stImage> m_aImages;
		std::vector<VkSemaphore> m_aRenderDone;
		VkCommandPool m_hCommandPool = VK_NULL_HANDLE;
		std::array<stFrame, uFramesInFlight> m_aFrames{};
		std::uint32_t m_uFrame = 0;
		std::uint32_t m_uImage = 0;
		OmegaGUI::clBackendVulkan m_clBackend;
	};
}

std::unique_ptr<clHost> CreateHostVulkan()
{
	return std::make_unique<clHostVulkan>();
}

#endif
