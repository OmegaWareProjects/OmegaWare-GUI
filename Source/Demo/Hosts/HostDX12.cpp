#if OMEGAGUI_BACKEND_DX12

#include "Demo/Host.hpp"
#include "OmegaGUI/Backends/DX12/BackendDX12.hpp"

#include <array>

namespace
{
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	constexpr UINT uFrameCount = 2;

	class clHostDX12 final : public clHost
	{
	public:
		~clHostDX12() override
		{
			WaitForGpu();
			m_clBackend.Shutdown();
			if (m_hFenceEvent)
				CloseHandle(m_hFenceEvent);
		}

		bool Create(HWND hWindow, int iWidth, int iHeight) override
		{
			ComPtr<IDXGIAdapter> pAdapter;
			if (g_bForceWarp)
			{
				ComPtr<IDXGIFactory4> pWarpFactory;
				if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&pWarpFactory))) || FAILED(pWarpFactory->EnumWarpAdapter(IID_PPV_ARGS(&pAdapter))))
					return false;
			}
			if (FAILED(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_pDevice))))
				return false;
			D3D12_COMMAND_QUEUE_DESC stQueue{ D3D12_COMMAND_LIST_TYPE_DIRECT };
			if (FAILED(m_pDevice->CreateCommandQueue(&stQueue, IID_PPV_ARGS(&m_pQueue))))
				return false;

			ComPtr<IDXGIFactory4> pFactory;
			if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&pFactory))))
				return false;
			DXGI_SWAP_CHAIN_DESC1 stSwap{};
			stSwap.Width = static_cast<UINT>(iWidth);
			stSwap.Height = static_cast<UINT>(iHeight);
			stSwap.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			stSwap.SampleDesc.Count = 1;
			stSwap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwap.BufferCount = uFrameCount;
			stSwap.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			ComPtr<IDXGISwapChain1> pSwapChain;
			if (FAILED(pFactory->CreateSwapChainForHwnd(m_pQueue.Get(), hWindow, &stSwap, nullptr, nullptr, &pSwapChain)) || FAILED(pSwapChain.As(&m_pSwapChain)))
				return false;

			D3D12_DESCRIPTOR_HEAP_DESC stRTV{ D3D12_DESCRIPTOR_HEAP_TYPE_RTV, uFrameCount };
			D3D12_DESCRIPTOR_HEAP_DESC stDSV{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1 };
			m_pDevice->CreateDescriptorHeap(&stRTV, IID_PPV_ARGS(&m_pRTVHeap));
			m_pDevice->CreateDescriptorHeap(&stDSV, IID_PPV_ARGS(&m_pDSVHeap));
			m_uRTVSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			for (UINT i = 0; i < uFrameCount; ++i)
				m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_aAllocators[i]));
			m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_aAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_pList));
			m_pList->Close();
			m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence));
			m_hFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

			CreateTargets(iWidth, iHeight);
			OmegaGUI::stDX12Config stConfig;
			stConfig.pDevice = m_pDevice.Get();
			stConfig.pQueue = m_pQueue.Get();
			stConfig.iFramesInFlight = uFrameCount;
			stConfig.eRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
			return m_clBackend.Initialize(stConfig);
		}

		void Resize(int iWidth, int iHeight) override
		{
			WaitForGpu();
			for (auto& pTarget : m_aTargets)
				pTarget.Reset();
			m_pDepth.Reset();
			m_pSwapChain->ResizeBuffers(0, static_cast<UINT>(std::max(iWidth, 1)), static_cast<UINT>(std::max(iHeight, 1)), DXGI_FORMAT_UNKNOWN, 0);
			CreateTargets(iWidth, iHeight);
		}

		void BeginFrame(const float aClearColor[4]) override
		{
			m_uBackBuffer = m_pSwapChain->GetCurrentBackBufferIndex();
			if (m_pFence->GetCompletedValue() < m_aFrameFence[m_uBackBuffer])
			{
				m_pFence->SetEventOnCompletion(m_aFrameFence[m_uBackBuffer], m_hFenceEvent);
				WaitForSingleObject(m_hFenceEvent, INFINITE);
			}
			m_aAllocators[m_uBackBuffer]->Reset();
			m_pList->Reset(m_aAllocators[m_uBackBuffer].Get(), nullptr);
			D3D12_RESOURCE_BARRIER stBarrier{};
			stBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			stBarrier.Transition.pResource = m_aTargets[m_uBackBuffer].Get();
			stBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			stBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			stBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			m_pList->ResourceBarrier(1, &stBarrier);

			const D3D12_CPU_DESCRIPTOR_HANDLE hRTV = RenderTargetHandle(m_uBackBuffer);
			const D3D12_CPU_DESCRIPTOR_HANDLE hDSV = m_pDSVHeap->GetCPUDescriptorHandleForHeapStart();
			m_pList->OMSetRenderTargets(1, &hRTV, FALSE, &hDSV);
			m_pList->ClearRenderTargetView(hRTV, aClearColor, 0, nullptr);
			m_pList->ClearDepthStencilView(hDSV, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
			m_clBackend.BeginFrame(m_pList.Get());
			m_clBackend.SetWorldTarget(hRTV, hDSV, DXGI_FORMAT_D32_FLOAT, static_cast<UINT>(m_iWidth), static_cast<UINT>(m_iHeight));
		}

		void EndFrame() override
		{
			D3D12_RESOURCE_BARRIER stBarrier{};
			stBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			stBarrier.Transition.pResource = m_aTargets[m_uBackBuffer].Get();
			stBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			stBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			stBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			m_pList->ResourceBarrier(1, &stBarrier);
			m_pList->Close();
			ID3D12CommandList* aLists[] = { m_pList.Get() };
			m_pQueue->ExecuteCommandLists(1, aLists);
			m_pSwapChain->Present(1, 0);
			m_pQueue->Signal(m_pFence.Get(), ++m_uFenceValue);
			m_aFrameFence[m_uBackBuffer] = m_uFenceValue;
		}

		OmegaGUI::clRenderBackend* Backend() override { return &m_clBackend; }

	private:
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetHandle(UINT uIndex) const
		{
			D3D12_CPU_DESCRIPTOR_HANDLE hHandle = m_pRTVHeap->GetCPUDescriptorHandleForHeapStart();
			hHandle.ptr += static_cast<SIZE_T>(uIndex) * m_uRTVSize;
			return hHandle;
		}

		void WaitForGpu()
		{
			if (!m_pQueue || !m_pFence)
				return;
			m_pQueue->Signal(m_pFence.Get(), ++m_uFenceValue);
			m_pFence->SetEventOnCompletion(m_uFenceValue, m_hFenceEvent);
			WaitForSingleObject(m_hFenceEvent, INFINITE);
		}

		void CreateTargets(int iWidth, int iHeight)
		{
			m_iWidth = std::max(iWidth, 1);
			m_iHeight = std::max(iHeight, 1);
			for (UINT i = 0; i < uFrameCount; ++i)
			{
				m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_aTargets[i]));
				m_pDevice->CreateRenderTargetView(m_aTargets[i].Get(), nullptr, RenderTargetHandle(i));
			}
			D3D12_HEAP_PROPERTIES stHeap{ D3D12_HEAP_TYPE_DEFAULT };
			D3D12_RESOURCE_DESC stDesc{};
			stDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			stDesc.Width = static_cast<UINT64>(m_iWidth);
			stDesc.Height = static_cast<UINT>(m_iHeight);
			stDesc.DepthOrArraySize = 1;
			stDesc.MipLevels = 1;
			stDesc.Format = DXGI_FORMAT_D32_FLOAT;
			stDesc.SampleDesc.Count = 1;
			stDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			D3D12_CLEAR_VALUE stClear{};
			stClear.Format = DXGI_FORMAT_D32_FLOAT;
			stClear.DepthStencil.Depth = 1.f;
			m_pDevice->CreateCommittedResource(&stHeap, D3D12_HEAP_FLAG_NONE, &stDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &stClear, IID_PPV_ARGS(&m_pDepth));
			m_pDevice->CreateDepthStencilView(m_pDepth.Get(), nullptr, m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());
		}

		ComPtr<ID3D12Device> m_pDevice;
		ComPtr<ID3D12CommandQueue> m_pQueue;
		ComPtr<IDXGISwapChain3> m_pSwapChain;
		ComPtr<ID3D12DescriptorHeap> m_pRTVHeap;
		ComPtr<ID3D12DescriptorHeap> m_pDSVHeap;
		std::array<ComPtr<ID3D12Resource>, uFrameCount> m_aTargets;
		ComPtr<ID3D12Resource> m_pDepth;
		std::array<ComPtr<ID3D12CommandAllocator>, uFrameCount> m_aAllocators;
		ComPtr<ID3D12GraphicsCommandList> m_pList;
		ComPtr<ID3D12Fence> m_pFence;
		HANDLE m_hFenceEvent = nullptr;
		UINT64 m_uFenceValue = 0;
		std::array<UINT64, uFrameCount> m_aFrameFence{};
		UINT m_uRTVSize = 0;
		UINT m_uBackBuffer = 0;
		int m_iWidth = 1;
		int m_iHeight = 1;
		OmegaGUI::clBackendDX12 m_clBackend;
	};
}

std::unique_ptr<clHost> CreateHostDX12()
{
	return std::make_unique<clHostDX12>();
}

#endif
