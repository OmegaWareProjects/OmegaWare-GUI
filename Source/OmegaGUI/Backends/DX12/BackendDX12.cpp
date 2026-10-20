#include "OmegaGUI/Backends/DX12/BackendDX12.hpp"
#include "OmegaGUI/Backends/Shaders.hpp"

#include <d3dcompiler.h>

#include <cstring>

namespace OmegaGUI
{
	namespace
	{
		[[nodiscard]] Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const char* szSource, std::size_t uLength, const char* szTarget)
		{
			Microsoft::WRL::ComPtr<ID3DBlob> pBlob;
			Microsoft::WRL::ComPtr<ID3DBlob> pErrors;
			if (FAILED(D3DCompile(szSource, uLength, nullptr, nullptr, nullptr, "main", szTarget, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pBlob, &pErrors)))
				return nullptr;
			return pBlob;
		}

		[[nodiscard]] D3D12_COMPARISON_FUNC DepthFunction(EDepthBatch eBatch, bool bReversed)
		{
			switch (eBatch)
			{
			case EDepthBatch::Behind: return bReversed ? D3D12_COMPARISON_FUNC_LESS : D3D12_COMPARISON_FUNC_GREATER;
			case EDepthBatch::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
			default: return bReversed ? D3D12_COMPARISON_FUNC_GREATER_EQUAL : D3D12_COMPARISON_FUNC_LESS_EQUAL;
			}
		}

