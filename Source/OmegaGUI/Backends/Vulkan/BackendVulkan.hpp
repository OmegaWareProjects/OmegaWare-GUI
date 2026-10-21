#pragma once

#include "OmegaGUI/Backends/Vulkan/VulkanLoader.hpp"
#include "OmegaGUI/Render/Backend.hpp"

#include <array>
#include <unordered_map>
#include <vector>

namespace OmegaGUI
{
	struct stVulkanConfig
	{
		PFN_vkGetInstanceProcAddr pfnGetInstanceProcAddr = nullptr;
		VkInstance hInstance = VK_NULL_HANDLE;
		VkPhysicalDevice hPhysicalDevice = VK_NULL_HANDLE;
		VkDevice hDevice = VK_NULL_HANDLE;
		std::uint32_t uQueueFamily = 0;
		VkQueue hQueue = VK_NULL_HANDLE;
		VkRenderPass hRenderPass = VK_NULL_HANDLE;
		std::uint32_t uSubpass = 0;
		VkFormat eTargetFormat = VK_FORMAT_UNDEFINED;
		VkRenderPass hWorldRenderPass = VK_NULL_HANDLE;
		std::uint32_t uWorldSubpass = 0;
		bool bWorldHasDepth = true;
		VkSampleCountFlagBits eSamples = VK_SAMPLE_COUNT_1_BIT;
		std::uint32_t uFramesInFlight = 2;
		std::uint32_t uMaxTextures = 64;
	};

	class clBackendVulkan final : public clRenderBackend
	{
	public:
		~clBackendVulkan() override;

		bool Initialize(const stVulkanConfig& stConfig);
		void Shutdown();
		void BeginFrame(VkCommandBuffer hCommandBuffer, VkExtent2D stExtent);

		[[nodiscard]] const Vulkan::stFunctions& Functions() const { return m_stVk; }
		[[nodiscard]] std::string_view GetName() const override { return "Vulkan"; }
		[[nodiscard]] TextureHandle CreateTexture(const std::uint8_t* pPixelsRGBA, int iWidth, int iHeight) override;
		void DestroyTexture(TextureHandle hTexture) override;
		void RenderDrawData(const stDrawData& stData) override;
		void RenderWorld(const clWorldDrawList& rList, const stWorldPass& stPass) override;
		void InvalidateDeviceObjects() override;
		bool CreateDeviceObjects() override;

	private:
		struct stBuffer
		{
			VkBuffer hBuffer = VK_NULL_HANDLE;
			VkDeviceMemory hMemory = VK_NULL_HANDLE;
			VkDeviceSize uSize = 0;
			void* pMapped = nullptr;
		};

		struct stTexture
		{
			VkImage hImage = VK_NULL_HANDLE;
			VkImageView hView = VK_NULL_HANDLE;
			VkDeviceMemory hMemory = VK_NULL_HANDLE;
			VkDescriptorSet hSet = VK_NULL_HANDLE;
		};

		struct stFrameResources
		{
			stBuffer stVertices;
			stBuffer stIndices;
			stBuffer stWorldVertices;
			stBuffer stWorldIndices;
			std::vector<stTexture> aPendingDestroy;
		};

		[[nodiscard]] std::uint32_t FindMemoryType(std::uint32_t uTypeBits, VkMemoryPropertyFlags eFlags) const;
		bool EnsureBuffer(stBuffer& rBuffer, VkDeviceSize uSize, VkBufferUsageFlags eUsage);
		void DestroyBuffer(stBuffer& rBuffer);
		void DestroyTextureNow(stTexture& rTexture);
		VkPipeline CreatePipeline(VkShaderModule hVertex, VkRenderPass hRenderPass, std::uint32_t uSubpass, bool bWorld, EDepthBatch eBatch, bool bReversed);
		VkPipeline WorldPipeline(EDepthBatch eBatch, bool bReversed);

		Vulkan::stFunctions m_stVk;
		stVulkanConfig m_stConfig;
		VkPhysicalDeviceMemoryProperties m_stMemory{};
		VkSampler m_hSampler = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_hSetLayout = VK_NULL_HANDLE;
		VkDescriptorPool m_hDescriptorPool = VK_NULL_HANDLE;
		VkPipelineLayout m_hPipelineLayout = VK_NULL_HANDLE;
		VkShaderModule m_hVertexModule = VK_NULL_HANDLE;
		VkShaderModule m_hWorldVertexModule = VK_NULL_HANDLE;
		VkShaderModule m_hFragmentModule = VK_NULL_HANDLE;
		VkPipeline m_hPipeline = VK_NULL_HANDLE;
		std::array<VkPipeline, 8> m_aWorldPipelines{};
		VkCommandPool m_hUploadPool = VK_NULL_HANDLE;
		std::unordered_map<TextureHandle, stTexture> m_mapTextures;
		std::vector<stFrameResources> m_aFrames;
		std::size_t m_uFrameIndex = 0;
		VkCommandBuffer m_hCommandBuffer = VK_NULL_HANDLE;
		VkExtent2D m_stExtent{};
		bool m_bInitialized = false;
	};
}
