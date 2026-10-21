#pragma once

#include "OmegaGUI/Render/Backend.hpp"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <array>
#include <unordered_map>
#include <vector>

namespace OmegaGUI
{
	struct stDX12Config
	{
		ID3D12Device* pDevice = nullptr;
		ID3D12CommandQueue* pQueue = nullptr;
		int iFramesInFlight = 3;
		DXGI_FORMAT eRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		UINT uMaxTextures = 64;
	};

	class clBackendDX12 final : public clRenderBackend
	{
	public:
		~clBackendDX12() override;

		bool Initialize(const stDX12Config& stConfig);
		void Shutdown();
		void BeginFrame(ID3D12GraphicsCommandList* pCommandList);
		void SetWorldTarget(D3D12_CPU_DESCRIPTOR_HANDLE hRenderTarget, D3D12_CPU_DESCRIPTOR_HANDLE hDepthStencil, DXGI_FORMAT eDepthFormat, UINT uWidth, UINT uHeight);

		[[nodiscard]] std::string_view GetName() const override { return "Direct3D 12"; }
		[[nodiscard]] TextureHandle CreateTexture(const std::uint8_t* pPixelsRGBA, int iWidth, int iHeight) override;
		void DestroyTexture(TextureHandle hTexture) override;
		void RenderDrawData(const stDrawData& stData) override;
		void RenderWorld(const clWorldDrawList& rList, const stWorldPass& stPass) override;
		void InvalidateDeviceObjects() override;
		bool CreateDeviceObjects() override;

	private:
		template <typename T>
		using ComPtr = Microsoft::WRL::ComPtr<T>;

		struct stFrameResources
		{
			ComPtr<ID3D12Resource> pVertexBuffer;
			ComPtr<ID3D12Resource> pIndexBuffer;
			std::size_t uVertexBytes = 0;
			std::size_t uIndexBytes = 0;
			ComPtr<ID3D12Resource> pWorldVertexBuffer;
			ComPtr<ID3D12Resource> pWorldIndexBuffer;
			std::size_t uWorldVertexBytes = 0;
			std::size_t uWorldIndexBytes = 0;
			std::vector<ComPtr<ID3D12Resource>> aPendingRelease;
			std::vector<UINT> aPendingSlots;
		};

		struct stTexture
		{
			ComPtr<ID3D12Resource> pResource;
			UINT uSlot = 0;
		};

		bool EnsureUploadBuffer(ComPtr<ID3D12Resource>& rBuffer, std::size_t& rCapacity, std::size_t uRequired);
		ID3D12PipelineState* WorldPipeline(EDepthBatch eBatch, bool bReversed, bool bDepth);
		void WaitForUploads();

		ComPtr<ID3D12Device> m_pDevice;
		ComPtr<ID3D12CommandQueue> m_pQueue;
		ComPtr<ID3D12RootSignature> m_pRootSignature;
		ComPtr<ID3D12PipelineState> m_pPipeline;
		std::array<ComPtr<ID3D12PipelineState>, 16> m_aWorldPipelines;
		DXGI_FORMAT m_eWorldDepthFormat = DXGI_FORMAT_UNKNOWN;
		ComPtr<ID3D12DescriptorHeap> m_pHeap;
		UINT m_uDescriptorSize = 0;
		std::vector<bool> m_aSlotUsed;
		std::unordered_map<TextureHandle, stTexture> m_mapTextures;
		ComPtr<ID3D12CommandAllocator> m_pUploadAllocator;
		ComPtr<ID3D12GraphicsCommandList> m_pUploadList;
		ComPtr<ID3D12Fence> m_pUploadFence;
		UINT64 m_uUploadFenceValue = 0;
		HANDLE m_hUploadEvent = nullptr;
		ComPtr<ID3DBlob> m_pVertexShader;
		ComPtr<ID3DBlob> m_pWorldVertexShader;
		ComPtr<ID3DBlob> m_pPixelShader;
		std::vector<stFrameResources> m_aFrames;
		std::size_t m_uFrameIndex = 0;
		ID3D12GraphicsCommandList* m_pCommandList = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE m_hWorldRenderTarget{};
		D3D12_CPU_DESCRIPTOR_HANDLE m_hWorldDepth{};
		UINT m_uWorldWidth = 0;
		UINT m_uWorldHeight = 0;
		stDX12Config m_stConfig;
	};
}
