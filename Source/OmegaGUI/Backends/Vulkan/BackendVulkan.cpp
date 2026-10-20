#include "OmegaGUI/Backends/Vulkan/BackendVulkan.hpp"

#include <cstring>
#include <type_traits>

namespace OmegaGUI
{
	namespace
	{
#include "OmegaGUI/Backends/Vulkan/Shaders/UIFragment.hpp"
#include "OmegaGUI/Backends/Vulkan/Shaders/UIVertex.hpp"
#include "OmegaGUI/Backends/Vulkan/Shaders/WorldVertex.hpp"

		template <typename T = VkDescriptorSet>
		[[nodiscard]] TextureHandle SetToHandle(T hSet)
		{
			if constexpr (std::is_pointer_v<T>)
				return static_cast<TextureHandle>(reinterpret_cast<std::uintptr_t>(hSet));
			else
				return static_cast<TextureHandle>(hSet);
		}

		template <typename T = VkDescriptorSet>
		[[nodiscard]] T HandleToSet(TextureHandle hTexture)
		{
			if constexpr (std::is_pointer_v<T>)
				return reinterpret_cast<T>(static_cast<std::uintptr_t>(hTexture));
			else
				return static_cast<T>(hTexture);
		}

		[[nodiscard]] VkCompareOp DepthFunction(EDepthBatch eBatch, bool bReversed)
		{
			switch (eBatch)
			{
			case EDepthBatch::Behind: return bReversed ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_GREATER;
			case EDepthBatch::Always: return VK_COMPARE_OP_ALWAYS;
			default: return bReversed ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
			}
		}
	}

	clBackendVulkan::~clBackendVulkan()
	{
		Shutdown();
	}

	bool clBackendVulkan::Initialize(const stVulkanConfig& stConfig)
	{
		m_stConfig = stConfig;
		if (m_stConfig.hWorldRenderPass == VK_NULL_HANDLE)
		{
			m_stConfig.hWorldRenderPass = m_stConfig.hRenderPass;
			m_stConfig.uWorldSubpass = m_stConfig.uSubpass;
		}
		if (!m_stVk.LoadGlobal(stConfig.pfnGetInstanceProcAddr) || !m_stVk.LoadInstance(stConfig.hInstance) || !m_stVk.LoadDevice(stConfig.hDevice))
			return false;
		m_stVk.vkGetPhysicalDeviceMemoryProperties(stConfig.hPhysicalDevice, &m_stMemory);
		m_aFrames.resize(std::max<std::uint32_t>(stConfig.uFramesInFlight, 1));
		m_bInitialized = CreateDeviceObjects();
		return m_bInitialized;
	}

	void clBackendVulkan::Shutdown()
	{
		if (!m_stVk.vkDeviceWaitIdle || !m_stConfig.hDevice)
			return;
		m_stVk.vkDeviceWaitIdle(m_stConfig.hDevice);
		for (stFrameResources& rFrame : m_aFrames)
		{
			DestroyBuffer(rFrame.stVertices);
			DestroyBuffer(rFrame.stIndices);
			DestroyBuffer(rFrame.stWorldVertices);
			DestroyBuffer(rFrame.stWorldIndices);
			for (stTexture& rTexture : rFrame.aPendingDestroy)
				DestroyTextureNow(rTexture);
		}
		m_aFrames.clear();
		for (auto& [hHandle, rTexture] : m_mapTextures)
			DestroyTextureNow(rTexture);
		m_mapTextures.clear();
		InvalidateDeviceObjects();
		m_stConfig.hDevice = VK_NULL_HANDLE;
		m_bInitialized = false;
	}

	std::uint32_t clBackendVulkan::FindMemoryType(std::uint32_t uTypeBits, VkMemoryPropertyFlags eFlags) const
	{
		for (std::uint32_t i = 0; i < m_stMemory.memoryTypeCount; ++i)
			if ((uTypeBits & (1u << i)) && (m_stMemory.memoryTypes[i].propertyFlags & eFlags) == eFlags)
				return i;
		return UINT32_MAX;
	}

