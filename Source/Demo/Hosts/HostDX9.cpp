#if OMEGAGUI_BACKEND_DX9

#include "Demo/Host.hpp"
#include "OmegaGUI/Backends/DX9/BackendDX9.hpp"

namespace
{
	class clHostDX9 final : public clHost
	{
	public:
		~clHostDX9() override
		{
			m_clBackend.Shutdown();
			if (m_pDevice)
				m_pDevice->Release();
			if (m_pDirect3D)
				m_pDirect3D->Release();
		}

		bool Create(HWND hWindow, int iWidth, int iHeight) override
		{
			m_pDirect3D = Direct3DCreate9(D3D_SDK_VERSION);
			if (!m_pDirect3D)
				return false;
			m_stParameters.Windowed = TRUE;
			m_stParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
			m_stParameters.BackBufferFormat = D3DFMT_A8R8G8B8;
			m_stParameters.BackBufferWidth = static_cast<UINT>(iWidth);
			m_stParameters.BackBufferHeight = static_cast<UINT>(iHeight);
			m_stParameters.EnableAutoDepthStencil = TRUE;
			m_stParameters.AutoDepthStencilFormat = D3DFMT_D24S8;
			m_stParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
			m_stParameters.hDeviceWindow = hWindow;
			if (FAILED(m_pDirect3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWindow, D3DCREATE_HARDWARE_VERTEXPROCESSING, &m_stParameters, &m_pDevice)))
				return false;
			return m_clBackend.Initialize(m_pDevice);
		}

		void Resize(int iWidth, int iHeight) override
		{
			m_stParameters.BackBufferWidth = static_cast<UINT>(std::max(iWidth, 1));
			m_stParameters.BackBufferHeight = static_cast<UINT>(std::max(iHeight, 1));
			m_clBackend.InvalidateDeviceObjects();
			m_pDevice->Reset(&m_stParameters);
			m_clBackend.CreateDeviceObjects();
		}

		void BeginFrame(const float aClearColor[4]) override
		{
			const D3DCOLOR uClear = D3DCOLOR_COLORVALUE(aClearColor[0], aClearColor[1], aClearColor[2], aClearColor[3]);
			m_pDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, uClear, 1.f, 0);
			m_pDevice->BeginScene();
		}

		void EndFrame() override
		{
			m_pDevice->EndScene();
			if (m_pDevice->Present(nullptr, nullptr, nullptr, nullptr) == D3DERR_DEVICELOST && m_pDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
				Resize(static_cast<int>(m_stParameters.BackBufferWidth), static_cast<int>(m_stParameters.BackBufferHeight));
		}

		OmegaGUI::clRenderBackend* Backend() override { return &m_clBackend; }

	private:
		IDirect3D9* m_pDirect3D = nullptr;
		IDirect3DDevice9* m_pDevice = nullptr;
		D3DPRESENT_PARAMETERS m_stParameters{};
		OmegaGUI::clBackendDX9 m_clBackend;
	};
}

std::unique_ptr<clHost> CreateHostDX9()
{
	return std::make_unique<clHostDX9>();
}

#endif
