#pragma once

#include "OmegaGUI/Render/Backend.hpp"

#include <d3d11.h>
#include <wrl/client.h>

#include <array>

namespace OmegaGUI
{
	class clBackendDX11 final : public clRenderBackend
	{
	public:
		~clBackendDX11() override;

		bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
		void Shutdown();
		void SetWorldTarget(ID3D11RenderTargetView* pRenderTarget, ID3D11DepthStencilView* pDepthStencil);

		[[nodiscard]] std::string_view GetName() const override { return "Direct3D 11"; }
		[[nodiscard]] TextureHandle CreateTexture(const std::uint8_t* pPixelsRGBA, int iWidth, int iHeight) override;
		void DestroyTexture(TextureHandle hTexture) override;
		void RenderDrawData(const stDrawData& stData) override;
		void RenderWorld(const clWorldDrawList& rList, const stWorldPass& stPass) override;
		void InvalidateDeviceObjects() override;
		bool CreateDeviceObjects() override;

	private:
		template <typename T>
		using ComPtr = Microsoft::WRL::ComPtr<T>;

		struct stStateBackup;

		bool EnsureBuffer(ComPtr<ID3D11Buffer>& rBuffer, std::size_t& rCapacity, std::size_t uRequired, std::size_t uStride, UINT uBindFlags);
		void BackupState(stStateBackup& rBackup);
		void RestoreState(stStateBackup& rBackup);

		ComPtr<ID3D11Device> m_pDevice;
		ComPtr<ID3D11DeviceContext> m_pContext;
		ComPtr<ID3D11VertexShader> m_pVertexShader;
		ComPtr<ID3D11VertexShader> m_pWorldVertexShader;
		ComPtr<ID3D11PixelShader> m_pPixelShader;
		ComPtr<ID3D11InputLayout> m_pInputLayout;
		ComPtr<ID3D11InputLayout> m_pWorldInputLayout;
		ComPtr<ID3D11Buffer> m_pConstantBuffer;
		ComPtr<ID3D11SamplerState> m_pSampler;
		ComPtr<ID3D11BlendState> m_pBlendState;
		ComPtr<ID3D11RasterizerState> m_pRasterizerState;
		ComPtr<ID3D11RasterizerState> m_pWorldRasterizerState;
		ComPtr<ID3D11DepthStencilState> m_pDepthDisabled;
		std::array<ComPtr<ID3D11DepthStencilState>, 8> m_aWorldDepthStates;
		ComPtr<ID3D11Buffer> m_pVertexBuffer;
		ComPtr<ID3D11Buffer> m_pIndexBuffer;
		ComPtr<ID3D11Buffer> m_pWorldVertexBuffer;
		ComPtr<ID3D11Buffer> m_pWorldIndexBuffer;
		std::size_t m_uVertexCapacity = 0;
		std::size_t m_uIndexCapacity = 0;
		std::size_t m_uWorldVertexCapacity = 0;
		std::size_t m_uWorldIndexCapacity = 0;
		ID3D11RenderTargetView* m_pWorldRenderTarget = nullptr;
		ID3D11DepthStencilView* m_pWorldDepthStencil = nullptr;
	};
}
