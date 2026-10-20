#include "OmegaGUI/Backends/DX9/BackendDX9.hpp"
#include "OmegaGUI/Backends/Shaders.hpp"

#include <d3dcompiler.h>

#include <cstring>

namespace OmegaGUI
{
	namespace
	{
		struct stVertexDX9
		{
			float flX, flY, flZ;
			D3DCOLOR uColor;
			float flU, flV;
		};

		constexpr DWORD uFVF = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;

		template <typename T>
		void SafeRelease(T*& pObject)
		{
			if (pObject)
				pObject->Release();
			pObject = nullptr;
		}

		[[nodiscard]] ID3DBlob* CompileShader(const char* szSource, std::size_t uLength, const char* szTarget)
		{
			ID3DBlob* pBlob = nullptr;
			ID3DBlob* pErrors = nullptr;
			const HRESULT hResult = D3DCompile(szSource, uLength, nullptr, nullptr, nullptr, "main", szTarget, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pBlob, &pErrors);
			SafeRelease(pErrors);
			return SUCCEEDED(hResult) ? pBlob : nullptr;
		}

		[[nodiscard]] D3DCMPFUNC DepthFunction(EDepthBatch eBatch, bool bReversed)
		{
			switch (eBatch)
			{
			case EDepthBatch::Behind: return bReversed ? D3DCMP_LESS : D3DCMP_GREATER;
			case EDepthBatch::Always: return D3DCMP_ALWAYS;
			default: return bReversed ? D3DCMP_GREATEREQUAL : D3DCMP_LESSEQUAL;
			}
		}

		[[nodiscard]] std::uint32_t SwizzleToARGB(std::uint32_t uRGBA)
		{
			return (uRGBA & 0xFF00FF00u) | ((uRGBA & 0xFF0000u) >> 16) | ((uRGBA & 0xFFu) << 16);
		}
	}

	clBackendDX9::~clBackendDX9()
	{
		Shutdown();
	}

	bool clBackendDX9::Initialize(IDirect3DDevice9* pDevice)
	{
		m_pDevice = pDevice;
		m_pDevice->AddRef();
		return CreateDeviceObjects();
	}

	void clBackendDX9::Shutdown()
	{
		InvalidateDeviceObjects();
		m_pWorldDepth = nullptr;
		SafeRelease(m_pWorldDeclaration);
		SafeRelease(m_pWorldVertexShader);
		SafeRelease(m_pWorldPixelShader);
		m_aTextures.clear();
		SafeRelease(m_pDevice);
	}

	void clBackendDX9::SetWorldDepthSurface(IDirect3DSurface9* pDepthSurface)
	{
		m_pWorldDepth = pDepthSurface;
	}