	bool clBackendVulkan::CreateDeviceObjects()
	{
		const VkDevice hDevice = m_stConfig.hDevice;
		InvalidateDeviceObjects();

		VkSamplerCreateInfo stSampler{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		stSampler.magFilter = stSampler.minFilter = VK_FILTER_LINEAR;
		stSampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		stSampler.addressModeU = stSampler.addressModeV = stSampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		stSampler.minLod = -1000.f;
		stSampler.maxLod = 1000.f;
		stSampler.maxAnisotropy = 1.f;
		if (m_stVk.vkCreateSampler(hDevice, &stSampler, nullptr, &m_hSampler) != VK_SUCCESS)
			return false;

		VkDescriptorSetLayoutBinding stBinding{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
		VkDescriptorSetLayoutCreateInfo stLayout{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		stLayout.bindingCount = 1;
		stLayout.pBindings = &stBinding;
		if (m_stVk.vkCreateDescriptorSetLayout(hDevice, &stLayout, nullptr, &m_hSetLayout) != VK_SUCCESS)
			return false;

		VkDescriptorPoolSize stPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_stConfig.uMaxTextures };
		VkDescriptorPoolCreateInfo stPool{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		stPool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		stPool.maxSets = m_stConfig.uMaxTextures;
		stPool.poolSizeCount = 1;
		stPool.pPoolSizes = &stPoolSize;
		if (m_stVk.vkCreateDescriptorPool(hDevice, &stPool, nullptr, &m_hDescriptorPool) != VK_SUCCESS)
			return false;

		VkPushConstantRange stPush{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16 };
		VkPipelineLayoutCreateInfo stPipelineLayout{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		stPipelineLayout.setLayoutCount = 1;
		stPipelineLayout.pSetLayouts = &m_hSetLayout;
		stPipelineLayout.pushConstantRangeCount = 1;
		stPipelineLayout.pPushConstantRanges = &stPush;
		if (m_stVk.vkCreatePipelineLayout(hDevice, &stPipelineLayout, nullptr, &m_hPipelineLayout) != VK_SUCCESS)
			return false;

		auto CreateModule = [&](const std::uint32_t* pCode, std::size_t uSize, VkShaderModule& rModule) {
			VkShaderModuleCreateInfo stModule{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
			stModule.codeSize = uSize;
			stModule.pCode = pCode;
			return m_stVk.vkCreateShaderModule(hDevice, &stModule, nullptr, &rModule) == VK_SUCCESS;
		};
		if (!CreateModule(g_aUIVertexSPIRV, sizeof(g_aUIVertexSPIRV), m_hVertexModule) ||
			!CreateModule(g_aWorldVertexSPIRV, sizeof(g_aWorldVertexSPIRV), m_hWorldVertexModule) ||
			!CreateModule(g_aUIFragmentSPIRV, sizeof(g_aUIFragmentSPIRV), m_hFragmentModule))
			return false;

		m_hPipeline = CreatePipeline(m_hVertexModule, m_stConfig.hRenderPass, m_stConfig.uSubpass, false, EDepthBatch::Always, false);
		if (m_hPipeline == VK_NULL_HANDLE)
			return false;

		VkCommandPoolCreateInfo stCommandPool{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		stCommandPool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		stCommandPool.queueFamilyIndex = m_stConfig.uQueueFamily;
		return m_stVk.vkCreateCommandPool(hDevice, &stCommandPool, nullptr, &m_hUploadPool) == VK_SUCCESS;
	}

	void clBackendVulkan::InvalidateDeviceObjects()
	{
		const VkDevice hDevice = m_stConfig.hDevice;
		if (!hDevice || !m_stVk.vkDestroyPipeline)
			return;
		if (m_hPipeline) m_stVk.vkDestroyPipeline(hDevice, m_hPipeline, nullptr);
		for (VkPipeline& hPipeline : m_aWorldPipelines)
		{
			if (hPipeline)
				m_stVk.vkDestroyPipeline(hDevice, hPipeline, nullptr);
			hPipeline = VK_NULL_HANDLE;
		}
		if (m_hVertexModule) m_stVk.vkDestroyShaderModule(hDevice, m_hVertexModule, nullptr);
		if (m_hWorldVertexModule) m_stVk.vkDestroyShaderModule(hDevice, m_hWorldVertexModule, nullptr);
		if (m_hFragmentModule) m_stVk.vkDestroyShaderModule(hDevice, m_hFragmentModule, nullptr);
		if (m_hPipelineLayout) m_stVk.vkDestroyPipelineLayout(hDevice, m_hPipelineLayout, nullptr);
		if (m_hDescriptorPool) m_stVk.vkDestroyDescriptorPool(hDevice, m_hDescriptorPool, nullptr);
		if (m_hSetLayout) m_stVk.vkDestroyDescriptorSetLayout(hDevice, m_hSetLayout, nullptr);
		if (m_hSampler) m_stVk.vkDestroySampler(hDevice, m_hSampler, nullptr);
		if (m_hUploadPool) m_stVk.vkDestroyCommandPool(hDevice, m_hUploadPool, nullptr);
		m_hPipeline = VK_NULL_HANDLE;
		m_hVertexModule = m_hWorldVertexModule = m_hFragmentModule = VK_NULL_HANDLE;
		m_hPipelineLayout = VK_NULL_HANDLE;
		m_hDescriptorPool = VK_NULL_HANDLE;
		m_hSetLayout = VK_NULL_HANDLE;
		m_hSampler = VK_NULL_HANDLE;
		m_hUploadPool = VK_NULL_HANDLE;
	}

	VkPipeline clBackendVulkan::CreatePipeline(VkShaderModule hVertex, VkRenderPass hRenderPass, std::uint32_t uSubpass, bool bWorld, EDepthBatch eBatch, bool bReversed)
	{
		VkPipelineShaderStageCreateInfo aStages[2]{};
		aStages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, hVertex, "main", nullptr };
		aStages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, m_hFragmentModule, "main", nullptr };

		const VkVertexInputBindingDescription stBinding{ 0, bWorld ? static_cast<std::uint32_t>(sizeof(stWorldVertex)) : static_cast<std::uint32_t>(sizeof(stVertex)), VK_VERTEX_INPUT_RATE_VERTEX };
		VkVertexInputAttributeDescription aAttributes[3]{};
		if (bWorld)
		{
			aAttributes[0] = { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<std::uint32_t>(offsetof(stWorldVertex, vecClip)) };
			aAttributes[1] = { 1, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(stWorldVertex, vecUV)) };
			aAttributes[2] = { 2, 0, VK_FORMAT_R8G8B8A8_UNORM, static_cast<std::uint32_t>(offsetof(stWorldVertex, uColor)) };
		}
		else
		{
			aAttributes[0] = { 0, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(stVertex, vecPosition)) };
			aAttributes[1] = { 1, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(stVertex, vecUV)) };
			aAttributes[2] = { 2, 0, VK_FORMAT_R8G8B8A8_UNORM, static_cast<std::uint32_t>(offsetof(stVertex, uColor)) };
		}
		VkPipelineVertexInputStateCreateInfo stVertexInput{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		stVertexInput.vertexBindingDescriptionCount = 1;
		stVertexInput.pVertexBindingDescriptions = &stBinding;
		stVertexInput.vertexAttributeDescriptionCount = 3;
		stVertexInput.pVertexAttributeDescriptions = aAttributes;

		VkPipelineInputAssemblyStateCreateInfo stAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		stAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		VkPipelineViewportStateCreateInfo stViewport{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		stViewport.viewportCount = 1;
		stViewport.scissorCount = 1;
		VkPipelineRasterizationStateCreateInfo stRaster{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		stRaster.polygonMode = VK_POLYGON_MODE_FILL;
		stRaster.cullMode = VK_CULL_MODE_NONE;
		stRaster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		stRaster.lineWidth = 1.f;
		VkPipelineMultisampleStateCreateInfo stMultisample{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		stMultisample.rasterizationSamples = m_stConfig.eSamples;
		VkPipelineColorBlendAttachmentState stAttachment{};
		stAttachment.blendEnable = VK_TRUE;
		stAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		stAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		stAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		stAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		stAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		stAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		stAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		VkPipelineColorBlendStateCreateInfo stBlend{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		stBlend.attachmentCount = 1;
		stBlend.pAttachments = &stAttachment;
		VkPipelineDepthStencilStateCreateInfo stDepth{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		if (bWorld && m_stConfig.bWorldHasDepth)
		{
			stDepth.depthTestEnable = eBatch != EDepthBatch::Always;
			stDepth.depthWriteEnable = eBatch == EDepthBatch::WriteDepth;
			stDepth.depthCompareOp = DepthFunction(eBatch, bReversed);
		}
		const VkDynamicState aDynamic[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo stDynamic{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		stDynamic.dynamicStateCount = 2;
		stDynamic.pDynamicStates = aDynamic;

		VkGraphicsPipelineCreateInfo stPipeline{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		stPipeline.stageCount = 2;
		stPipeline.pStages = aStages;
		stPipeline.pVertexInputState = &stVertexInput;
		stPipeline.pInputAssemblyState = &stAssembly;
		stPipeline.pViewportState = &stViewport;
		stPipeline.pRasterizationState = &stRaster;
		stPipeline.pMultisampleState = &stMultisample;
		stPipeline.pDepthStencilState = &stDepth;
		stPipeline.pColorBlendState = &stBlend;
		stPipeline.pDynamicState = &stDynamic;
		stPipeline.layout = m_hPipelineLayout;
		stPipeline.renderPass = hRenderPass;
		stPipeline.subpass = uSubpass;
		VkPipeline hPipeline = VK_NULL_HANDLE;
		if (m_stVk.vkCreateGraphicsPipelines(m_stConfig.hDevice, VK_NULL_HANDLE, 1, &stPipeline, nullptr, &hPipeline) != VK_SUCCESS)
			return VK_NULL_HANDLE;
		return hPipeline;
	}

	VkPipeline clBackendVulkan::WorldPipeline(EDepthBatch eBatch, bool bReversed)
	{
		VkPipeline& rPipeline = m_aWorldPipelines[static_cast<std::size_t>(eBatch) * 2 + (bReversed ? 1 : 0)];
		if (rPipeline == VK_NULL_HANDLE)
			rPipeline = CreatePipeline(m_hWorldVertexModule, m_stConfig.hWorldRenderPass, m_stConfig.uWorldSubpass, true, eBatch, bReversed);
		return rPipeline;
	}

	bool clBackendVulkan::EnsureBuffer(stBuffer& rBuffer, VkDeviceSize uSize, VkBufferUsageFlags eUsage)
	{
		if (rBuffer.hBuffer && rBuffer.uSize >= uSize)
			return true;
		DestroyBuffer(rBuffer);
		const VkDeviceSize uCapacity = std::max<VkDeviceSize>(uSize + uSize / 2, 65536);
		VkBufferCreateInfo stInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		stInfo.size = uCapacity;
		stInfo.usage = eUsage;
		stInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (m_stVk.vkCreateBuffer(m_stConfig.hDevice, &stInfo, nullptr, &rBuffer.hBuffer) != VK_SUCCESS)
			return false;
		VkMemoryRequirements stRequirements{};
		m_stVk.vkGetBufferMemoryRequirements(m_stConfig.hDevice, rBuffer.hBuffer, &stRequirements);
		VkMemoryAllocateInfo stAllocate{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		stAllocate.allocationSize = stRequirements.size;
		stAllocate.memoryTypeIndex = FindMemoryType(stRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		if (stAllocate.memoryTypeIndex == UINT32_MAX || m_stVk.vkAllocateMemory(m_stConfig.hDevice, &stAllocate, nullptr, &rBuffer.hMemory) != VK_SUCCESS)
		{
			DestroyBuffer(rBuffer);
			return false;
		}
		m_stVk.vkBindBufferMemory(m_stConfig.hDevice, rBuffer.hBuffer, rBuffer.hMemory, 0);
		m_stVk.vkMapMemory(m_stConfig.hDevice, rBuffer.hMemory, 0, VK_WHOLE_SIZE, 0, &rBuffer.pMapped);
		rBuffer.uSize = uCapacity;
		return rBuffer.pMapped != nullptr;
	}

	void clBackendVulkan::DestroyBuffer(stBuffer& rBuffer)
	{
		if (rBuffer.hMemory && rBuffer.pMapped)
			m_stVk.vkUnmapMemory(m_stConfig.hDevice, rBuffer.hMemory);
		if (rBuffer.hBuffer)
			m_stVk.vkDestroyBuffer(m_stConfig.hDevice, rBuffer.hBuffer, nullptr);
		if (rBuffer.hMemory)
			m_stVk.vkFreeMemory(m_stConfig.hDevice, rBuffer.hMemory, nullptr);
		rBuffer = {};
	}

	void clBackendVulkan::DestroyTextureNow(stTexture& rTexture)
	{
		const VkDevice hDevice = m_stConfig.hDevice;
		if (rTexture.hSet)
			m_stVk.vkFreeDescriptorSets(hDevice, m_hDescriptorPool, 1, &rTexture.hSet);
		if (rTexture.hView)
			m_stVk.vkDestroyImageView(hDevice, rTexture.hView, nullptr);
		if (rTexture.hImage)
			m_stVk.vkDestroyImage(hDevice, rTexture.hImage, nullptr);
		if (rTexture.hMemory)
			m_stVk.vkFreeMemory(hDevice, rTexture.hMemory, nullptr);
		rTexture = {};
	}

	TextureHandle clBackendVulkan::CreateTexture(const std::uint8_t* pPixelsRGBA, int iWidth, int iHeight)
	{
		if (!m_bInitialized && !m_hUploadPool)
			return 0;
		const VkDevice hDevice = m_stConfig.hDevice;
		stTexture stNew;

		VkImageCreateInfo stImage{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		stImage.imageType = VK_IMAGE_TYPE_2D;
		stImage.format = VK_FORMAT_R8G8B8A8_UNORM;
		stImage.extent = { static_cast<std::uint32_t>(iWidth), static_cast<std::uint32_t>(iHeight), 1 };
		stImage.mipLevels = 1;
		stImage.arrayLayers = 1;
		stImage.samples = VK_SAMPLE_COUNT_1_BIT;
		stImage.tiling = VK_IMAGE_TILING_OPTIMAL;
		stImage.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		stImage.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		stImage.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		if (m_stVk.vkCreateImage(hDevice, &stImage, nullptr, &stNew.hImage) != VK_SUCCESS)
			return 0;
		VkMemoryRequirements stRequirements{};
		m_stVk.vkGetImageMemoryRequirements(hDevice, stNew.hImage, &stRequirements);
		VkMemoryAllocateInfo stAllocate{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		stAllocate.allocationSize = stRequirements.size;
		stAllocate.memoryTypeIndex = FindMemoryType(stRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (stAllocate.memoryTypeIndex == UINT32_MAX || m_stVk.vkAllocateMemory(hDevice, &stAllocate, nullptr, &stNew.hMemory) != VK_SUCCESS)
		{
			DestroyTextureNow(stNew);
			return 0;
		}
		m_stVk.vkBindImageMemory(hDevice, stNew.hImage, stNew.hMemory, 0);

		VkImageViewCreateInfo stView{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		stView.image = stNew.hImage;
		stView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		stView.format = VK_FORMAT_R8G8B8A8_UNORM;
		stView.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		if (m_stVk.vkCreateImageView(hDevice, &stView, nullptr, &stNew.hView) != VK_SUCCESS)
		{
			DestroyTextureNow(stNew);
			return 0;
		}

		VkDescriptorSetAllocateInfo stSetAllocate{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		stSetAllocate.descriptorPool = m_hDescriptorPool;
		stSetAllocate.descriptorSetCount = 1;
		stSetAllocate.pSetLayouts = &m_hSetLayout;
		if (m_stVk.vkAllocateDescriptorSets(hDevice, &stSetAllocate, &stNew.hSet) != VK_SUCCESS)
		{
			DestroyTextureNow(stNew);
			return 0;
		}
		VkDescriptorImageInfo stImageInfo{ m_hSampler, stNew.hView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkWriteDescriptorSet stWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		stWrite.dstSet = stNew.hSet;
		stWrite.descriptorCount = 1;
		stWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		stWrite.pImageInfo = &stImageInfo;
		m_stVk.vkUpdateDescriptorSets(hDevice, 1, &stWrite, 0, nullptr);

		stBuffer stStaging;
		const VkDeviceSize uBytes = static_cast<VkDeviceSize>(iWidth) * iHeight * 4;
		if (!EnsureBuffer(stStaging, uBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT))
		{
			DestroyTextureNow(stNew);
			return 0;
		}
		std::memcpy(stStaging.pMapped, pPixelsRGBA, static_cast<std::size_t>(uBytes));

		VkCommandBufferAllocateInfo stCommandAllocate{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		stCommandAllocate.commandPool = m_hUploadPool;
		stCommandAllocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		stCommandAllocate.commandBufferCount = 1;
		VkCommandBuffer hCommand = VK_NULL_HANDLE;
		m_stVk.vkAllocateCommandBuffers(hDevice, &stCommandAllocate, &hCommand);
		VkCommandBufferBeginInfo stBegin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		stBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		m_stVk.vkBeginCommandBuffer(hCommand, &stBegin);

		VkImageMemoryBarrier stToCopy{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		stToCopy.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		stToCopy.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		stToCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		stToCopy.srcQueueFamilyIndex = stToCopy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		stToCopy.image = stNew.hImage;
		stToCopy.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		m_stVk.vkCmdPipelineBarrier(hCommand, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &stToCopy);
		VkBufferImageCopy stCopy{};
		stCopy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		stCopy.imageExtent = { static_cast<std::uint32_t>(iWidth), static_cast<std::uint32_t>(iHeight), 1 };
		m_stVk.vkCmdCopyBufferToImage(hCommand, stStaging.hBuffer, stNew.hImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &stCopy);
		VkImageMemoryBarrier stToRead = stToCopy;
		stToRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		stToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		stToRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		stToRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		m_stVk.vkCmdPipelineBarrier(hCommand, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &stToRead);
		m_stVk.vkEndCommandBuffer(hCommand);

		VkSubmitInfo stSubmit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		stSubmit.commandBufferCount = 1;
		stSubmit.pCommandBuffers = &hCommand;
		m_stVk.vkQueueSubmit(m_stConfig.hQueue, 1, &stSubmit, VK_NULL_HANDLE);
		m_stVk.vkQueueWaitIdle(m_stConfig.hQueue);
		m_stVk.vkFreeCommandBuffers(hDevice, m_hUploadPool, 1, &hCommand);
		DestroyBuffer(stStaging);

		const TextureHandle hResult = SetToHandle(stNew.hSet);
		m_mapTextures[hResult] = stNew;
		return hResult;
	}

	void clBackendVulkan::DestroyTexture(TextureHandle hTexture)
	{
		const auto itTexture = m_mapTextures.find(hTexture);
		if (itTexture == m_mapTextures.end())
			return;
		if (m_aFrames.empty())
			DestroyTextureNow(itTexture->second);
		else
			m_aFrames[m_uFrameIndex].aPendingDestroy.push_back(itTexture->second);
		m_mapTextures.erase(itTexture);
	}

	void clBackendVulkan::BeginFrame(VkCommandBuffer hCommandBuffer, VkExtent2D stExtent)
	{
		m_hCommandBuffer = hCommandBuffer;
		m_stExtent = stExtent;
		if (m_aFrames.empty())
			return;
		m_uFrameIndex = (m_uFrameIndex + 1) % m_aFrames.size();
		for (stTexture& rTexture : m_aFrames[m_uFrameIndex].aPendingDestroy)
			DestroyTextureNow(rTexture);
		m_aFrames[m_uFrameIndex].aPendingDestroy.clear();
	}

	void clBackendVulkan::RenderDrawData(const stDrawData& stData)
	{
		if (!m_hCommandBuffer || !m_hPipeline || stData.uTotalIndices == 0 || stData.vecDisplaySize.x <= 0.f || stData.vecDisplaySize.y <= 0.f)
			return;
		stFrameResources& rFrame = m_aFrames[m_uFrameIndex];
		const VkDeviceSize uVertexBytes = stData.uTotalVertices * sizeof(stVertex);
		const VkDeviceSize uIndexBytes = stData.uTotalIndices * sizeof(std::uint32_t);
		if (!EnsureBuffer(rFrame.stVertices, uVertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) || !EnsureBuffer(rFrame.stIndices, uIndexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
			return;
		auto* pVertexOut = static_cast<stVertex*>(rFrame.stVertices.pMapped);
		auto* pIndexOut = static_cast<std::uint32_t*>(rFrame.stIndices.pMapped);
		for (const clDrawList* pList : stData.aLists)
		{
			std::memcpy(pVertexOut, pList->GetVertices().data(), pList->GetVertices().size() * sizeof(stVertex));
			std::memcpy(pIndexOut, pList->GetIndices().data(), pList->GetIndices().size() * sizeof(std::uint32_t));
			pVertexOut += pList->GetVertices().size();
			pIndexOut += pList->GetIndices().size();
		}

		const VkCommandBuffer hCommand = m_hCommandBuffer;
		m_stVk.vkCmdBindPipeline(hCommand, VK_PIPELINE_BIND_POINT_GRAPHICS, m_hPipeline);
		const VkDeviceSize uOffset = 0;
		m_stVk.vkCmdBindVertexBuffers(hCommand, 0, 1, &rFrame.stVertices.hBuffer, &uOffset);
		m_stVk.vkCmdBindIndexBuffer(hCommand, rFrame.stIndices.hBuffer, 0, VK_INDEX_TYPE_UINT32);
		const VkViewport stViewport{ 0.f, 0.f, stData.vecDisplaySize.x, stData.vecDisplaySize.y, 0.f, 1.f };
		m_stVk.vkCmdSetViewport(hCommand, 0, 1, &stViewport);
		const auto aProjection = stData.OrthoProjection();
		m_stVk.vkCmdPushConstants(hCommand, m_hPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16, aProjection.data());

		std::uint32_t uGlobalIndex = 0;
		std::int32_t iGlobalVertex = 0;
		TextureHandle hBound = 0;
		for (const clDrawList* pList : stData.aLists)
		{
			for (const stDrawCommand& rCommand : pList->GetCommands())
			{
				if (rCommand.uIndexCount == 0)
					continue;
				const rect rcClip = rCommand.rcClip.Clipped({ stData.vecDisplayPosition, stData.vecDisplayPosition + stData.vecDisplaySize });
				if (rcClip.IsEmpty())
					continue;
				const VkRect2D stScissor{ { static_cast<std::int32_t>(rcClip.min.x - stData.vecDisplayPosition.x), static_cast<std::int32_t>(rcClip.min.y - stData.vecDisplayPosition.y) },
					{ static_cast<std::uint32_t>(rcClip.Width()), static_cast<std::uint32_t>(rcClip.Height()) } };
				m_stVk.vkCmdSetScissor(hCommand, 0, 1, &stScissor);
				if (rCommand.hTexture != hBound)
				{
					const VkDescriptorSet hSet = HandleToSet(rCommand.hTexture);
					m_stVk.vkCmdBindDescriptorSets(hCommand, VK_PIPELINE_BIND_POINT_GRAPHICS, m_hPipelineLayout, 0, 1, &hSet, 0, nullptr);
					hBound = rCommand.hTexture;
				}
				m_stVk.vkCmdDrawIndexed(hCommand, rCommand.uIndexCount, 1, uGlobalIndex + rCommand.uIndexOffset, iGlobalVertex, 0);
			}
			uGlobalIndex += static_cast<std::uint32_t>(pList->GetIndices().size());
			iGlobalVertex += static_cast<std::int32_t>(pList->GetVertices().size());
		}
	}

	void clBackendVulkan::RenderWorld(const clWorldDrawList& rList, const stWorldPass& stPass)
	{
		if (!m_hCommandBuffer || !m_hPipelineLayout || rList.IsEmpty() || m_stExtent.width == 0 || m_stExtent.height == 0)
			return;
		stFrameResources& rFrame = m_aFrames[m_uFrameIndex];
		std::size_t uVertices = 0, uIndices = 0;
		for (std::size_t i = 0; i < static_cast<std::size_t>(EDepthBatch::Count); ++i)
		{
			uVertices += rList.GetBatch(static_cast<EDepthBatch>(i)).aVertices.size();
			uIndices += rList.GetBatch(static_cast<EDepthBatch>(i)).aIndices.size();
		}
		if (!EnsureBuffer(rFrame.stWorldVertices, uVertices * sizeof(stWorldVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ||
			!EnsureBuffer(rFrame.stWorldIndices, uIndices * sizeof(std::uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
			return;
		auto* pVertexOut = static_cast<stWorldVertex*>(rFrame.stWorldVertices.pMapped);
		auto* pIndexOut = static_cast<std::uint32_t*>(rFrame.stWorldIndices.pMapped);
		for (std::size_t i = 0; i < static_cast<std::size_t>(EDepthBatch::Count); ++i)
		{
			const stWorldBatch& rBatch = rList.GetBatch(static_cast<EDepthBatch>(i));
			std::memcpy(pVertexOut, rBatch.aVertices.data(), rBatch.aVertices.size() * sizeof(stWorldVertex));
			std::memcpy(pIndexOut, rBatch.aIndices.data(), rBatch.aIndices.size() * sizeof(std::uint32_t));
			pVertexOut += rBatch.aVertices.size();
			pIndexOut += rBatch.aIndices.size();
		}

		const VkCommandBuffer hCommand = m_hCommandBuffer;
		const VkDeviceSize uOffset = 0;
		m_stVk.vkCmdBindVertexBuffers(hCommand, 0, 1, &rFrame.stWorldVertices.hBuffer, &uOffset);
		m_stVk.vkCmdBindIndexBuffer(hCommand, rFrame.stWorldIndices.hBuffer, 0, VK_INDEX_TYPE_UINT32);
		const VkViewport stViewport{ 0.f, 0.f, static_cast<float>(m_stExtent.width), static_cast<float>(m_stExtent.height), 0.f, 1.f };
		const VkRect2D stScissor{ { 0, 0 }, m_stExtent };
		m_stVk.vkCmdSetViewport(hCommand, 0, 1, &stViewport);
		m_stVk.vkCmdSetScissor(hCommand, 0, 1, &stScissor);
		const VkDescriptorSet hSet = HandleToSet(rList.GetTexture());

		std::uint32_t uIndexBase = 0;
		std::int32_t iVertexBase = 0;
		bool bSetBound = false;
		for (std::size_t i = 0; i < static_cast<std::size_t>(EDepthBatch::Count); ++i)
		{
			const auto eBatch = static_cast<EDepthBatch>(i);
			const stWorldBatch& rBatch = rList.GetBatch(eBatch);
			const bool bDrawable = m_stConfig.bWorldHasDepth || eBatch == EDepthBatch::Always || eBatch == EDepthBatch::Tested;
			if (!rBatch.aIndices.empty() && bDrawable)
				if (const VkPipeline hPipeline = WorldPipeline(eBatch, stPass.bReversedZ))
				{
					m_stVk.vkCmdBindPipeline(hCommand, VK_PIPELINE_BIND_POINT_GRAPHICS, hPipeline);
					if (!bSetBound)
					{
						m_stVk.vkCmdBindDescriptorSets(hCommand, VK_PIPELINE_BIND_POINT_GRAPHICS, m_hPipelineLayout, 0, 1, &hSet, 0, nullptr);
						bSetBound = true;
					}
					m_stVk.vkCmdDrawIndexed(hCommand, static_cast<std::uint32_t>(rBatch.aIndices.size()), 1, uIndexBase, iVertexBase, 0);
				}
			uIndexBase += static_cast<std::uint32_t>(rBatch.aIndices.size());
			iVertexBase += static_cast<std::int32_t>(rBatch.aVertices.size());
		}
	}
}
