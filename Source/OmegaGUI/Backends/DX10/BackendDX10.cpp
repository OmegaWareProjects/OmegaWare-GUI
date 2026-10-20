#include "OmegaGUI/Backends/DX10/BackendDX10.hpp"
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

		[[nodiscard]] D3D10_COMPARISON_FUNC DepthFunction(EDepthBatch eBatch, bool bReversed)
		{
			switch (eBatch)
			{
			case EDepthBatch::Behind: return bReversed ? D3D10_COMPARISON_LESS : D3D10_COMPARISON_GREATER;
			case EDepthBatch::Always: return D3D10_COMPARISON_ALWAYS;
			default: return bReversed ? D3D10_COMPARISON_GREATER_EQUAL : D3D10_COMPARISON_LESS_EQUAL;
			}
		}
	}

	struct clBackendDX10::stStateBackup
	{
		UINT uScissorCount = 0;
		UINT uViewportCount = 0;
		D3D10_RECT aScissors[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
		D3D10_VIEWPORT aViewports[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
		ID3D10RasterizerState* pRasterizer = nullptr;
		ID3D10BlendState* pBlend = nullptr;
		FLOAT aBlendFactor[4]{};
		UINT uSampleMask = 0;
		UINT uStencilRef = 0;
		ID3D10DepthStencilState* pDepthStencil = nullptr;
		ID3D10ShaderResourceView* pShaderResource = nullptr;
		ID3D10SamplerState* pSampler = nullptr;
		ID3D10PixelShader* pPixelShader = nullptr;
		ID3D10VertexShader* pVertexShader = nullptr;
		ID3D10GeometryShader* pGeometryShader = nullptr;
		D3D10_PRIMITIVE_TOPOLOGY eTopology = D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED;
		ID3D10Buffer* pIndexBuffer = nullptr;
		ID3D10Buffer* pVertexBuffer = nullptr;
		ID3D10Buffer* pVertexConstantBuffer = nullptr;
		UINT uIndexOffset = 0, uVertexStride = 0, uVertexOffset = 0;
		DXGI_FORMAT eIndexFormat = DXGI_FORMAT_UNKNOWN;
		ID3D10InputLayout* pInputLayout = nullptr;
		ID3D10RenderTargetView* pRenderTarget = nullptr;
		ID3D10DepthStencilView* pDepthView = nullptr;
	};

	clBackendDX10::~clBackendDX10()
	{
		Shutdown();
	}

	bool clBackendDX10::Initialize(ID3D10Device* pDevice)
	{
		m_pDevice = pDevice;
		return CreateDeviceObjects();
	}

	void clBackendDX10::Shutdown()
	{
		InvalidateDeviceObjects();
		m_pDevice.Reset();
	}

	void clBackendDX10::SetWorldTarget(ID3D10RenderTargetView* pRenderTarget, ID3D10DepthStencilView* pDepthStencil)
	{
		m_pWorldRenderTarget = pRenderTarget;
		m_pWorldDepthStencil = pDepthStencil;
	}

	bool clBackendDX10::CreateDeviceObjects()
	{
		if (!m_pDevice)
			return false;
		InvalidateDeviceObjects();

		const auto pVertexBlob = CompileShader(Shaders::szVertexHLSL, sizeof(Shaders::szVertexHLSL) - 1, "vs_4_0");
		const auto pWorldBlob = CompileShader(Shaders::szWorldVertexHLSL, sizeof(Shaders::szWorldVertexHLSL) - 1, "vs_4_0");
		const auto pPixelBlob = CompileShader(Shaders::szPixelHLSL, sizeof(Shaders::szPixelHLSL) - 1, "ps_4_0");
		if (!pVertexBlob || !pWorldBlob || !pPixelBlob)
			return false;
		if (FAILED(m_pDevice->CreateVertexShader(pVertexBlob->GetBufferPointer(), pVertexBlob->GetBufferSize(), &m_pVertexShader)) ||
			FAILED(m_pDevice->CreateVertexShader(pWorldBlob->GetBufferPointer(), pWorldBlob->GetBufferSize(), &m_pWorldVertexShader)) ||
			FAILED(m_pDevice->CreatePixelShader(pPixelBlob->GetBufferPointer(), pPixelBlob->GetBufferSize(), &m_pPixelShader)))
			return false;

		const D3D10_INPUT_ELEMENT_DESC aLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(stVertex, vecPosition), D3D10_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(stVertex, vecUV), D3D10_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(stVertex, uColor), D3D10_INPUT_PER_VERTEX_DATA, 0 },
		};
		const D3D10_INPUT_ELEMENT_DESC aWorldLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(stWorldVertex, vecClip), D3D10_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(stWorldVertex, vecUV), D3D10_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(stWorldVertex, uColor), D3D10_INPUT_PER_VERTEX_DATA, 0 },
		};
		if (FAILED(m_pDevice->CreateInputLayout(aLayout, 3, pVertexBlob->GetBufferPointer(), pVertexBlob->GetBufferSize(), &m_pInputLayout)) ||
			FAILED(m_pDevice->CreateInputLayout(aWorldLayout, 3, pWorldBlob->GetBufferPointer(), pWorldBlob->GetBufferSize(), &m_pWorldInputLayout)))
			return false;

		D3D10_BUFFER_DESC stConstant{ sizeof(float) * 16, D3D10_USAGE_DYNAMIC, D3D10_BIND_CONSTANT_BUFFER, D3D10_CPU_ACCESS_WRITE, 0 };
		if (FAILED(m_pDevice->CreateBuffer(&stConstant, nullptr, &m_pConstantBuffer)))
			return false;

		D3D10_BLEND_DESC stBlend{};
		stBlend.BlendEnable[0] = TRUE;
		stBlend.SrcBlend = D3D10_BLEND_SRC_ALPHA;
		stBlend.DestBlend = D3D10_BLEND_INV_SRC_ALPHA;
		stBlend.BlendOp = D3D10_BLEND_OP_ADD;
		stBlend.SrcBlendAlpha = D3D10_BLEND_ONE;
		stBlend.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
		stBlend.BlendOpAlpha = D3D10_BLEND_OP_ADD;
		stBlend.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;
		m_pDevice->CreateBlendState(&stBlend, &m_pBlendState);

		D3D10_RASTERIZER_DESC stRasterizer{};
		stRasterizer.FillMode = D3D10_FILL_SOLID;
		stRasterizer.CullMode = D3D10_CULL_NONE;
		stRasterizer.ScissorEnable = TRUE;
		stRasterizer.DepthClipEnable = TRUE;
		m_pDevice->CreateRasterizerState(&stRasterizer, &m_pRasterizerState);
		stRasterizer.ScissorEnable = FALSE;
		m_pDevice->CreateRasterizerState(&stRasterizer, &m_pWorldRasterizerState);

		D3D10_DEPTH_STENCIL_DESC stDepth{};
		stDepth.DepthEnable = FALSE;
		stDepth.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ALL;
		stDepth.DepthFunc = D3D10_COMPARISON_ALWAYS;
		stDepth.FrontFace.StencilFailOp = stDepth.FrontFace.StencilDepthFailOp = stDepth.FrontFace.StencilPassOp = D3D10_STENCIL_OP_KEEP;
		stDepth.FrontFace.StencilFunc = D3D10_COMPARISON_ALWAYS;
		stDepth.BackFace = stDepth.FrontFace;
		m_pDevice->CreateDepthStencilState(&stDepth, &m_pDepthDisabled);
		for (std::size_t uBatch = 0; uBatch < static_cast<std::size_t>(EDepthBatch::Count); ++uBatch)
			for (int iReversed = 0; iReversed < 2; ++iReversed)
			{
				const auto eBatch = static_cast<EDepthBatch>(uBatch);
				D3D10_DEPTH_STENCIL_DESC stWorld = stDepth;
				stWorld.DepthEnable = eBatch != EDepthBatch::Always;
				stWorld.DepthWriteMask = eBatch == EDepthBatch::WriteDepth ? D3D10_DEPTH_WRITE_MASK_ALL : D3D10_DEPTH_WRITE_MASK_ZERO;
				stWorld.DepthFunc = DepthFunction(eBatch, iReversed != 0);
				m_pDevice->CreateDepthStencilState(&stWorld, &m_aWorldDepthStates[uBatch * 2 + iReversed]);
			}

		D3D10_SAMPLER_DESC stSampler{};
		stSampler.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
		stSampler.AddressU = stSampler.AddressV = stSampler.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;
		stSampler.ComparisonFunc = D3D10_COMPARISON_ALWAYS;
		m_pDevice->CreateSamplerState(&stSampler, &m_pSampler);
		return true;
	}

	void clBackendDX10::InvalidateDeviceObjects()
	{
		m_pVertexShader.Reset();
		m_pWorldVertexShader.Reset();
		m_pPixelShader.Reset();
		m_pInputLayout.Reset();
		m_pWorldInputLayout.Reset();
		m_pConstantBuffer.Reset();
		m_pSampler.Reset();
		m_pBlendState.Reset();
		m_pRasterizerState.Reset();
		m_pWorldRasterizerState.Reset();
		m_pDepthDisabled.Reset();
		for (auto& pState : m_aWorldDepthStates)
			pState.Reset();
		m_pVertexBuffer.Reset();
		m_pIndexBuffer.Reset();
		m_pWorldVertexBuffer.Reset();
		m_pWorldIndexBuffer.Reset();
		m_uVertexCapacity = m_uIndexCapacity = m_uWorldVertexCapacity = m_uWorldIndexCapacity = 0;
	}

	TextureHandle clBackendDX10::CreateTexture(const std::uint8_t* pPixelsRGBA, int iWidth, int iHeight)
	{
		if (!m_pDevice)
			return 0;
		D3D10_TEXTURE2D_DESC stDesc{};
		stDesc.Width = static_cast<UINT>(iWidth);
		stDesc.Height = static_cast<UINT>(iHeight);
		stDesc.MipLevels = 1;
		stDesc.ArraySize = 1;
		stDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		stDesc.SampleDesc.Count = 1;
		stDesc.Usage = D3D10_USAGE_DEFAULT;
		stDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
		const D3D10_SUBRESOURCE_DATA stData{ pPixelsRGBA, static_cast<UINT>(iWidth * 4), 0 };
		ComPtr<ID3D10Texture2D> pTexture;
		if (FAILED(m_pDevice->CreateTexture2D(&stDesc, &stData, &pTexture)))
			return 0;
		D3D10_SHADER_RESOURCE_VIEW_DESC stView{};
		stView.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		stView.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
		stView.Texture2D.MipLevels = 1;
		ID3D10ShaderResourceView* pView = nullptr;
		if (FAILED(m_pDevice->CreateShaderResourceView(pTexture.Get(), &stView, &pView)))
			return 0;
		return PointerToHandle(pView);
	}

	void clBackendDX10::DestroyTexture(TextureHandle hTexture)
	{
		if (hTexture)
			HandleToPointer<ID3D10ShaderResourceView>(hTexture)->Release();
	}

	bool clBackendDX10::EnsureBuffer(ComPtr<ID3D10Buffer>& rBuffer, std::size_t& rCapacity, std::size_t uRequired, std::size_t uStride, UINT uBindFlags)
	{
		if (rBuffer && rCapacity >= uRequired)
			return true;
		rBuffer.Reset();
		rCapacity = std::max<std::size_t>(uRequired + uRequired / 2, 4096);
		D3D10_BUFFER_DESC stDesc{ static_cast<UINT>(rCapacity * uStride), D3D10_USAGE_DYNAMIC, uBindFlags, D3D10_CPU_ACCESS_WRITE, 0 };
		if (FAILED(m_pDevice->CreateBuffer(&stDesc, nullptr, &rBuffer)))
		{
			rCapacity = 0;
			return false;
		}
		return true;
	}

	void clBackendDX10::BackupState(stStateBackup& rBackup)
	{
		ID3D10Device* pDevice = m_pDevice.Get();
		rBackup.uScissorCount = rBackup.uViewportCount = D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
		pDevice->RSGetScissorRects(&rBackup.uScissorCount, rBackup.aScissors);
		pDevice->RSGetViewports(&rBackup.uViewportCount, rBackup.aViewports);
		pDevice->RSGetState(&rBackup.pRasterizer);
		pDevice->OMGetBlendState(&rBackup.pBlend, rBackup.aBlendFactor, &rBackup.uSampleMask);
		pDevice->OMGetDepthStencilState(&rBackup.pDepthStencil, &rBackup.uStencilRef);
		pDevice->OMGetRenderTargets(1, &rBackup.pRenderTarget, &rBackup.pDepthView);
		pDevice->PSGetShaderResources(0, 1, &rBackup.pShaderResource);
		pDevice->PSGetSamplers(0, 1, &rBackup.pSampler);
		pDevice->PSGetShader(&rBackup.pPixelShader);
		pDevice->VSGetShader(&rBackup.pVertexShader);
		pDevice->VSGetConstantBuffers(0, 1, &rBackup.pVertexConstantBuffer);
		pDevice->GSGetShader(&rBackup.pGeometryShader);
		pDevice->IAGetPrimitiveTopology(&rBackup.eTopology);
		pDevice->IAGetIndexBuffer(&rBackup.pIndexBuffer, &rBackup.eIndexFormat, &rBackup.uIndexOffset);
		pDevice->IAGetVertexBuffers(0, 1, &rBackup.pVertexBuffer, &rBackup.uVertexStride, &rBackup.uVertexOffset);
		pDevice->IAGetInputLayout(&rBackup.pInputLayout);
	}

	void clBackendDX10::RestoreState(stStateBackup& rBackup)
	{
		ID3D10Device* pDevice = m_pDevice.Get();
		auto Release = [](auto*& pObject) {
			if (pObject)
				pObject->Release();
			pObject = nullptr;
		};
		pDevice->RSSetScissorRects(rBackup.uScissorCount, rBackup.aScissors);
		pDevice->RSSetViewports(rBackup.uViewportCount, rBackup.aViewports);
		pDevice->RSSetState(rBackup.pRasterizer);
		pDevice->OMSetBlendState(rBackup.pBlend, rBackup.aBlendFactor, rBackup.uSampleMask);
		pDevice->OMSetDepthStencilState(rBackup.pDepthStencil, rBackup.uStencilRef);
		pDevice->OMSetRenderTargets(1, &rBackup.pRenderTarget, rBackup.pDepthView);
		pDevice->PSSetShaderResources(0, 1, &rBackup.pShaderResource);
		pDevice->PSSetSamplers(0, 1, &rBackup.pSampler);
		pDevice->PSSetShader(rBackup.pPixelShader);
		pDevice->VSSetShader(rBackup.pVertexShader);
		pDevice->VSSetConstantBuffers(0, 1, &rBackup.pVertexConstantBuffer);
		pDevice->GSSetShader(rBackup.pGeometryShader);
		pDevice->IASetPrimitiveTopology(rBackup.eTopology);
		pDevice->IASetIndexBuffer(rBackup.pIndexBuffer, rBackup.eIndexFormat, rBackup.uIndexOffset);
		pDevice->IASetVertexBuffers(0, 1, &rBackup.pVertexBuffer, &rBackup.uVertexStride, &rBackup.uVertexOffset);
		pDevice->IASetInputLayout(rBackup.pInputLayout);
		Release(rBackup.pRasterizer);
		Release(rBackup.pBlend);
		Release(rBackup.pDepthStencil);
		Release(rBackup.pRenderTarget);
		Release(rBackup.pDepthView);
		Release(rBackup.pShaderResource);
		Release(rBackup.pSampler);
		Release(rBackup.pPixelShader);
		Release(rBackup.pVertexShader);
		Release(rBackup.pVertexConstantBuffer);
		Release(rBackup.pGeometryShader);
		Release(rBackup.pIndexBuffer);
		Release(rBackup.pVertexBuffer);
		Release(rBackup.pInputLayout);
	}

	void clBackendDX10::RenderDrawData(const stDrawData& stData)
	{
		if (!m_pDevice || !m_pVertexShader || stData.uTotalIndices == 0 || stData.vecDisplaySize.x <= 0.f || stData.vecDisplaySize.y <= 0.f)
			return;
		ID3D10Device* pDevice = m_pDevice.Get();
		if (!EnsureBuffer(m_pVertexBuffer, m_uVertexCapacity, stData.uTotalVertices, sizeof(stVertex), D3D10_BIND_VERTEX_BUFFER) ||
			!EnsureBuffer(m_pIndexBuffer, m_uIndexCapacity, stData.uTotalIndices, sizeof(std::uint32_t), D3D10_BIND_INDEX_BUFFER))
			return;

		void* pVertexData = nullptr;
		void* pIndexData = nullptr;
		if (FAILED(m_pVertexBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &pVertexData)))
			return;
		if (FAILED(m_pIndexBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &pIndexData)))
		{
			m_pVertexBuffer->Unmap();
			return;
		}
		auto* pVertexOut = static_cast<stVertex*>(pVertexData);
		auto* pIndexOut = static_cast<std::uint32_t*>(pIndexData);
		for (const clDrawList* pList : stData.aLists)
		{
			std::memcpy(pVertexOut, pList->GetVertices().data(), pList->GetVertices().size() * sizeof(stVertex));
			std::memcpy(pIndexOut, pList->GetIndices().data(), pList->GetIndices().size() * sizeof(std::uint32_t));
			pVertexOut += pList->GetVertices().size();
			pIndexOut += pList->GetIndices().size();
		}
		m_pVertexBuffer->Unmap();
		m_pIndexBuffer->Unmap();

		void* pConstantData = nullptr;
		if (SUCCEEDED(m_pConstantBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &pConstantData)))
		{
			const auto aProjection = stData.OrthoProjection();
			std::memcpy(pConstantData, aProjection.data(), sizeof(float) * 16);
			m_pConstantBuffer->Unmap();
		}

		stStateBackup stBackup;
		BackupState(stBackup);

		const D3D10_VIEWPORT stViewport{ 0, 0, static_cast<UINT>(stData.vecDisplaySize.x), static_cast<UINT>(stData.vecDisplaySize.y), 0.f, 1.f };
		pDevice->RSSetViewports(1, &stViewport);
		const UINT uStride = sizeof(stVertex), uOffset = 0;
		pDevice->IASetInputLayout(m_pInputLayout.Get());
		pDevice->IASetVertexBuffers(0, 1, m_pVertexBuffer.GetAddressOf(), &uStride, &uOffset);
		pDevice->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		pDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pDevice->VSSetShader(m_pVertexShader.Get());
		pDevice->VSSetConstantBuffers(0, 1, m_pConstantBuffer.GetAddressOf());
		pDevice->PSSetShader(m_pPixelShader.Get());
		pDevice->PSSetSamplers(0, 1, m_pSampler.GetAddressOf());
		pDevice->GSSetShader(nullptr);
		const float aBlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
		pDevice->OMSetBlendState(m_pBlendState.Get(), aBlendFactor, 0xFFFFFFFF);
		pDevice->OMSetDepthStencilState(m_pDepthDisabled.Get(), 0);
		pDevice->RSSetState(m_pRasterizerState.Get());

		UINT uGlobalIndex = 0;
		INT iGlobalVertex = 0;
		for (const clDrawList* pList : stData.aLists)
		{
			for (const stDrawCommand& rCommand : pList->GetCommands())
			{
				if (rCommand.uIndexCount == 0)
					continue;
				const rect rcClip = rCommand.rcClip.Clipped({ stData.vecDisplayPosition, stData.vecDisplayPosition + stData.vecDisplaySize });
				if (rcClip.IsEmpty())
					continue;
				const D3D10_RECT stScissor{ static_cast<LONG>(rcClip.min.x - stData.vecDisplayPosition.x), static_cast<LONG>(rcClip.min.y - stData.vecDisplayPosition.y),
					static_cast<LONG>(rcClip.max.x - stData.vecDisplayPosition.x), static_cast<LONG>(rcClip.max.y - stData.vecDisplayPosition.y) };
				pDevice->RSSetScissorRects(1, &stScissor);
				auto* pView = HandleToPointer<ID3D10ShaderResourceView>(rCommand.hTexture);
				pDevice->PSSetShaderResources(0, 1, &pView);
				pDevice->DrawIndexed(rCommand.uIndexCount, uGlobalIndex + rCommand.uIndexOffset, iGlobalVertex);
			}
			uGlobalIndex += static_cast<UINT>(pList->GetIndices().size());
			iGlobalVertex += static_cast<INT>(pList->GetVertices().size());
		}

		RestoreState(stBackup);
	}

	void clBackendDX10::RenderWorld(const clWorldDrawList& rList, const stWorldPass& stPass)
	{
		if (!m_pDevice || !m_pWorldVertexShader || rList.IsEmpty())
			return;
		ID3D10Device* pDevice = m_pDevice.Get();

		std::size_t uVertices = 0, uIndices = 0;
		for (std::size_t i = 0; i < static_cast<std::size_t>(EDepthBatch::Count); ++i)
		{
			uVertices += rList.GetBatch(static_cast<EDepthBatch>(i)).aVertices.size();
			uIndices += rList.GetBatch(static_cast<EDepthBatch>(i)).aIndices.size();
		}
		if (!EnsureBuffer(m_pWorldVertexBuffer, m_uWorldVertexCapacity, uVertices, sizeof(stWorldVertex), D3D10_BIND_VERTEX_BUFFER) ||
			!EnsureBuffer(m_pWorldIndexBuffer, m_uWorldIndexCapacity, uIndices, sizeof(std::uint32_t), D3D10_BIND_INDEX_BUFFER))
			return;

		void* pVertexData = nullptr;
		void* pIndexData = nullptr;
		if (FAILED(m_pWorldVertexBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &pVertexData)))
			return;
		if (FAILED(m_pWorldIndexBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &pIndexData)))
		{
			m_pWorldVertexBuffer->Unmap();
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
		m_pWorldVertexBuffer->Unmap();
		m_pWorldIndexBuffer->Unmap();

		stStateBackup stBackup;
		BackupState(stBackup);

		if (m_pWorldRenderTarget)
		{
			pDevice->OMSetRenderTargets(1, &m_pWorldRenderTarget, m_pWorldDepthStencil);
			ComPtr<ID3D10Resource> pResource;
			m_pWorldRenderTarget->GetResource(&pResource);
			ComPtr<ID3D10Texture2D> pTexture;
			if (SUCCEEDED(pResource.As(&pTexture)))
			{
				D3D10_TEXTURE2D_DESC stDesc{};
				pTexture->GetDesc(&stDesc);
				const D3D10_VIEWPORT stViewport{ 0, 0, stDesc.Width, stDesc.Height, 0.f, 1.f };
				pDevice->RSSetViewports(1, &stViewport);
			}
		}

		const UINT uStride = sizeof(stWorldVertex), uOffset = 0;
		pDevice->IASetInputLayout(m_pWorldInputLayout.Get());
		pDevice->IASetVertexBuffers(0, 1, m_pWorldVertexBuffer.GetAddressOf(), &uStride, &uOffset);
		pDevice->IASetIndexBuffer(m_pWorldIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		pDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pDevice->VSSetShader(m_pWorldVertexShader.Get());
		pDevice->PSSetShader(m_pPixelShader.Get());
		pDevice->PSSetSamplers(0, 1, m_pSampler.GetAddressOf());
		pDevice->GSSetShader(nullptr);
		const float aBlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
		pDevice->OMSetBlendState(m_pBlendState.Get(), aBlendFactor, 0xFFFFFFFF);
		pDevice->RSSetState(m_pWorldRasterizerState.Get());
		auto* pView = HandleToPointer<ID3D10ShaderResourceView>(rList.GetTexture());
		pDevice->PSSetShaderResources(0, 1, &pView);

		UINT uIndexBase = 0;
		INT iVertexBase = 0;
		for (std::size_t i = 0; i < static_cast<std::size_t>(EDepthBatch::Count); ++i)
		{
			const stWorldBatch& rBatch = rList.GetBatch(static_cast<EDepthBatch>(i));
			if (!rBatch.aIndices.empty())
			{
				pDevice->OMSetDepthStencilState(m_aWorldDepthStates[i * 2 + (stPass.bReversedZ ? 1 : 0)].Get(), 0);
				pDevice->DrawIndexed(static_cast<UINT>(rBatch.aIndices.size()), uIndexBase, iVertexBase);
			}
			uIndexBase += static_cast<UINT>(rBatch.aIndices.size());
			iVertexBase += static_cast<INT>(rBatch.aVertices.size());
		}

		RestoreState(stBackup);
	}
}