		[[nodiscard]] D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* pResource, D3D12_RESOURCE_STATES eBefore, D3D12_RESOURCE_STATES eAfter)
		{
			D3D12_RESOURCE_BARRIER stBarrier{};
			stBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			stBarrier.Transition.pResource = pResource;
			stBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			stBarrier.Transition.StateBefore = eBefore;
			stBarrier.Transition.StateAfter = eAfter;
			return stBarrier;
		}

		[[nodiscard]] D3D12_BLEND_DESC AlphaBlend()
		{
			D3D12_BLEND_DESC stBlend{};
			stBlend.RenderTarget[0].BlendEnable = TRUE;
			stBlend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			stBlend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			stBlend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			stBlend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			stBlend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			stBlend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			stBlend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			return stBlend;
		}

		[[nodiscard]] D3D12_RASTERIZER_DESC Rasterizer()
		{
			D3D12_RASTERIZER_DESC stRasterizer{};
			stRasterizer.FillMode = D3D12_FILL_MODE_SOLID;
			stRasterizer.CullMode = D3D12_CULL_MODE_NONE;
			stRasterizer.DepthClipEnable = TRUE;
			return stRasterizer;
		}
	}

	clBackendDX12::~clBackendDX12()
	{
		Shutdown();
	}

	bool clBackendDX12::Initialize(const stDX12Config& stConfig)
	{
		m_stConfig = stConfig;
		m_pDevice = stConfig.pDevice;
		m_pQueue = stConfig.pQueue;
		m_aFrames.resize(static_cast<std::size_t>(std::max(stConfig.iFramesInFlight, 1)));

		D3D12_DESCRIPTOR_HEAP_DESC stHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, stConfig.uMaxTextures, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 };
		if (FAILED(m_pDevice->CreateDescriptorHeap(&stHeap, IID_PPV_ARGS(&m_pHeap))))
			return false;
		m_uDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		m_aSlotUsed.assign(stConfig.uMaxTextures, false);

		if (FAILED(m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pUploadAllocator))) ||
			FAILED(m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pUploadAllocator.Get(), nullptr, IID_PPV_ARGS(&m_pUploadList))) ||
			FAILED(m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pUploadFence))))
			return false;
		m_pUploadList->Close();
		m_hUploadEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		return CreateDeviceObjects();
	}

	void clBackendDX12::Shutdown()
	{
		if (!m_pDevice)
			return;
		WaitForUploads();
		InvalidateDeviceObjects();
		m_mapTextures.clear();
		m_aFrames.clear();
		m_pUploadList.Reset();
		m_pUploadAllocator.Reset();
		m_pUploadFence.Reset();
		if (m_hUploadEvent)
			CloseHandle(m_hUploadEvent);
		m_hUploadEvent = nullptr;
		m_pHeap.Reset();
		m_pQueue.Reset();
		m_pDevice.Reset();
	}

	void clBackendDX12::WaitForUploads()
	{
		if (!m_pUploadFence || m_pUploadFence->GetCompletedValue() >= m_uUploadFenceValue)
			return;
		m_pUploadFence->SetEventOnCompletion(m_uUploadFenceValue, m_hUploadEvent);
		WaitForSingleObject(m_hUploadEvent, INFINITE);
	}

	bool clBackendDX12::CreateDeviceObjects()
	{
		if (!m_pDevice)
			return false;
		InvalidateDeviceObjects();

		D3D12_DESCRIPTOR_RANGE stRange{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0 };
		D3D12_ROOT_PARAMETER aParameters[2]{};
		aParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		aParameters[0].Constants = { 0, 0, 16 };
		aParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		aParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		aParameters[1].DescriptorTable = { 1, &stRange };
		aParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		D3D12_STATIC_SAMPLER_DESC stSampler{};
		stSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		stSampler.AddressU = stSampler.AddressV = stSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		stSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		stSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		stSampler.MaxLOD = D3D12_FLOAT32_MAX;
		stSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		D3D12_ROOT_SIGNATURE_DESC stRoot{ 2, aParameters, 1, &stSampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS };
		ComPtr<ID3DBlob> pSerialized;
		ComPtr<ID3DBlob> pErrors;
		if (FAILED(D3D12SerializeRootSignature(&stRoot, D3D_ROOT_SIGNATURE_VERSION_1, &pSerialized, &pErrors)) ||
			FAILED(m_pDevice->CreateRootSignature(0, pSerialized->GetBufferPointer(), pSerialized->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature))))
			return false;

		m_pVertexShader = CompileShader(Shaders::szVertexHLSL, sizeof(Shaders::szVertexHLSL) - 1, "vs_5_0");
		m_pWorldVertexShader = CompileShader(Shaders::szWorldVertexHLSL, sizeof(Shaders::szWorldVertexHLSL) - 1, "vs_5_0");
		m_pPixelShader = CompileShader(Shaders::szPixelHLSL, sizeof(Shaders::szPixelHLSL) - 1, "ps_5_0");
		if (!m_pVertexShader || !m_pWorldVertexShader || !m_pPixelShader)
			return false;

		const D3D12_INPUT_ELEMENT_DESC aLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(stVertex, vecPosition), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(stVertex, vecUV), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(stVertex, uColor), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC stPipeline{};
		stPipeline.pRootSignature = m_pRootSignature.Get();
		stPipeline.VS = { m_pVertexShader->GetBufferPointer(), m_pVertexShader->GetBufferSize() };
		stPipeline.PS = { m_pPixelShader->GetBufferPointer(), m_pPixelShader->GetBufferSize() };
		stPipeline.BlendState = AlphaBlend();
		stPipeline.SampleMask = UINT_MAX;
		stPipeline.RasterizerState = Rasterizer();
		stPipeline.InputLayout = { aLayout, 3 };
		stPipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		stPipeline.NumRenderTargets = 1;
		stPipeline.RTVFormats[0] = m_stConfig.eRenderTargetFormat;
		stPipeline.SampleDesc.Count = 1;
		return SUCCEEDED(m_pDevice->CreateGraphicsPipelineState(&stPipeline, IID_PPV_ARGS(&m_pPipeline)));
	}

	void clBackendDX12::InvalidateDeviceObjects()
	{
		m_pPipeline.Reset();
		for (auto& pPipeline : m_aWorldPipelines)
			pPipeline.Reset();
		m_eWorldDepthFormat = DXGI_FORMAT_UNKNOWN;
		m_pRootSignature.Reset();
	}

	ID3D12PipelineState* clBackendDX12::WorldPipeline(EDepthBatch eBatch, bool bReversed)
	{
		ComPtr<ID3D12PipelineState>& rPipeline = m_aWorldPipelines[static_cast<std::size_t>(eBatch) * 2 + (bReversed ? 1 : 0)];
		if (rPipeline)
			return rPipeline.Get();

		const D3D12_INPUT_ELEMENT_DESC aLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(stWorldVertex, vecClip), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(stWorldVertex, vecUV), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(stWorldVertex, uColor), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC stPipeline{};
		stPipeline.pRootSignature = m_pRootSignature.Get();
		stPipeline.VS = { m_pWorldVertexShader->GetBufferPointer(), m_pWorldVertexShader->GetBufferSize() };
		stPipeline.PS = { m_pPixelShader->GetBufferPointer(), m_pPixelShader->GetBufferSize() };
		stPipeline.BlendState = AlphaBlend();
		stPipeline.SampleMask = UINT_MAX;
		stPipeline.RasterizerState = Rasterizer();
		stPipeline.InputLayout = { aLayout, 3 };
		stPipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		stPipeline.NumRenderTargets = 1;
		stPipeline.RTVFormats[0] = m_stConfig.eRenderTargetFormat;
		stPipeline.SampleDesc.Count = 1;
		if (m_eWorldDepthFormat != DXGI_FORMAT_UNKNOWN)
		{
			stPipeline.DSVFormat = m_eWorldDepthFormat;
			stPipeline.DepthStencilState.DepthEnable = eBatch != EDepthBatch::Always;
			stPipeline.DepthStencilState.DepthWriteMask = eBatch == EDepthBatch::WriteDepth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
			stPipeline.DepthStencilState.DepthFunc = DepthFunction(eBatch, bReversed);
		}
		m_pDevice->CreateGraphicsPipelineState(&stPipeline, IID_PPV_ARGS(&rPipeline));
		return rPipeline.Get();
	}

	void clBackendDX12::BeginFrame(ID3D12GraphicsCommandList* pCommandList)
	{
		m_pCommandList = pCommandList;
		m_uFrameIndex = (m_uFrameIndex + 1) % m_aFrames.size();
		stFrameResources& rFrame = m_aFrames[m_uFrameIndex];
		rFrame.aPendingRelease.clear();
		for (const UINT uSlot : rFrame.aPendingSlots)
			m_aSlotUsed[uSlot] = false;
		rFrame.aPendingSlots.clear();
	}

	void clBackendDX12::SetWorldTarget(D3D12_CPU_DESCRIPTOR_HANDLE hRenderTarget, D3D12_CPU_DESCRIPTOR_HANDLE hDepthStencil, DXGI_FORMAT eDepthFormat, UINT uWidth, UINT uHeight)
	{
		m_hWorldRenderTarget = hRenderTarget;
		m_hWorldDepth = hDepthStencil;
		m_uWorldWidth = uWidth;
		m_uWorldHeight = uHeight;
		if (eDepthFormat != m_eWorldDepthFormat)
		{
			for (auto& pPipeline : m_aWorldPipelines)
				pPipeline.Reset();
			m_eWorldDepthFormat = eDepthFormat;
		}
	}

	TextureHandle clBackendDX12::CreateTexture(const std::uint8_t* pPixelsRGBA, int iWidth, int iHeight)
	{
		if (!m_pDevice)
			return 0;
		const auto itSlot = std::ranges::find(m_aSlotUsed, false);
		if (itSlot == m_aSlotUsed.end())
			return 0;
		const auto uSlot = static_cast<UINT>(itSlot - m_aSlotUsed.begin());

		D3D12_HEAP_PROPERTIES stDefault{ D3D12_HEAP_TYPE_DEFAULT };
		D3D12_RESOURCE_DESC stDesc{};
		stDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		stDesc.Width = static_cast<UINT64>(iWidth);
		stDesc.Height = static_cast<UINT>(iHeight);
		stDesc.DepthOrArraySize = 1;
		stDesc.MipLevels = 1;
		stDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		stDesc.SampleDesc.Count = 1;
		stDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		ComPtr<ID3D12Resource> pTexture;
		if (FAILED(m_pDevice->CreateCommittedResource(&stDefault, D3D12_HEAP_FLAG_NONE, &stDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&pTexture))))
			return 0;

		const UINT uPitch = (static_cast<UINT>(iWidth) * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
		const UINT uUploadSize = uPitch * static_cast<UINT>(iHeight);
		D3D12_HEAP_PROPERTIES stUploadHeap{ D3D12_HEAP_TYPE_UPLOAD };
		D3D12_RESOURCE_DESC stBuffer{};
		stBuffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		stBuffer.Width = uUploadSize;
		stBuffer.Height = 1;
		stBuffer.DepthOrArraySize = 1;
		stBuffer.MipLevels = 1;
		stBuffer.SampleDesc.Count = 1;
		stBuffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ComPtr<ID3D12Resource> pUpload;
		if (FAILED(m_pDevice->CreateCommittedResource(&stUploadHeap, D3D12_HEAP_FLAG_NONE, &stBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&pUpload))))
			return 0;
		void* pMapped = nullptr;
		const D3D12_RANGE stNoRead{ 0, 0 };
		if (FAILED(pUpload->Map(0, &stNoRead, &pMapped)))
			return 0;
		for (int y = 0; y < iHeight; ++y)
			std::memcpy(static_cast<std::uint8_t*>(pMapped) + static_cast<std::size_t>(y) * uPitch, pPixelsRGBA + static_cast<std::size_t>(y) * iWidth * 4, static_cast<std::size_t>(iWidth) * 4);
		pUpload->Unmap(0, nullptr);

		WaitForUploads();
		m_pUploadAllocator->Reset();
		m_pUploadList->Reset(m_pUploadAllocator.Get(), nullptr);
		D3D12_TEXTURE_COPY_LOCATION stSource{};
		stSource.pResource = pUpload.Get();
		stSource.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		stSource.PlacedFootprint.Footprint = { DXGI_FORMAT_R8G8B8A8_UNORM, static_cast<UINT>(iWidth), static_cast<UINT>(iHeight), 1, uPitch };
		D3D12_TEXTURE_COPY_LOCATION stDestination{};
		stDestination.pResource = pTexture.Get();
		stDestination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		m_pUploadList->CopyTextureRegion(&stDestination, 0, 0, 0, &stSource, nullptr);
		const D3D12_RESOURCE_BARRIER stBarrier = Transition(pTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_pUploadList->ResourceBarrier(1, &stBarrier);
		m_pUploadList->Close();
		ID3D12CommandList* aLists[] = { m_pUploadList.Get() };
		m_pQueue->ExecuteCommandLists(1, aLists);
		m_pQueue->Signal(m_pUploadFence.Get(), ++m_uUploadFenceValue);
		WaitForUploads();

		D3D12_SHADER_RESOURCE_VIEW_DESC stView{};
		stView.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		stView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		stView.Texture2D.MipLevels = 1;
		stView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		D3D12_CPU_DESCRIPTOR_HANDLE hCPU = m_pHeap->GetCPUDescriptorHandleForHeapStart();
		hCPU.ptr += static_cast<SIZE_T>(uSlot) * m_uDescriptorSize;
		m_pDevice->CreateShaderResourceView(pTexture.Get(), &stView, hCPU);
		D3D12_GPU_DESCRIPTOR_HANDLE hGPU = m_pHeap->GetGPUDescriptorHandleForHeapStart();
		hGPU.ptr += static_cast<UINT64>(uSlot) * m_uDescriptorSize;

		m_aSlotUsed[uSlot] = true;
		const TextureHandle hResult = hGPU.ptr;
		m_mapTextures[hResult] = { std::move(pTexture), uSlot };
		return hResult;
	}

	void clBackendDX12::DestroyTexture(TextureHandle hTexture)
	{
		const auto itTexture = m_mapTextures.find(hTexture);
		if (itTexture == m_mapTextures.end())
			return;
		if (m_aFrames.empty())
		{
			m_aSlotUsed[itTexture->second.uSlot] = false;
		}
		else
		{
			stFrameResources& rFrame = m_aFrames[m_uFrameIndex];
			rFrame.aPendingRelease.push_back(itTexture->second.pResource);
			rFrame.aPendingSlots.push_back(itTexture->second.uSlot);
		}
		m_mapTextures.erase(itTexture);
	}

	bool clBackendDX12::EnsureUploadBuffer(ComPtr<ID3D12Resource>& rBuffer, std::size_t& rCapacity, std::size_t uRequired)
	{
		if (rBuffer && rCapacity >= uRequired)
			return true;
		rBuffer.Reset();
		rCapacity = std::max<std::size_t>(uRequired + uRequired / 2, 65536);
		D3D12_HEAP_PROPERTIES stHeap{ D3D12_HEAP_TYPE_UPLOAD };
		D3D12_RESOURCE_DESC stDesc{};
		stDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		stDesc.Width = rCapacity;
		stDesc.Height = 1;
		stDesc.DepthOrArraySize = 1;
		stDesc.MipLevels = 1;
		stDesc.SampleDesc.Count = 1;
		stDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		if (FAILED(m_pDevice->CreateCommittedResource(&stHeap, D3D12_HEAP_FLAG_NONE, &stDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&rBuffer))))
		{
			rCapacity = 0;
			return false;
		}
		return true;
	}

	void clBackendDX12::RenderDrawData(const stDrawData& stData)
	{
		if (!m_pCommandList || !m_pPipeline || stData.uTotalIndices == 0 || stData.vecDisplaySize.x <= 0.f || stData.vecDisplaySize.y <= 0.f)
			return;
		stFrameResources& rFrame = m_aFrames[m_uFrameIndex];
		const std::size_t uVertexBytes = stData.uTotalVertices * sizeof(stVertex);
		const std::size_t uIndexBytes = stData.uTotalIndices * sizeof(std::uint32_t);
		if (!EnsureUploadBuffer(rFrame.pVertexBuffer, rFrame.uVertexBytes, uVertexBytes) || !EnsureUploadBuffer(rFrame.pIndexBuffer, rFrame.uIndexBytes, uIndexBytes))
			return;

		void* pVertexData = nullptr;
		void* pIndexData = nullptr;
		const D3D12_RANGE stNoRead{ 0, 0 };
		if (FAILED(rFrame.pVertexBuffer->Map(0, &stNoRead, &pVertexData)) || FAILED(rFrame.pIndexBuffer->Map(0, &stNoRead, &pIndexData)))
			return;
		auto* pVertexOut = static_cast<stVertex*>(pVertexData);
		auto* pIndexOut = static_cast<std::uint32_t*>(pIndexData);
		for (const clDrawList* pList : stData.aLists)
		{
			std::memcpy(pVertexOut, pList->GetVertices().data(), pList->GetVertices().size() * sizeof(stVertex));
			std::memcpy(pIndexOut, pList->GetIndices().data(), pList->GetIndices().size() * sizeof(std::uint32_t));
			pVertexOut += pList->GetVertices().size();
			pIndexOut += pList->GetIndices().size();
		}
		rFrame.pVertexBuffer->Unmap(0, nullptr);
		rFrame.pIndexBuffer->Unmap(0, nullptr);

		ID3D12GraphicsCommandList* pList = m_pCommandList;
		const D3D12_VIEWPORT stViewport{ 0.f, 0.f, stData.vecDisplaySize.x, stData.vecDisplaySize.y, 0.f, 1.f };
		pList->RSSetViewports(1, &stViewport);
		const D3D12_VERTEX_BUFFER_VIEW stVertexView{ rFrame.pVertexBuffer->GetGPUVirtualAddress(), static_cast<UINT>(uVertexBytes), sizeof(stVertex) };
		const D3D12_INDEX_BUFFER_VIEW stIndexView{ rFrame.pIndexBuffer->GetGPUVirtualAddress(), static_cast<UINT>(uIndexBytes), DXGI_FORMAT_R32_UINT };
		pList->IASetVertexBuffers(0, 1, &stVertexView);
		pList->IASetIndexBuffer(&stIndexView);
		pList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pList->SetPipelineState(m_pPipeline.Get());
		pList->SetGraphicsRootSignature(m_pRootSignature.Get());
		const auto aProjection = stData.OrthoProjection();
		pList->SetGraphicsRoot32BitConstants(0, 16, aProjection.data(), 0);
		const float aBlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
		pList->OMSetBlendFactor(aBlendFactor);
		ID3D12DescriptorHeap* aHeaps[] = { m_pHeap.Get() };
		pList->SetDescriptorHeaps(1, aHeaps);

		UINT uGlobalIndex = 0;
		INT iGlobalVertex = 0;
		for (const clDrawList* pDrawList : stData.aLists)
		{
			for (const stDrawCommand& rCommand : pDrawList->GetCommands())
			{
				if (rCommand.uIndexCount == 0)
					continue;
				const rect rcClip = rCommand.rcClip.Clipped({ stData.vecDisplayPosition, stData.vecDisplayPosition + stData.vecDisplaySize });
				if (rcClip.IsEmpty())
					continue;
				const D3D12_RECT stScissor{ static_cast<LONG>(rcClip.min.x - stData.vecDisplayPosition.x), static_cast<LONG>(rcClip.min.y - stData.vecDisplayPosition.y),
					static_cast<LONG>(rcClip.max.x - stData.vecDisplayPosition.x), static_cast<LONG>(rcClip.max.y - stData.vecDisplayPosition.y) };
				pList->RSSetScissorRects(1, &stScissor);
				pList->SetGraphicsRootDescriptorTable(1, D3D12_GPU_DESCRIPTOR_HANDLE{ rCommand.hTexture });
				pList->DrawIndexedInstanced(rCommand.uIndexCount, 1, uGlobalIndex + rCommand.uIndexOffset, iGlobalVertex, 0);
			}
			uGlobalIndex += static_cast<UINT>(pDrawList->GetIndices().size());
			iGlobalVertex += static_cast<INT>(pDrawList->GetVertices().size());
		}
	}

	void clBackendDX12::RenderWorld(const clWorldDrawList& rList, const stWorldPass& stPass)
	{
		if (!m_pCommandList || !m_pRootSignature || rList.IsEmpty() || m_uWorldWidth == 0 || m_uWorldHeight == 0)
			return;
		stFrameResources& rFrame = m_aFrames[m_uFrameIndex];
		std::size_t uVertices = 0, uIndices = 0;
		for (std::size_t i = 0; i < static_cast<std::size_t>(EDepthBatch::Count); ++i)
		{
			uVertices += rList.GetBatch(static_cast<EDepthBatch>(i)).aVertices.size();
			uIndices += rList.GetBatch(static_cast<EDepthBatch>(i)).aIndices.size();
		}
		const std::size_t uVertexBytes = uVertices * sizeof(stWorldVertex);
		const std::size_t uIndexBytes = uIndices * sizeof(std::uint32_t);
		if (!EnsureUploadBuffer(rFrame.pWorldVertexBuffer, rFrame.uWorldVertexBytes, uVertexBytes) || !EnsureUploadBuffer(rFrame.pWorldIndexBuffer, rFrame.uWorldIndexBytes, uIndexBytes))
			return;

		void* pVertexData = nullptr;
		void* pIndexData = nullptr;
		const D3D12_RANGE stNoRead{ 0, 0 };
		if (FAILED(rFrame.pWorldVertexBuffer->Map(0, &stNoRead, &pVertexData)) || FAILED(rFrame.pWorldIndexBuffer->Map(0, &stNoRead, &pIndexData)))
			return;
		auto* pVertexOut = static_cast<stWorldVertex*>(pVertexData);
		auto* pIndexOut = static_cast<std::uint32_t*>(pIndexData);
		for (std::size_t i = 0; i < static_cast<std::size_t>(EDepthBatch::Count); ++i)
		{
			const stWorldBatch& rBatch = rList.GetBatch(static_cast<EDepthBatch>(i));
			std::memcpy(pVertexOut, rBatch.aVertices.data(), rBatch.aVertices.size() * sizeof(stWorldVertex));
			std::memcpy(pIndexOut, rBatch.aIndices.data(), rBatch.aIndices.size() * sizeof(std::uint32_t));
			pVertexOut += rBatch.aVertices.size();
			pIndexOut += rBatch.aIndices.size();
		}
		rFrame.pWorldVertexBuffer->Unmap(0, nullptr);
		rFrame.pWorldIndexBuffer->Unmap(0, nullptr);

		ID3D12GraphicsCommandList* pList = m_pCommandList;
		const bool bHasDepth = m_eWorldDepthFormat != DXGI_FORMAT_UNKNOWN;
		if (m_hWorldRenderTarget.ptr)
			pList->OMSetRenderTargets(1, &m_hWorldRenderTarget, FALSE, bHasDepth ? &m_hWorldDepth : nullptr);
		const D3D12_VIEWPORT stViewport{ 0.f, 0.f, static_cast<float>(m_uWorldWidth), static_cast<float>(m_uWorldHeight), 0.f, 1.f };
		const D3D12_RECT stScissor{ 0, 0, static_cast<LONG>(m_uWorldWidth), static_cast<LONG>(m_uWorldHeight) };
		pList->RSSetViewports(1, &stViewport);
		pList->RSSetScissorRects(1, &stScissor);
		const D3D12_VERTEX_BUFFER_VIEW stVertexView{ rFrame.pWorldVertexBuffer->GetGPUVirtualAddress(), static_cast<UINT>(uVertexBytes), sizeof(stWorldVertex) };
		const D3D12_INDEX_BUFFER_VIEW stIndexView{ rFrame.pWorldIndexBuffer->GetGPUVirtualAddress(), static_cast<UINT>(uIndexBytes), DXGI_FORMAT_R32_UINT };
		pList->IASetVertexBuffers(0, 1, &stVertexView);
		pList->IASetIndexBuffer(&stIndexView);
		pList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pList->SetGraphicsRootSignature(m_pRootSignature.Get());
		ID3D12DescriptorHeap* aHeaps[] = { m_pHeap.Get() };
		pList->SetDescriptorHeaps(1, aHeaps);
		pList->SetGraphicsRootDescriptorTable(1, D3D12_GPU_DESCRIPTOR_HANDLE{ rList.GetTexture() });

		UINT uIndexBase = 0;
		INT iVertexBase = 0;
		for (std::size_t i = 0; i < static_cast<std::size_t>(EDepthBatch::Count); ++i)
		{
			const auto eBatch = static_cast<EDepthBatch>(i);
			const stWorldBatch& rBatch = rList.GetBatch(eBatch);
			if (!rBatch.aIndices.empty() && (bHasDepth || eBatch == EDepthBatch::Always || eBatch == EDepthBatch::Tested))
				if (ID3D12PipelineState* pPipeline = WorldPipeline(eBatch, stPass.bReversedZ))
				{
					pList->SetPipelineState(pPipeline);
					pList->DrawIndexedInstanced(static_cast<UINT>(rBatch.aIndices.size()), 1, uIndexBase, iVertexBase, 0);
				}
			uIndexBase += static_cast<UINT>(rBatch.aIndices.size());
			iVertexBase += static_cast<INT>(rBatch.aVertices.size());
		}
	}
}
