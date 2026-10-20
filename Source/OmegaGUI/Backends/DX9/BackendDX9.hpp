#pragma once

#include "OmegaGUI/Render/Backend.hpp"

#include <d3d9.h>

#include <memory>
#include <vector>

namespace OmegaGUI
{
	class clBackendDX9 final : public clRenderBackend
	{
	public:
		~clBackendDX9() override;

		bool Initialize(IDirect3DDevice9* pDevice);
		void Shutdown();
		void SetWorldDepthSurface(IDirect3DSurface9* pDepthSurface);

		[[nodiscard]] std::string_view GetName() const override { return "Direct3D 9"; }
		[[nodiscard]] TextureHandle CreateTexture(const std::uint8_t* pPixelsRGBA, int iWidth, int iHeight) override;
		void DestroyTexture(TextureHandle hTexture) override;
		void RenderDrawData(const stDrawData& stData) override;
		void RenderWorld(const clWorldDrawList& rList, const stWorldPass& stPass) override;
		void InvalidateDeviceObjects() override;
		bool CreateDeviceObjects() override;

	private:
		struct stTexture
		{
			IDirect3DTexture9* pTexture = nullptr;
			std::vector<std::uint8_t> aPixels;
			int iWidth = 0;
			int iHeight = 0;
		};

		bool UploadTexture(stTexture& rTexture);
		bool EnsureBuffers(IDirect3DVertexBuffer9*& rVertexBuffer, std::size_t& rVertexCapacity, std::size_t uVertices, std::size_t uStride, DWORD uFVF,
			IDirect3DIndexBuffer9*& rIndexBuffer, std::size_t& rIndexCapacity, std::size_t uIndices);
		void SetupRenderState(const stDrawData& stData);

		IDirect3DDevice9* m_pDevice = nullptr;
		IDirect3DVertexBuffer9* m_pVertexBuffer = nullptr;
		IDirect3DIndexBuffer9* m_pIndexBuffer = nullptr;
		IDirect3DVertexBuffer9* m_pWorldVertexBuffer = nullptr;
		IDirect3DIndexBuffer9* m_pWorldIndexBuffer = nullptr;
		IDirect3DVertexDeclaration9* m_pWorldDeclaration = nullptr;
		IDirect3DVertexShader9* m_pWorldVertexShader = nullptr;
		IDirect3DPixelShader9* m_pWorldPixelShader = nullptr;
		IDirect3DSurface9* m_pWorldDepth = nullptr;
		std::size_t m_uVertexCapacity = 0;
		std::size_t m_uIndexCapacity = 0;
		std::size_t m_uWorldVertexCapacity = 0;
		std::size_t m_uWorldIndexCapacity = 0;
		std::vector<std::unique_ptr<stTexture>> m_aTextures;
	};
}
