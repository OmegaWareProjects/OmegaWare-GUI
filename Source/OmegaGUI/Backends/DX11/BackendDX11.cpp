#include "OmegaGUI/Backends/DX11/BackendDX11.hpp"
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

		[[nodiscard]] D3D11_COMPARISON_FUNC DepthFunction(EDepthBatch eBatch, bool bReversed)
		{
			switch (eBatch)
			{
			case EDepthBatch::Behind: return bReversed ? D3D11_COMPARISON_LESS : D3D11_COMPARISON_GREATER;
			case EDepthBatch::Always: return D3D11_COMPARISON_ALWAYS;
			default: return bReversed ? D3D11_COMPARISON_GREATER_EQUAL : D3D11_COMPARISON_LESS_EQUAL;
			}
		}
	}

	struct clBackendDX11::stStateBackup
	{
		UINT uScissorCount = 0;
		UINT uViewportCount = 0;
		D3D11_RECT aScissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
		D3D11_VIEWPORT aViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
		ID3D11RasterizerState* pRasterizer = nullptr;
		ID3D11BlendState* pBlend = nullptr;
		FLOAT aBlendFactor[4]{};
		UINT uSampleMask = 0;
		UINT uStencilRef = 0;
		ID3D11DepthStencilState* pDepthStencil = nullptr;
		ID3D11ShaderResourceView* pShaderResource = nullptr;
		ID3D11SamplerState* pSampler = nullptr;
		ID3D11PixelShader* pPixelShader = nullptr;
		ID3D11VertexShader* pVertexShader = nullptr;
		ID3D11GeometryShader* pGeometryShader = nullptr;
		ID3D11HullShader* pHullShader = nullptr;
		ID3D11DomainShader* pDomainShader = nullptr;
		UINT uPixelInstances = 0, uVertexInstances = 0, uGeometryInstances = 0;
		ID3D11ClassInstance* aPixelInstances[256]{};
		ID3D11ClassInstance* aVertexInstances[256]{};
		ID3D11ClassInstance* aGeometryInstances[256]{};
		D3D11_PRIMITIVE_TOPOLOGY eTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
		ID3D11Buffer* pIndexBuffer = nullptr;
		ID3D11Buffer* pVertexBuffer = nullptr;
		ID3D11Buffer* pVertexConstantBuffer = nullptr;
		UINT uIndexOffset = 0, uVertexStride = 0, uVertexOffset = 0;
		DXGI_FORMAT eIndexFormat = DXGI_FORMAT_UNKNOWN;
		ID3D11InputLayout* pInputLayout = nullptr;
		ID3D11RenderTargetView* pRenderTarget = nullptr;
		ID3D11DepthStencilView* pDepthView = nullptr;
	};

	clBackendDX11::~clBackendDX11()
	{
		Shutdown();
	}

	bool clBackendDX11::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	{
		m_pDevice = pDevice;
		m_pContext = pContext;
		return CreateDeviceObjects();
	}

	void clBackendDX11::Shutdown()
	{
		InvalidateDeviceObjects();
		m_pContext.Reset();
		m_pDevice.Reset();
	}

	void clBackendDX11::SetWorldTarget(ID3D11RenderTargetView* pRenderTarget, ID3D11DepthStencilView* pDepthStencil)
	{
		m_pWorldRenderTarget = pRenderTarget;
		m_pWorldDepthStencil = pDepthStencil;
	}

	bool clBackendDX11::CreateDeviceObjects()
	{
		if (!m_pDevice)
			return false;
		InvalidateDeviceObjects();

		const auto pVertexBlob = CompileShader(Shaders::szVertexHLSL, sizeof(Shaders::szVertexHLSL) - 1, "vs_4_0");
		const auto pWorldBlob = CompileShader(Shaders::szWorldVertexHLSL, sizeof(Shaders::szWorldVertexHLSL) - 1, "vs_4_0");
		const auto pPixelBlob = CompileShader(Shaders::szPixelHLSL, sizeof(Shaders::szPixelHLSL) - 1, "ps_4_0");
		if (!pVertexBlob || !pWorldBlob || !pPixelBlob)
			return false;

		if (FAILED(m_pDevice->CreateVertexShader(pVertexBlob->GetBufferPointer(), pVertexBlob->GetBufferSize(), nullptr, &m_pVertexShader)) ||
			FAILED(m_pDevice->CreateVertexShader(pWorldBlob->GetBufferPointer(), pWorldBlob->GetBufferSize(), nullptr, &m_pWorldVertexShader)) ||
			FAILED(m_pDevice->CreatePixelShader(pPixelBlob->GetBufferPointer(), pPixelBlob->GetBufferSize(), nullptr, &m_pPixelShader)))
			return false;

		const D3D11_INPUT_ELEMENT_DESC aLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(stVertex, vecPosition), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(stVertex, vecUV), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(stVertex, uColor), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		const D3D11_INPUT_ELEMENT_DESC aWorldLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(stWorldVertex, vecClip), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(stWorldVertex, vecUV), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(stWorldVertex, uColor), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		if (FAILED(m_pDevice->CreateInputLayout(aLayout, 3, pVertexBlob->GetBufferPointer(), pVertexBlob->GetBufferSize(), &m_pInputLayout)) ||
			FAILED(m_pDevice->CreateInputLayout(aWorldLayout, 3, pWorldBlob->GetBufferPointer(), pWorldBlob->GetBufferSize(), &m_pWorldInputLayout)))
			return false;

		D3D11_BUFFER_DESC stConstant{ sizeof(float) * 16, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
		if (FAILED(m_pDevice->CreateBuffer(&stConstant, nullptr, &m_pConstantBuffer)))
			return false;

		D3D11_BLEND_DESC stBlend{};
		stBlend.RenderTarget[0].BlendEnable = TRUE;
		stBlend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		stBlend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		stBlend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		stBlend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		stBlend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		stBlend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		stBlend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		m_pDevice->CreateBlendState(&stBlend, &m_pBlendState);

		D3D11_RASTERIZER_DESC stRasterizer{};
		stRasterizer.FillMode = D3D11_FILL_SOLID;
		stRasterizer.CullMode = D3D11_CULL_NONE;
		stRasterizer.ScissorEnable = TRUE;
		stRasterizer.DepthClipEnable = TRUE;
		m_pDevice->CreateRasterizerState(&stRasterizer, &m_pRasterizerState);
		stRasterizer.ScissorEnable = FALSE;
		m_pDevice->CreateRasterizerState(&stRasterizer, &m_pWorldRasterizerState);

		D3D11_DEPTH_STENCIL_DESC stDepth{};
		stDepth.DepthEnable = FALSE;
		stDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		stDepth.DepthFunc = D3D11_COMPARISON_ALWAYS;
		stDepth.FrontFace.StencilFailOp = stDepth.FrontFace.StencilDepthFailOp = stDepth.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stDepth.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		stDepth.BackFace = stDepth.FrontFace;
		m_pDevice->CreateDepthStencilState(&stDepth, &m_pDepthDisabled);

		for (std::size_t uBatch = 0; uBatch < static_cast<std::size_t>(EDepthBatch::Count); ++uBatch)
			for (int iReversed = 0; iReversed < 2; ++iReversed)
			{
				const auto eBatch = static_cast<EDepthBatch>(uBatch);
				D3D11_DEPTH_STENCIL_DESC stWorld = stDepth;
				stWorld.DepthEnable = eBatch != EDepthBatch::Always;
				stWorld.DepthWriteMask = eBatch == EDepthBatch::WriteDepth ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
				stWorld.DepthFunc = DepthFunction(eBatch, iReversed != 0);
				m_pDevice->CreateDepthStencilState(&stWorld, &m_aWorldDepthStates[uBatch * 2 + iReversed]);
			}

		D3D11_SAMPLER_DESC stSampler{};
		stSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		stSampler.AddressU = stSampler.AddressV = stSampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		stSampler.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		m_pDevice->CreateSamplerState(&stSampler, &m_pSampler);
		return true;
	}

	void clBackendDX11::InvalidateDeviceObjects()
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

	TextureHandle clBackendDX11::CreateTexture(const std::uint8_t* pPixelsRGBA, int iWidth, int iHeight)
	{
		if (!m_pDevice)
			return 0;
		D3D11_TEXTURE2D_DESC stDesc{};
		stDesc.Width = static_cast<UINT>(iWidth);
		stDesc.Height = static_cast<UINT>(iHeight);
		stDesc.MipLevels = 1;
		stDesc.ArraySize = 1;
		stDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		stDesc.SampleDesc.Count = 1;
		stDesc.Usage = D3D11_USAGE_DEFAULT;
		stDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		const D3D11_SUBRESOURCE_DATA stData{ pPixelsRGBA, static_cast<UINT>(iWidth * 4), 0 };
		ComPtr<ID3D11Texture2D> pTexture;
		if (FAILED(m_pDevice->CreateTexture2D(&stDesc, &stData, &pTexture)))
			return 0;
		D3D11_SHADER_RESOURCE_VIEW_DESC stView{};
		stView.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		stView.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		stView.Texture2D.MipLevels = 1;
		ID3D11ShaderResourceView* pView = nullptr;
		if (FAILED(m_pDevice->CreateShaderResourceView(pTexture.Get(), &stView, &pView)))
			return 0;
		return PointerToHandle(pView);
	}

	void clBackendDX11::DestroyTexture(TextureHandle hTexture)
	{
		if (hTexture)
			HandleToPointer<ID3D11ShaderResourceView>(hTexture)->Release();
	}

	bool clBackendDX11::EnsureBuffer(ComPtr<ID3D11Buffer>& rBuffer, std::size_t& rCapacity, std::size_t uRequired, std::size_t uStride, UINT uBindFlags)
	{
		if (rBuffer && rCapacity >= uRequired)
			return true;
		rBuffer.Reset();
		rCapacity = std::max<std::size_t>(uRequired + uRequired / 2, 4096);
		D3D11_BUFFER_DESC stDesc{ static_cast<UINT>(rCapacity * uStride), D3D11_USAGE_DYNAMIC, uBindFlags, D3D11_CPU_ACCESS_WRITE, 0, 0 };
		if (FAILED(m_pDevice->CreateBuffer(&stDesc, nullptr, &rBuffer)))
		{
			rCapacity = 0;
			return false;
		}
		return true;
	}

	void clBackendDX11::BackupState(stStateBackup& rBackup)
	{
		ID3D11DeviceContext* pContext = m_pContext.Get();
		rBackup.uScissorCount = rBackup.uViewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
		pContext->RSGetScissorRects(&rBackup.uScissorCount, rBackup.aScissors);
		pContext->RSGetViewports(&rBackup.uViewportCount, rBackup.aViewports);
		pContext->RSGetState(&rBackup.pRasterizer);
		pContext->OMGetBlendState(&rBackup.pBlend, rBackup.aBlendFactor, &rBackup.uSampleMask);
		pContext->OMGetDepthStencilState(&rBackup.pDepthStencil, &rBackup.uStencilRef);
		pContext->OMGetRenderTargets(1, &rBackup.pRenderTarget, &rBackup.pDepthView);
		pContext->PSGetShaderResources(0, 1, &rBackup.pShaderResource);
		pContext->PSGetSamplers(0, 1, &rBackup.pSampler);
		rBackup.uPixelInstances = rBackup.uVertexInstances = rBackup.uGeometryInstances = 256;
		pContext->PSGetShader(&rBackup.pPixelShader, rBackup.aPixelInstances, &rBackup.uPixelInstances);
		pContext->VSGetShader(&rBackup.pVertexShader, rBackup.aVertexInstances, &rBackup.uVertexInstances);
		pContext->VSGetConstantBuffers(0, 1, &rBackup.pVertexConstantBuffer);
		pContext->GSGetShader(&rBackup.pGeometryShader, rBackup.aGeometryInstances, &rBackup.uGeometryInstances);
		pContext->HSGetShader(&rBackup.pHullShader, nullptr, nullptr);
		pContext->DSGetShader(&rBackup.pDomainShader, nullptr, nullptr);
		pContext->IAGetPrimitiveTopology(&rBackup.eTopology);
		pContext->IAGetIndexBuffer(&rBackup.pIndexBuffer, &rBackup.eIndexFormat, &rBackup.uIndexOffset);
		pContext->IAGetVertexBuffers(0, 1, &rBackup.pVertexBuffer, &rBackup.uVertexStride, &rBackup.uVertexOffset);
		pContext->IAGetInputLayout(&rBackup.pInputLayout);
	}

	void clBackendDX11::RestoreState(stStateBackup& rBackup)
	{
		ID3D11DeviceContext* pContext = m_pContext.Get();
		auto Release = [](auto*& pObject) {
			if (pObject)
				pObject->Release();
			pObject = nullptr;
		};
		pContext->RSSetScissorRects(rBackup.uScissorCount, rBackup.aScissors);
		pContext->RSSetViewports(rBackup.uViewportCount, rBackup.aViewports);
		pContext->RSSetState(rBackup.pRasterizer);
		pContext->OMSetBlendState(rBackup.pBlend, rBackup.aBlendFactor, rBackup.uSampleMask);
		pContext->OMSetDepthStencilState(rBackup.pDepthStencil, rBackup.uStencilRef);
		pContext->OMSetRenderTargets(1, &rBackup.pRenderTarget, rBackup.pDepthView);
		pContext->PSSetShaderResources(0, 1, &rBackup.pShaderResource);
		pContext->PSSetSamplers(0, 1, &rBackup.pSampler);
		pContext->PSSetShader(rBackup.pPixelShader, rBackup.aPixelInstances, rBackup.uPixelInstances);
		pContext->VSSetShader(rBackup.pVertexShader, rBackup.aVertexInstances, rBackup.uVertexInstances);
		pContext->VSSetConstantBuffers(0, 1, &rBackup.pVertexConstantBuffer);
		pContext->GSSetShader(rBackup.pGeometryShader, rBackup.aGeometryInstances, rBackup.uGeometryInstances);
		pContext->HSSetShader(rBackup.pHullShader, nullptr, 0);
		pContext->DSSetShader(rBackup.pDomainShader, nullptr, 0);
		pContext->IASetPrimitiveTopology(rBackup.eTopology);
		pContext->IASetIndexBuffer(rBackup.pIndexBuffer, rBackup.eIndexFormat, rBackup.uIndexOffset);
		pContext->IASetVertexBuffers(0, 1, &rBackup.pVertexBuffer, &rBackup.uVertexStride, &rBackup.uVertexOffset);
		pContext->IASetInputLayout(rBackup.pInputLayout);

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
		Release(rBackup.pHullShader);
		Release(rBackup.pDomainShader);
		Release(rBackup.pIndexBuffer);
		Release(rBackup.pVertexBuffer);
		Release(rBackup.pInputLayout);
		for (UINT i = 0; i < rBackup.uPixelInstances; ++i) Release(rBackup.aPixelInstances[i]);
		for (UINT i = 0; i < rBackup.uVertexInstances; ++i) Release(rBackup.aVertexInstances[i]);
		for (UINT i = 0; i < rBackup.uGeometryInstances; ++i) Release(rBackup.aGeometryInstances[i]);
	}

	void clBackendDX11::RenderDrawData(const stDrawData& stData)
	{
		if (!m_pContext || !m_pVertexShader || stData.uTotalIndices == 0 || stData.vecDisplaySize.x <= 0.f || stData.vecDisplaySize.y <= 0.f)
			return;
		ID3D11DeviceContext* pContext = m_pContext.Get();
		if (!EnsureBuffer(m_pVertexBuffer, m_uVertexCapacity, stData.uTotalVertices, sizeof(stVertex), D3D11_BIND_VERTEX_BUFFER) ||
			!EnsureBuffer(m_pIndexBuffer, m_uIndexCapacity, stData.uTotalIndices, sizeof(std::uint32_t), D3D11_BIND_INDEX_BUFFER))
			return;

		D3D11_MAPPED_SUBRESOURCE stVertices{}, stIndices{}, stConstant{};
		if (FAILED(pContext->Map(m_pVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &stVertices)))
			return;
		if (FAILED(pContext->Map(m_pIndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &stIndices)))
		{
			pContext->Unmap(m_pVertexBuffer.Get(), 0);
			return;
		}
		auto* pVertexOut = static_cast<stVertex*>(stVertices.pData);
		auto* pIndexOut = static_cast<std::uint32_t*>(stIndices.pData);
		for (const clDrawList* pList : stData.aLists)
		{
			std::memcpy(pVertexOut, pList->GetVertices().data(), pList->GetVertices().size() * sizeof(stVertex));
			std::memcpy(pIndexOut, pList->GetIndices().data(), pList->GetIndices().size() * sizeof(std::uint32_t));
			pVertexOut += pList->GetVertices().size();
			pIndexOut += pList->GetIndices().size();
		}
		pContext->Unmap(m_pVertexBuffer.Get(), 0);
		pContext->Unmap(m_pIndexBuffer.Get(), 0);

		if (SUCCEEDED(pContext->Map(m_pConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &stConstant)))
		{
			const auto aProjection = stData.OrthoProjection();
			std::memcpy(stConstant.pData, aProjection.data(), sizeof(float) * 16);
			pContext->Unmap(m_pConstantBuffer.Get(), 0);
		}

		stStateBackup stBackup;
		BackupState(stBackup);

		const D3D11_VIEWPORT stViewport{ 0.f, 0.f, stData.vecDisplaySize.x, stData.vecDisplaySize.y, 0.f, 1.f };
		pContext->RSSetViewports(1, &stViewport);
		const UINT uStride = sizeof(stVertex), uOffset = 0;
		pContext->IASetInputLayout(m_pInputLayout.Get());
		pContext->IASetVertexBuffers(0, 1, m_pVertexBuffer.GetAddressOf(), &uStride, &uOffset);
		pContext->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pContext->VSSetShader(m_pVertexShader.Get(), nullptr, 0);
		pContext->VSSetConstantBuffers(0, 1, m_pConstantBuffer.GetAddressOf());
		pContext->PSSetShader(m_pPixelShader.Get(), nullptr, 0);
		pContext->PSSetSamplers(0, 1, m_pSampler.GetAddressOf());
		pContext->GSSetShader(nullptr, nullptr, 0);
		pContext->HSSetShader(nullptr, nullptr, 0);
		pContext->DSSetShader(nullptr, nullptr, 0);
		pContext->CSSetShader(nullptr, nullptr, 0);
		const float aBlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
		pContext->OMSetBlendState(m_pBlendState.Get(), aBlendFactor, 0xFFFFFFFF);
		pContext->OMSetDepthStencilState(m_pDepthDisabled.Get(), 0);
		pContext->RSSetState(m_pRasterizerState.Get());

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
				const D3D11_RECT stScissor{ static_cast<LONG>(rcClip.min.x - stData.vecDisplayPosition.x), static_cast<LONG>(rcClip.min.y - stData.vecDisplayPosition.y),
					static_cast<LONG>(rcClip.max.x - stData.vecDisplayPosition.x), static_cast<LONG>(rcClip.max.y - stData.vecDisplayPosition.y) };
				pContext->RSSetScissorRects(1, &stScissor);
				auto* pView = HandleToPointer<ID3D11ShaderResourceView>(rCommand.hTexture);
				pContext->PSSetShaderResources(0, 1, &pView);
				pContext->DrawIndexed(rCommand.uIndexCount, uGlobalIndex + rCommand.uIndexOffset, iGlobalVertex);
			}
			uGlobalIndex += static_cast<UINT>(pList->GetIndices().size());
			iGlobalVertex += static_cast<INT>(pList->GetVertices().size());
		}

		RestoreState(stBackup);
	}

	void clBackendDX11::RenderWorld(const clWorldDrawList& rList, const stWorldPass& stPass)
	{
		if (!m_pContext || !m_pWorldVertexShader || rList.IsEmpty())
			return;
		ID3D11DeviceContext* pContext = m_pContext.Get();

		std::size_t uVertices = 0, uIndices = 0;
		for (std::size_t i = 0; i < static_cast<std::size_t>(EDepthBatch::Count); ++i)
		{
			uVertices += rList.GetBatch(static_cast<EDepthBatch>(i)).aVertices.size();
			uIndices += rList.GetBatch(static_cast<EDepthBatch>(i)).aIndices.size();
		}
		if (!EnsureBuffer(m_pWorldVertexBuffer, m_uWorldVertexCapacity, uVertices, sizeof(stWorldVertex), D3D11_BIND_VERTEX_BUFFER) ||
			!EnsureBuffer(m_pWorldIndexBuffer, m_uWorldIndexCapacity, uIndices, sizeof(std::uint32_t), D3D11_BIND_INDEX_BUFFER))
			return;

		D3D11_MAPPED_SUBRESOURCE stVertices{}, stIndices{};
		if (FAILED(pContext->Map(m_pWorldVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &stVertices)))
			return;
		if (FAILED(pContext->Map(m_pWorldIndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &stIndices)))
		{
			pContext->Unmap(m_pWorldVertexBuffer.Get(), 0);
			return;
		}
		auto* pVertexOut = static_cast<stWorldVertex*>(stVertices.pData);
		auto* pIndexOut = static_cast<std::uint32_t*>(stIndices.pData);
		for (std::size_t i = 0; i < static_cast<std::size_t>(EDepthBatch::Count); ++i)
		{
			const stWorldBatch& rBatch = rList.GetBatch(static_cast<EDepthBatch>(i));
			std::memcpy(pVertexOut, rBatch.aVertices.data(), rBatch.aVertices.size() * sizeof(stWorldVertex));
			std::memcpy(pIndexOut, rBatch.aIndices.data(), rBatch.aIndices.size() * sizeof(std::uint32_t));
			pVertexOut += rBatch.aVertices.size();
			pIndexOut += rBatch.aIndices.size();
		}
		pContext->Unmap(m_pWorldVertexBuffer.Get(), 0);
		pContext->Unmap(m_pWorldIndexBuffer.Get(), 0);

		stStateBackup stBackup;
		BackupState(stBackup);

		if (m_pWorldRenderTarget)
		{
			pContext->OMSetRenderTargets(1, &m_pWorldRenderTarget, m_pWorldDepthStencil);
			ComPtr<ID3D11Resource> pResource;
			m_pWorldRenderTarget->GetResource(&pResource);
			ComPtr<ID3D11Texture2D> pTexture;
			if (SUCCEEDED(pResource.As(&pTexture)))
			{
				D3D11_TEXTURE2D_DESC stDesc{};
				pTexture->GetDesc(&stDesc);
				const D3D11_VIEWPORT stViewport{ 0.f, 0.f, static_cast<float>(stDesc.Width), static_cast<float>(stDesc.Height), 0.f, 1.f };
				pContext->RSSetViewports(1, &stViewport);
			}
		}

		const UINT uStride = sizeof(stWorldVertex), uOffset = 0;
		pContext->IASetInputLayout(m_pWorldInputLayout.Get());
		pContext->IASetVertexBuffers(0, 1, m_pWorldVertexBuffer.GetAddressOf(), &uStride, &uOffset);
		pContext->IASetIndexBuffer(m_pWorldIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pContext->VSSetShader(m_pWorldVertexShader.Get(), nullptr, 0);
		pContext->PSSetShader(m_pPixelShader.Get(), nullptr, 0);
		pContext->PSSetSamplers(0, 1, m_pSampler.GetAddressOf());
		pContext->GSSetShader(nullptr, nullptr, 0);
		pContext->HSSetShader(nullptr, nullptr, 0);
		pContext->DSSetShader(nullptr, nullptr, 0);
		const float aBlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
		pContext->OMSetBlendState(m_pBlendState.Get(), aBlendFactor, 0xFFFFFFFF);
		pContext->RSSetState(m_pWorldRasterizerState.Get());
		auto* pView = HandleToPointer<ID3D11ShaderResourceView>(rList.GetTexture());
		pContext->PSSetShaderResources(0, 1, &pView);

		UINT uIndexBase = 0;
		INT iVertexBase = 0;
		for (std::size_t i = 0; i < static_cast<std::size_t>(EDepthBatch::Count); ++i)
		{
			const stWorldBatch& rBatch = rList.GetBatch(static_cast<EDepthBatch>(i));
			if (!rBatch.aIndices.empty())
			{
				pContext->OMSetDepthStencilState(m_aWorldDepthStates[i * 2 + (stPass.bReversedZ ? 1 : 0)].Get(), 0);
				pContext->DrawIndexed(static_cast<UINT>(rBatch.aIndices.size()), uIndexBase, iVertexBase);
			}
			uIndexBase += static_cast<UINT>(rBatch.aIndices.size());
			iVertexBase += static_cast<INT>(rBatch.aVertices.size());
		}

		RestoreState(stBackup);
	}
}
