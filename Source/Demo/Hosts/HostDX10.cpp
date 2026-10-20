#if OMEGAGUI_BACKEND_DX10

#include "Demo/Host.hpp"
#include "OmegaGUI/Backends/DX10/BackendDX10.hpp"

namespace
{
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	class clHostDX10 final : public clHost
	{
	public:
		~clHostDX10() override
		{
			m_clBackend.Shutdown();
		}

		bool Create(HWND hWindow, int iWidth, int iHeight) override
		{
			DXGI_SWAP_CHAIN_DESC stDesc{};
			stDesc.BufferCount = 2;
			stDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			stDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stDesc.OutputWindow = hWindow;
			stDesc.SampleDesc.Count = 1;
			stDesc.Windowed = TRUE;
			stDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
			if ((g_bForceWarp || FAILED(D3D10CreateDeviceAndSwapChain(nullptr, D3D10_DRIVER_TYPE_HARDWARE, nullptr, 0, D3D10_SDK_VERSION, &stDesc, &m_pSwapChain, &m_pDevice))) &&
				FAILED(D3D10CreateDeviceAndSwapChain(nullptr, D3D10_DRIVER_TYPE_WARP, nullptr, 0, D3D10_SDK_VERSION, &stDesc, &m_pSwapChain, &m_pDevice)))
				return false;
			CreateTargets(iWidth, iHeight);
			return m_clBackend.Initialize(m_pDevice.Get());
		}

		void Resize(int iWidth, int iHeight) override
		{
			m_pRenderTarget.Reset();
			m_pDepthView.Reset();
			m_pSwapChain->ResizeBuffers(0, static_cast<UINT>(iWidth), static_cast<UINT>(iHeight), DXGI_FORMAT_UNKNOWN, 0);
			CreateTargets(iWidth, iHeight);
		}

		void BeginFrame(const float aClearColor[4]) override
		{
			m_pDevice->OMSetRenderTargets(1, m_pRenderTarget.GetAddressOf(), m_pDepthView.Get());
			m_pDevice->ClearRenderTargetView(m_pRenderTarget.Get(), aClearColor);
			m_pDevice->ClearDepthStencilView(m_pDepthView.Get(), D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.f, 0);
			const D3D10_VIEWPORT stViewport{ 0, 0, static_cast<UINT>(m_iWidth), static_cast<UINT>(m_iHeight), 0.f, 1.f };
			m_pDevice->RSSetViewports(1, &stViewport);
			m_clBackend.SetWorldTarget(m_pRenderTarget.Get(), m_pDepthView.Get());
		}

		void EndFrame() override
		{
			m_pSwapChain->Present(1, 0);
		}

		OmegaGUI::clRenderBackend* Backend() override { return &m_clBackend; }

	private:
		void CreateTargets(int iWidth, int iHeight)
		{
			m_iWidth = std::max(iWidth, 1);
			m_iHeight = std::max(iHeight, 1);
			ComPtr<ID3D10Texture2D> pBackBuffer;
			m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
			m_pDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &m_pRenderTarget);
			D3D10_TEXTURE2D_DESC stDepth{};
			stDepth.Width = static_cast<UINT>(m_iWidth);
			stDepth.Height = static_cast<UINT>(m_iHeight);
			stDepth.MipLevels = 1;
			stDepth.ArraySize = 1;
			stDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			stDepth.SampleDesc.Count = 1;
			stDepth.BindFlags = D3D10_BIND_DEPTH_STENCIL;
			ComPtr<ID3D10Texture2D> pDepth;
			m_pDevice->CreateTexture2D(&stDepth, nullptr, &pDepth);
			m_pDevice->CreateDepthStencilView(pDepth.Get(), nullptr, &m_pDepthView);
		}

		ComPtr<ID3D10Device> m_pDevice;
		ComPtr<IDXGISwapChain> m_pSwapChain;
		ComPtr<ID3D10RenderTargetView> m_pRenderTarget;
		ComPtr<ID3D10DepthStencilView> m_pDepthView;
		OmegaGUI::clBackendDX10 m_clBackend;
		int m_iWidth = 1;
		int m_iHeight = 1;
	};
}

std::unique_ptr<clHost> CreateHostDX10()
{
	return std::make_unique<clHostDX10>();
}

#endif