	bool clBackendDX9::UploadTexture(stTexture& rTexture)
	{
		IDirect3DTexture9* pStaging = nullptr;
		if (FAILED(m_pDevice->CreateTexture(rTexture.iWidth, rTexture.iHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pStaging, nullptr)))
			return false;
		D3DLOCKED_RECT stLocked{};
		if (FAILED(pStaging->LockRect(0, &stLocked, nullptr, 0)))
		{
			pStaging->Release();
			return false;
		}
		for (int y = 0; y < rTexture.iHeight; ++y)
		{
			const auto* pSource = reinterpret_cast<const std::uint32_t*>(rTexture.aPixels.data() + static_cast<std::size_t>(y) * rTexture.iWidth * 4);
			auto* pDestination = reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(stLocked.pBits) + static_cast<std::size_t>(y) * stLocked.Pitch);
			for (int x = 0; x < rTexture.iWidth; ++x)
				pDestination[x] = SwizzleToARGB(pSource[x]);
		}
		pStaging->UnlockRect(0);
		const bool bResult = SUCCEEDED(m_pDevice->CreateTexture(rTexture.iWidth, rTexture.iHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &rTexture.pTexture, nullptr)) &&
			SUCCEEDED(m_pDevice->UpdateTexture(pStaging, rTexture.pTexture));
		pStaging->Release();
		return bResult;
	}

	bool clBackendDX9::CreateDeviceObjects()
	{
		if (!m_pDevice)
			return false;
		for (const auto& pTexture : m_aTextures)
			if (!pTexture->pTexture)
				UploadTexture(*pTexture);

		if (!m_pWorldDeclaration)
		{
			const D3DVERTEXELEMENT9 aElements[] = {
				{ 0, static_cast<WORD>(offsetof(stWorldVertex, vecClip)), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
				{ 0, static_cast<WORD>(offsetof(stWorldVertex, vecUV)), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
				{ 0, static_cast<WORD>(offsetof(stWorldVertex, uColor)), D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
				D3DDECL_END(),
			};
			m_pDevice->CreateVertexDeclaration(aElements, &m_pWorldDeclaration);
		}
		if (!m_pWorldVertexShader)
			if (ID3DBlob* pBlob = CompileShader(Shaders::szWorldVertexHLSL9, sizeof(Shaders::szWorldVertexHLSL9) - 1, "vs_3_0"))
			{
				m_pDevice->CreateVertexShader(static_cast<const DWORD*>(pBlob->GetBufferPointer()), &m_pWorldVertexShader);
				pBlob->Release();
			}
		if (!m_pWorldPixelShader)
			if (ID3DBlob* pBlob = CompileShader(Shaders::szPixelHLSL9, sizeof(Shaders::szPixelHLSL9) - 1, "ps_3_0"))
			{
				m_pDevice->CreatePixelShader(static_cast<const DWORD*>(pBlob->GetBufferPointer()), &m_pWorldPixelShader);
				pBlob->Release();
			}
		return true;
	}

	void clBackendDX9::InvalidateDeviceObjects()
	{
		SafeRelease(m_pVertexBuffer);
		SafeRelease(m_pIndexBuffer);
		SafeRelease(m_pWorldVertexBuffer);
		SafeRelease(m_pWorldIndexBuffer);
		m_uVertexCapacity = m_uIndexCapacity = m_uWorldVertexCapacity = m_uWorldIndexCapacity = 0;
		for (const auto& pTexture : m_aTextures)
			SafeRelease(pTexture->pTexture);
	}

	TextureHandle clBackendDX9::CreateTexture(const std::uint8_t* pPixelsRGBA, int iWidth, int iHeight)
	{
		if (!m_pDevice)
			return 0;
		auto pTexture = std::make_unique<stTexture>();
		pTexture->aPixels.assign(pPixelsRGBA, pPixelsRGBA + static_cast<std::size_t>(iWidth) * iHeight * 4);
		pTexture->iWidth = iWidth;
		pTexture->iHeight = iHeight;
		if (!UploadTexture(*pTexture))
			return 0;
		const TextureHandle hResult = PointerToHandle(pTexture.get());
		m_aTextures.push_back(std::move(pTexture));
		return hResult;
	}

	void clBackendDX9::DestroyTexture(TextureHandle hTexture)
	{
		std::erase_if(m_aTextures, [&](const std::unique_ptr<stTexture>& pTexture) {
			if (PointerToHandle(pTexture.get()) != hTexture)
				return false;
			SafeRelease(pTexture->pTexture);
			return true;
		});
	}

	bool clBackendDX9::EnsureBuffers(IDirect3DVertexBuffer9*& rVertexBuffer, std::size_t& rVertexCapacity, std::size_t uVertices, std::size_t uStride, DWORD uVertexFVF,
		IDirect3DIndexBuffer9*& rIndexBuffer, std::size_t& rIndexCapacity, std::size_t uIndices)
	{
		if (!rVertexBuffer || rVertexCapacity < uVertices)
		{
			SafeRelease(rVertexBuffer);
			rVertexCapacity = std::max<std::size_t>(uVertices + uVertices / 2, 4096);
			if (FAILED(m_pDevice->CreateVertexBuffer(static_cast<UINT>(rVertexCapacity * uStride), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, uVertexFVF, D3DPOOL_DEFAULT, &rVertexBuffer, nullptr)))
			{
				rVertexCapacity = 0;
				return false;
			}
		}
		if (!rIndexBuffer || rIndexCapacity < uIndices)
		{
			SafeRelease(rIndexBuffer);
			rIndexCapacity = std::max<std::size_t>(uIndices + uIndices / 2, 8192);
			if (FAILED(m_pDevice->CreateIndexBuffer(static_cast<UINT>(rIndexCapacity * sizeof(std::uint32_t)), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &rIndexBuffer, nullptr)))
			{
				rIndexCapacity = 0;
				return false;
			}
		}
		return true;
	}

	void clBackendDX9::SetupRenderState(const stDrawData& stData)
	{
		const D3DVIEWPORT9 stViewport{ 0, 0, static_cast<DWORD>(stData.vecDisplaySize.x), static_cast<DWORD>(stData.vecDisplaySize.y), 0.f, 1.f };
		m_pDevice->SetViewport(&stViewport);
		m_pDevice->SetPixelShader(nullptr);
		m_pDevice->SetVertexShader(nullptr);
		m_pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		m_pDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
		m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		m_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		m_pDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		m_pDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
		m_pDevice->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
		m_pDevice->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
		m_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
		m_pDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
		m_pDevice->SetRenderState(D3DRS_RANGEFOGENABLE, FALSE);
		m_pDevice->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
		m_pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
		m_pDevice->SetRenderState(D3DRS_CLIPPING, TRUE);
		m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
		m_pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
		m_pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
		m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		m_pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
		m_pDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		m_pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
		m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		m_pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		m_pDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

		const float flL = stData.vecDisplayPosition.x + .5f;
		const float flR = stData.vecDisplayPosition.x + stData.vecDisplaySize.x + .5f;
		const float flT = stData.vecDisplayPosition.y + .5f;
		const float flB = stData.vecDisplayPosition.y + stData.vecDisplaySize.y + .5f;
		const D3DMATRIX stIdentity{ { { 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f } } };
		const D3DMATRIX stProjection{ { {
			2.f / (flR - flL), 0.f, 0.f, 0.f,
			0.f, 2.f / (flT - flB), 0.f, 0.f,
			0.f, 0.f, .5f, 0.f,
			(flL + flR) / (flL - flR), (flT + flB) / (flB - flT), .5f, 1.f,
		} } };
		m_pDevice->SetTransform(D3DTS_WORLD, &stIdentity);
		m_pDevice->SetTransform(D3DTS_VIEW, &stIdentity);
		m_pDevice->SetTransform(D3DTS_PROJECTION, &stProjection);
	}

	void clBackendDX9::RenderDrawData(const stDrawData& stData)
	{
		if (!m_pDevice || stData.uTotalIndices == 0 || stData.vecDisplaySize.x <= 0.f || stData.vecDisplaySize.y <= 0.f)
			return;
		if (!EnsureBuffers(m_pVertexBuffer, m_uVertexCapacity, stData.uTotalVertices, sizeof(stVertexDX9), uFVF, m_pIndexBuffer, m_uIndexCapacity, stData.uTotalIndices))
			return;

		IDirect3DStateBlock9* pStateBlock = nullptr;
		if (FAILED(m_pDevice->CreateStateBlock(D3DSBT_ALL, &pStateBlock)))
			return;
		pStateBlock->Capture();
		D3DMATRIX stWorld{}, stView{}, stProjection{};
		m_pDevice->GetTransform(D3DTS_WORLD, &stWorld);
		m_pDevice->GetTransform(D3DTS_VIEW, &stView);
		m_pDevice->GetTransform(D3DTS_PROJECTION, &stProjection);

		void* pVertexData = nullptr;
		void* pIndexData = nullptr;
		if (SUCCEEDED(m_pVertexBuffer->Lock(0, static_cast<UINT>(stData.uTotalVertices * sizeof(stVertexDX9)), &pVertexData, D3DLOCK_DISCARD)) &&
			SUCCEEDED(m_pIndexBuffer->Lock(0, static_cast<UINT>(stData.uTotalIndices * sizeof(std::uint32_t)), &pIndexData, D3DLOCK_DISCARD)))
		{
			auto* pVertexOut = static_cast<stVertexDX9*>(pVertexData);
			auto* pIndexOut = static_cast<std::uint32_t*>(pIndexData);
			for (const clDrawList* pList : stData.aLists)
			{
				for (const stVertex& rVertex : pList->GetVertices())
					*pVertexOut++ = { rVertex.vecPosition.x, rVertex.vecPosition.y, 0.f, SwizzleToARGB(rVertex.uColor), rVertex.vecUV.x, rVertex.vecUV.y };
				std::memcpy(pIndexOut, pList->GetIndices().data(), pList->GetIndices().size() * sizeof(std::uint32_t));
				pIndexOut += pList->GetIndices().size();
			}
			m_pVertexBuffer->Unlock();
			m_pIndexBuffer->Unlock();

			m_pDevice->SetStreamSource(0, m_pVertexBuffer, 0, sizeof(stVertexDX9));
			m_pDevice->SetIndices(m_pIndexBuffer);
			m_pDevice->SetFVF(uFVF);
			SetupRenderState(stData);

			UINT uGlobalIndex = 0;
			INT iGlobalVertex = 0;
			for (const clDrawList* pList : stData.aLists)
			{
				const auto uListVertices = static_cast<UINT>(pList->GetVertices().size());
				for (const stDrawCommand& rCommand : pList->GetCommands())
				{
					if (rCommand.uIndexCount == 0)
						continue;
					const rect rcClip = rCommand.rcClip.Clipped({ stData.vecDisplayPosition, stData.vecDisplayPosition + stData.vecDisplaySize });
					if (rcClip.IsEmpty())
						continue;
					const RECT stScissor{ static_cast<LONG>(rcClip.min.x - stData.vecDisplayPosition.x), static_cast<LONG>(rcClip.min.y - stData.vecDisplayPosition.y),
						static_cast<LONG>(rcClip.max.x - stData.vecDisplayPosition.x), static_cast<LONG>(rcClip.max.y - stData.vecDisplayPosition.y) };
					const stTexture* pTexture = HandleToPointer<stTexture>(rCommand.hTexture);
					m_pDevice->SetTexture(0, pTexture ? pTexture->pTexture : nullptr);
					m_pDevice->SetScissorRect(&stScissor);
					m_pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, iGlobalVertex, 0, uListVertices, uGlobalIndex + rCommand.uIndexOffset, rCommand.uIndexCount / 3);
				}
				uGlobalIndex += static_cast<UINT>(pList->GetIndices().size());
				iGlobalVertex += static_cast<INT>(uListVertices);
			}
		}

		m_pDevice->SetTransform(D3DTS_WORLD, &stWorld);
		m_pDevice->SetTransform(D3DTS_VIEW, &stView);
		m_pDevice->SetTransform(D3DTS_PROJECTION, &stProjection);
		pStateBlock->Apply();
		pStateBlock->Release();
	}

	void clBackendDX9::RenderWorld(const clWorldDrawList& rList, const stWorldPass& stPass)
	{
		if (!m_pDevice || !m_pWorldVertexShader || !m_pWorldPixelShader || !m_pWorldDeclaration || rList.IsEmpty())
			return;

		std::size_t uVertices = 0, uIndices = 0;
		for (std::size_t i = 0; i < static_cast<std::size_t>(EDepthBatch::Count); ++i)
		{
			uVertices += rList.GetBatch(static_cast<EDepthBatch>(i)).aVertices.size();
			uIndices += rList.GetBatch(static_cast<EDepthBatch>(i)).aIndices.size();
		}
		if (!EnsureBuffers(m_pWorldVertexBuffer, m_uWorldVertexCapacity, uVertices, sizeof(stWorldVertex), 0, m_pWorldIndexBuffer, m_uWorldIndexCapacity, uIndices))
			return;

		void* pVertexData = nullptr;
		void* pIndexData = nullptr;
		if (FAILED(m_pWorldVertexBuffer->Lock(0, static_cast<UINT>(uVertices * sizeof(stWorldVertex)), &pVertexData, D3DLOCK_DISCARD)))
			return;
		if (FAILED(m_pWorldIndexBuffer->Lock(0, static_cast<UINT>(uIndices * sizeof(std::uint32_t)), &pIndexData, D3DLOCK_DISCARD)))
		{
			m_pWorldVertexBuffer->Unlock();
			return;
		}
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
		m_pWorldVertexBuffer->Unlock();
		m_pWorldIndexBuffer->Unlock();

		IDirect3DStateBlock9* pStateBlock = nullptr;
		if (FAILED(m_pDevice->CreateStateBlock(D3DSBT_ALL, &pStateBlock)))
			return;
		pStateBlock->Capture();

		IDirect3DSurface9* pPreviousDepth = nullptr;
		if (m_pWorldDepth)
		{
			m_pDevice->GetDepthStencilSurface(&pPreviousDepth);
			m_pDevice->SetDepthStencilSurface(m_pWorldDepth);
		}

		D3DVIEWPORT9 stViewport{};
		m_pDevice->GetViewport(&stViewport);
		const float aHalfPixel[4] = { -1.f / static_cast<float>(std::max<DWORD>(stViewport.Width, 1)), 1.f / static_cast<float>(std::max<DWORD>(stViewport.Height, 1)), 0.f, 0.f };
		m_pDevice->SetVertexShaderConstantF(0, aHalfPixel, 1);
		m_pDevice->SetVertexDeclaration(m_pWorldDeclaration);
		m_pDevice->SetVertexShader(m_pWorldVertexShader);
		m_pDevice->SetPixelShader(m_pWorldPixelShader);
		m_pDevice->SetStreamSource(0, m_pWorldVertexBuffer, 0, sizeof(stWorldVertex));
		m_pDevice->SetIndices(m_pWorldIndexBuffer);
		const stTexture* pTexture = HandleToPointer<stTexture>(rList.GetTexture());
		m_pDevice->SetTexture(0, pTexture ? pTexture->pTexture : nullptr);
		m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		m_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		m_pDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
		m_pDevice->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
		m_pDevice->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
		m_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		m_pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
		m_pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);

		UINT uIndexBase = 0;
		INT iVertexBase = 0;
		for (std::size_t i = 0; i < static_cast<std::size_t>(EDepthBatch::Count); ++i)
		{
			const auto eBatch = static_cast<EDepthBatch>(i);
			const stWorldBatch& rBatch = rList.GetBatch(eBatch);
			if (!rBatch.aIndices.empty())
			{
				m_pDevice->SetRenderState(D3DRS_ZENABLE, eBatch == EDepthBatch::Always ? D3DZB_FALSE : D3DZB_TRUE);
				m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, eBatch == EDepthBatch::WriteDepth);
				m_pDevice->SetRenderState(D3DRS_ZFUNC, DepthFunction(eBatch, stPass.bReversedZ));
				m_pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, iVertexBase, 0, static_cast<UINT>(rBatch.aVertices.size()), uIndexBase, static_cast<UINT>(rBatch.aIndices.size() / 3));
			}
			uIndexBase += static_cast<UINT>(rBatch.aIndices.size());
			iVertexBase += static_cast<INT>(rBatch.aVertices.size());
		}

		if (m_pWorldDepth)
		{
			m_pDevice->SetDepthStencilSurface(pPreviousDepth);
			SafeRelease(pPreviousDepth);
		}
		pStateBlock->Apply();
		pStateBlock->Release();
	}
}
