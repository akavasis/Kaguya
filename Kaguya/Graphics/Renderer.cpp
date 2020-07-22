#include "pch.h"
#include "Renderer.h"
#include "../Core/Application.h"
#include "../Core/Window.h"
#include "../Core/Time.h"

struct ShadowPassData
{
	const GpuBufferAllocator* pGpuBufferAllocator;
	const GpuTextureAllocator* pGpuTextureAllocator;

	const Buffer* pRenderPassConstantBuffer;

	RenderResourceHandle outputDepthTexture;
	DescriptorAllocation depthStencilView;
};

struct ForwardPassData
{
	const GpuBufferAllocator* pGpuBufferAllocator;
	const GpuTextureAllocator* pGpuTextureAllocator;

	const Texture* pShadowDepthBuffer;

	Texture* pFrameBuffer;
	Texture* pFrameDepthStencilBuffer;
	Buffer* pRenderPassConstantBuffer;
	DescriptorAllocation RenderTargetView;
	DescriptorAllocation DepthStencilView;
};

struct SkyboxPassData
{
	const GpuBufferAllocator* pGpuBufferAllocator;
	const GpuTextureAllocator* pGpuTextureAllocator;

	Texture* pFrameBuffer;
	Texture* pFrameDepthStencilBuffer;
	Buffer* pRenderPassConstantBuffer;
	DescriptorAllocation RenderTargetView;
	DescriptorAllocation DepthStencilView;
};

struct TonemapPassData
{
	TonemapData TonemapData;

	RenderResourceHandle FrameBufferHandle;
	Texture* pFrameBuffer;
	Descriptor FrameBufferSRV;
	Descriptor SwapChainRTV;
	Texture* pSwapChainTexture;
};

Renderer::Renderer(Window& Window)
	: m_RenderDevice(m_DXGIManager.QueryAdapter(API::API_D3D12).Get()),
	m_RenderGraph(m_RenderDevice),
	m_GpuBufferAllocator(50_MiB, 50_MiB, 64_KiB, &m_RenderDevice),
	m_GpuTextureAllocator(100, &m_RenderDevice)
{
	m_EventReceiver.Register(Window, [&](const Event& event)
	{
		Window::Event windowEvent;
		event.Read(windowEvent.type, windowEvent.data);
		switch (windowEvent.type)
		{
		case Window::Event::Type::Maximize:
		case Window::Event::Type::Resize:
			Resize(windowEvent.data.Width, windowEvent.data.Height);
			break;
		}
	});

	m_AspectRatio = static_cast<float>(Window.GetWindowWidth()) / static_cast<float>(Window.GetWindowHeight());

	Shaders::Register(&m_RenderDevice);
	RootSignatures::Register(&m_RenderDevice);
	GraphicsPSOs::Register(&m_RenderDevice);
	ComputePSOs::Register(&m_RenderDevice);

	// Create swap chain after command objects have been created
	m_pSwapChain = m_DXGIManager.CreateSwapChain(m_RenderDevice.GetGraphicsQueue()->GetD3DCommandQueue(), Window, RendererFormats::SwapChainBufferFormat, NumSwapChainBuffers);
	m_CurrentBackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	// Initialize Non-transient resources
	m_SwapChainRTVs = m_RenderDevice.GetDescriptorAllocator().AllocateRenderTargetDescriptors(NumSwapChainBuffers);
	for (UINT i = 0; i < NumSwapChainBuffers; ++i)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffer;
		ThrowCOMIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
		m_SwapChainTextureHandles[i] = m_RenderDevice.CreateSwapChainTexture(pBackBuffer, D3D12_RESOURCE_STATE_COMMON);
		m_pSwapChainTextures[i] = m_RenderDevice.GetSwapChainTexture(m_SwapChainTextureHandles[i]);
		m_RenderDevice.CreateRTVForSwapChainTexture(m_SwapChainTextureHandles[i], m_SwapChainRTVs[i]);
	}

	// Allocate upload context
	m_pUploadCommandContext = m_RenderDevice.AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT);

	Texture::Properties textureProp{};
	textureProp.Type = Resource::Type::Texture2D;
	textureProp.Format = RendererFormats::HDRBufferFormat;
	textureProp.Width = Window.GetWindowWidth();
	textureProp.Height = Window.GetWindowHeight();
	textureProp.DepthOrArraySize = 1;
	textureProp.MipLevels = 1;
	textureProp.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	textureProp.InitialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	textureProp.pOptimizedClearValue = &CD3DX12_CLEAR_VALUE(RendererFormats::HDRBufferFormat, DirectX::Colors::LightBlue);
	m_FrameBufferHandle = m_RenderDevice.CreateTexture(textureProp);
	m_pFrameBuffer = m_RenderDevice.GetTexture(m_FrameBufferHandle);

	textureProp.Format = RendererFormats::DepthStencilFormat;
	textureProp.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	textureProp.InitialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	textureProp.pOptimizedClearValue = &CD3DX12_CLEAR_VALUE(RendererFormats::DepthStencilFormat, 1.0f, 0);
	m_FrameDepthStencilBufferHandle = m_RenderDevice.CreateTexture(textureProp);
	m_pFrameDepthStencilBuffer = m_RenderDevice.GetTexture(m_FrameDepthStencilBufferHandle);

	Buffer::Properties bufferProp{};
	// + 1 for forward pass render constants
	bufferProp.SizeInBytes = (NUM_CASCADES + 1) * Math::AlignUp<UINT64>(sizeof(RenderPassConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	bufferProp.Stride = Math::AlignUp<UINT64>(sizeof(RenderPassConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	m_RenderPassConstantBufferHandle = m_RenderDevice.CreateBuffer(bufferProp, CPUAccessibleHeapType::Upload, nullptr);
	m_pRenderPassConstantBuffer = m_RenderDevice.GetBuffer(m_RenderPassConstantBufferHandle);
	m_pRenderPassConstantBuffer->Map();

	auto pShadowPass = m_RenderGraph.AddRenderPass<RenderPassType::Graphics, ShadowPassData>(
		[&](ShadowPassData& Data, RenderDevice& RenderDevice)
	{
		Data.pGpuBufferAllocator = &m_GpuBufferAllocator;
		Data.pGpuTextureAllocator = &m_GpuTextureAllocator;

		Data.pRenderPassConstantBuffer = RenderDevice.GetBuffer(m_RenderPassConstantBufferHandle);

		Texture::Properties textureProp{};
		textureProp.Type = Resource::Type::Texture2D;
		textureProp.Format = DXGI_FORMAT_R32_TYPELESS;
		textureProp.Width = textureProp.Height = Constants::SunShadowMapResolution;
		textureProp.DepthOrArraySize = NUM_CASCADES;
		textureProp.MipLevels = 1;
		textureProp.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		textureProp.IsCubemap = false;
		textureProp.pOptimizedClearValue = &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);
		textureProp.InitialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		Data.outputDepthTexture = RenderDevice.CreateTexture(textureProp);
		Data.depthStencilView = RenderDevice.GetDescriptorAllocator().AllocateDepthStencilDescriptors(NUM_CASCADES);
		for (UINT i = 0; i < NUM_CASCADES; ++i)
		{
			Descriptor descriptor = Data.depthStencilView[i];
			RenderDevice.CreateDSV(Data.outputDepthTexture, descriptor, i, {}, 1);
		}

		return [](const ShadowPassData& Data, const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* pRenderCommandContext)
		{
			std::vector<const Model*> visibleModelsIndices;
			std::unordered_map<const Model*, std::vector<UINT>> visibleMeshesIndices;
			visibleModelsIndices.resize(Scene.Models.size());
			for (const auto& model : Scene.Models)
			{
				visibleMeshesIndices[&model].resize(model.Meshes.size());
			}

			// Begin rendering
			PIXMarker(pRenderCommandContext->GetD3DCommandList(), L"Scene Cascade Shadow Map Render");

			pRenderCommandContext->TransitionBarrier(RenderGraphRegistry.GetTexture(Data.outputDepthTexture), D3D12_RESOURCE_STATE_DEPTH_WRITE);

			pRenderCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pRenderCommandContext->SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::Shadow));
			pRenderCommandContext->SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::Shadow));

			Data.pGpuBufferAllocator->Bind(pRenderCommandContext);

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.Width = vp.Height = Constants::SunShadowMapResolution;

			D3D12_RECT sr;
			sr.left = sr.top = 0;
			sr.right = sr.bottom = Constants::SunShadowMapResolution;

			pRenderCommandContext->SetViewports(1, &vp);
			pRenderCommandContext->SetScissorRects(1, &sr);

			for (UINT cascadeIndex = 0; cascadeIndex < NUM_CASCADES; ++cascadeIndex)
			{
				wchar_t msg[32]; swprintf(msg, 32, L"Cascade Shadow Map %u", cascadeIndex);
				PIXMarker(pRenderCommandContext->GetD3DCommandList(), msg);

				const OrthographicCamera& CascadeCamera = Scene.CascadeCameras[cascadeIndex];
				pRenderCommandContext->SetGraphicsRootConstantBufferView(RootParameters::StandardShaderLayout::RenderPassDataCB, Data.pRenderPassConstantBuffer->GetGpuVirtualAddressAt(cascadeIndex));

				Descriptor descriptor = Data.depthStencilView[cascadeIndex];
				pRenderCommandContext->SetRenderTargets(0, Descriptor(), FALSE, descriptor);
				pRenderCommandContext->ClearDepthStencil(descriptor, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

				const UINT numVisibleModels = CullModelsOrthographic(CascadeCamera, true, Scene.Models, visibleModelsIndices);
				for (UINT i = 0; i < numVisibleModels; ++i)
				{
					auto& pModel = visibleModelsIndices[i];
					const UINT numVisibleMeshes = CullMeshesOrthographic(CascadeCamera, true, pModel->Meshes, visibleMeshesIndices[pModel]);

					for (UINT j = 0; j < numVisibleMeshes; ++j)
					{
						const UINT meshIndex = visibleMeshesIndices[pModel][j];
						auto& mesh = pModel->Meshes[meshIndex];

						pRenderCommandContext->SetGraphicsRootConstantBufferView(RootParameters::StandardShaderLayout::ConstantDataCB, Data.pGpuBufferAllocator->GetConstantBuffer()->GetGpuVirtualAddressAt(mesh.ObjectConstantsIndex));
						pRenderCommandContext->DrawIndexedInstanced(mesh.IndexCount, 1, mesh.StartIndexLocation, mesh.BaseVertexLocation, 0);
					}
				}
			}

			pRenderCommandContext->TransitionBarrier(RenderGraphRegistry.GetTexture(Data.outputDepthTexture), D3D12_RESOURCE_STATE_DEPTH_READ);
		};
	});

	auto pForwardPass = m_RenderGraph.AddRenderPass<RenderPassType::Graphics, ForwardPassData>(
		[&](ForwardPassData& Data, RenderDevice& RenderDevice)
	{
		Data.pGpuBufferAllocator = &m_GpuBufferAllocator;
		Data.pGpuTextureAllocator = &m_GpuTextureAllocator;

		Data.pFrameBuffer = RenderDevice.GetTexture(m_FrameBufferHandle);
		Data.pFrameDepthStencilBuffer = RenderDevice.GetTexture(m_FrameDepthStencilBufferHandle);
		Data.pRenderPassConstantBuffer = RenderDevice.GetBuffer(m_RenderPassConstantBufferHandle);

		Data.RenderTargetView = RenderDevice.GetDescriptorAllocator().AllocateRenderTargetDescriptors(1);
		Data.DepthStencilView = RenderDevice.GetDescriptorAllocator().AllocateDepthStencilDescriptors(1);

		RenderDevice.CreateRTV(m_FrameBufferHandle, Data.RenderTargetView[0], {}, {}, {});
		RenderDevice.CreateDSV(m_FrameDepthStencilBufferHandle, Data.DepthStencilView[0], {}, {}, {});

		return [](const ForwardPassData& Data, const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* pRenderCommandContext)
		{
			std::vector<const Model*> visibleModelsIndices;
			std::unordered_map<const Model*, std::vector<UINT>> visibleMeshesIndices;
			visibleModelsIndices.resize(Scene.Models.size());
			for (const auto& model : Scene.Models)
			{
				visibleMeshesIndices[&model].resize(model.Meshes.size());
			}

			PIXMarker(pRenderCommandContext->GetD3DCommandList(), L"Scene Render");

			pRenderCommandContext->TransitionBarrier(Data.pFrameBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
			pRenderCommandContext->TransitionBarrier(Data.pFrameDepthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);

			pRenderCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pRenderCommandContext->SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::PBR));
			pRenderCommandContext->SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::PBR));

			Data.pGpuBufferAllocator->Bind(pRenderCommandContext);
			Data.pGpuTextureAllocator->Bind(RootParameters::PBR::MaterialTextureIndicesSBuffer, RootParameters::PBR::MaterialTexturePropertiesSBuffer, pRenderCommandContext);
			pRenderCommandContext->SetGraphicsRootConstantBufferView(RootParameters::StandardShaderLayout::RenderPassDataCB + RootParameters::PBR::NumRootParameters, Data.pRenderPassConstantBuffer->GetGpuVirtualAddressAt(NUM_CASCADES));
			pRenderCommandContext->SetGraphicsRootDescriptorTable(RootParameters::StandardShaderLayout::DescriptorTables + RootParameters::PBR::NumRootParameters, RenderGraphRegistry.GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart());

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.Width = Data.pFrameBuffer->Width;
			vp.Height = Data.pFrameBuffer->Height;

			D3D12_RECT sr;
			sr.left = sr.top = 0;
			sr.right = Data.pFrameBuffer->Width;
			sr.bottom = Data.pFrameBuffer->Height;

			pRenderCommandContext->SetViewports(1, &vp);
			pRenderCommandContext->SetScissorRects(1, &sr);

			pRenderCommandContext->SetRenderTargets(1, Data.RenderTargetView[0], TRUE, Data.DepthStencilView[0]);
			pRenderCommandContext->ClearRenderTarget(Data.RenderTargetView[0], DirectX::Colors::LightBlue, 0, nullptr);
			pRenderCommandContext->ClearDepthStencil(Data.DepthStencilView[0], D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

			const UINT numVisibleModels = CullModels(&Scene.Camera, Scene.Models, visibleModelsIndices);
			for (UINT i = 0; i < numVisibleModels; ++i)
			{
				auto& pModel = visibleModelsIndices[i];
				const UINT numVisibleMeshes = CullMeshes(&Scene.Camera, pModel->Meshes, visibleMeshesIndices[pModel]);

				for (UINT j = 0; j < numVisibleMeshes; ++j)
				{
					const UINT meshIndex = visibleMeshesIndices[pModel][j];
					auto& mesh = pModel->Meshes[meshIndex];

					pRenderCommandContext->SetGraphicsRootConstantBufferView(RootParameters::StandardShaderLayout::ConstantDataCB + RootParameters::PBR::NumRootParameters, Data.pGpuBufferAllocator->GetConstantBuffer()->GetGpuVirtualAddressAt(mesh.ObjectConstantsIndex));
					pRenderCommandContext->DrawIndexedInstanced(mesh.IndexCount, 1, mesh.StartIndexLocation, mesh.BaseVertexLocation, 0);
				}
			}

			pRenderCommandContext->TransitionBarrier(Data.pFrameBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			pRenderCommandContext->TransitionBarrier(Data.pFrameDepthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
		};
	});

	auto pSkyboxPass = m_RenderGraph.AddRenderPass<RenderPassType::Graphics, SkyboxPassData>(
		[&](SkyboxPassData& Data, RenderDevice& RenderDevice)
	{
		Data.pGpuBufferAllocator = &m_GpuBufferAllocator;
		Data.pGpuTextureAllocator = &m_GpuTextureAllocator;

		Data.pFrameBuffer = pForwardPass->GetData().pFrameBuffer;
		Data.pFrameDepthStencilBuffer = pForwardPass->GetData().pFrameDepthStencilBuffer;
		Data.pRenderPassConstantBuffer = pForwardPass->GetData().pRenderPassConstantBuffer;

		Data.RenderTargetView = pForwardPass->GetData().RenderTargetView;
		Data.DepthStencilView = pForwardPass->GetData().DepthStencilView;

		return [](const SkyboxPassData& Data, const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* pRenderCommandContext)
		{
			PIXMarker(pRenderCommandContext->GetD3DCommandList(), L"Skybox Render");

			pRenderCommandContext->TransitionBarrier(Data.pFrameBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
			pRenderCommandContext->TransitionBarrier(Data.pFrameDepthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);

			pRenderCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pRenderCommandContext->SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::Skybox));
			pRenderCommandContext->SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::Skybox));

			Data.pGpuBufferAllocator->Bind(pRenderCommandContext);
			pRenderCommandContext->SetGraphicsRootConstantBufferView(RootParameters::StandardShaderLayout::RenderPassDataCB, Data.pRenderPassConstantBuffer->GetGpuVirtualAddressAt(NUM_CASCADES));
			pRenderCommandContext->SetGraphicsRootDescriptorTable(RootParameters::StandardShaderLayout::DescriptorTables, RenderGraphRegistry.GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart());

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.Width = Data.pFrameBuffer->Width;
			vp.Height = Data.pFrameBuffer->Height;

			D3D12_RECT sr;
			sr.left = sr.top = 0;
			sr.right = Data.pFrameBuffer->Width;
			sr.bottom = Data.pFrameBuffer->Height;

			pRenderCommandContext->SetViewports(1, &vp);
			pRenderCommandContext->SetScissorRects(1, &sr);

			pRenderCommandContext->SetRenderTargets(1, Data.RenderTargetView[0], TRUE, Data.DepthStencilView[0]);

			pRenderCommandContext->DrawIndexedInstanced(Scene.Skybox.Mesh.IndexCount, 1, Scene.Skybox.Mesh.StartIndexLocation, Scene.Skybox.Mesh.BaseVertexLocation, 0);

			pRenderCommandContext->TransitionBarrier(Data.pFrameBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			pRenderCommandContext->TransitionBarrier(Data.pFrameDepthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
		};
	});

	auto pTonemapPass = m_RenderGraph.AddRenderPass<RenderPassType::Graphics, TonemapPassData>(
		[&](TonemapPassData& Data, RenderDevice& RenderDevice)
	{
		Data.FrameBufferHandle = m_FrameBufferHandle;
		Data.pFrameBuffer = pSkyboxPass->GetData().pFrameBuffer;
		Data.SwapChainRTV = m_SwapChainRTVs[m_CurrentBackBufferIndex];
		Data.pSwapChainTexture = m_pSwapChainTextures[m_CurrentBackBufferIndex];

		return [](const TonemapPassData& Data, const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* pRenderCommandContext)
		{
			pRenderCommandContext->TransitionBarrier(Data.pSwapChainTexture, D3D12_RESOURCE_STATE_RENDER_TARGET);

			pRenderCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pRenderCommandContext->SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::PostProcess_Tonemapping));
			pRenderCommandContext->SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::PostProcess_Tonemapping));

			pRenderCommandContext->SetGraphicsRoot32BitConstants(RootParameters::StandardShaderLayout::ConstantDataCB, sizeof(Data.TonemapData) / 4, &Data.TonemapData, 0);
			pRenderCommandContext->SetGraphicsRootDescriptorTable(RootParameters::StandardShaderLayout::DescriptorTables, RenderGraphRegistry.GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart());

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.Width = Data.pFrameBuffer->Width;
			vp.Height = Data.pFrameBuffer->Height;

			D3D12_RECT sr;
			sr.left = sr.top = 0;
			sr.right = Data.pFrameBuffer->Width;
			sr.bottom = Data.pFrameBuffer->Height;

			pRenderCommandContext->SetViewports(1, &vp);
			pRenderCommandContext->SetScissorRects(1, &sr);

			pRenderCommandContext->SetRenderTargets(1, Data.SwapChainRTV, TRUE, Descriptor());
			pRenderCommandContext->DrawInstanced(3, 1, 0, 0);

			pRenderCommandContext->TransitionBarrier(Data.pSwapChainTexture, D3D12_RESOURCE_STATE_PRESENT);
		};
	});

	m_RenderGraph.SetExecutionPolicy(RenderGraph::ExecutionPolicy::Parallel);
	m_RenderGraph.Setup();
}

Renderer::~Renderer()
{
	m_RenderDevice.GetGraphicsQueue()->WaitForIdle();
	m_RenderDevice.GetCopyQueue()->WaitForIdle();
	m_RenderDevice.GetComputeQueue()->WaitForIdle();
}

void Renderer::Update(const Time& Time)
{
	++m_Statistics.TotalFrameCount;
	++m_Statistics.FrameCount;
	if (Time.TotalTime() - m_Statistics.TimeElapsed >= 1.0)
	{
		m_Statistics.FPS = static_cast<DOUBLE>(m_Statistics.FrameCount);
		m_Statistics.FPMS = 1000.0 / m_Statistics.FPS;
		m_Statistics.FrameCount = 0;
		m_Statistics.TimeElapsed += 1.0;
	}
}

void Renderer::Render(Scene& Scene)
{
	PIXCapture();
	auto shadowPass = m_RenderGraph.GetRenderPass<RenderPassType::Graphics, ShadowPassData>();
	auto tonemapPass = m_RenderGraph.GetRenderPass<RenderPassType::Graphics, TonemapPassData>();

	if (!m_Status.IsSceneStaged)
	{
		m_Status.IsSceneStaged = true;

		m_GpuBufferAllocator.Stage(Scene, m_pUploadCommandContext);
		m_GpuBufferAllocator.Bind(m_pUploadCommandContext);
		m_GpuTextureAllocator.Stage(Scene, m_pUploadCommandContext);
		m_RenderDevice.ExecuteRenderCommandContexts(1, &m_pUploadCommandContext);

		const std::size_t numSRVsToAllocate = m_GpuTextureAllocator.GetNumTextures() + 2; // 1+ Shadow map, 1+ FrameBuffer
		m_GpuDescriptorIndices.TextureShaderResourceViews = m_RenderDevice.GetDescriptorAllocator().GetUniversalGpuDescriptorHeap()->AllocateSRDescriptors(numSRVsToAllocate).value();

		for (size_t i = 0; i < GpuTextureAllocator::NumAssetTextures; ++i)
		{
			auto handle = m_GpuTextureAllocator.AssetTextures[i];
			m_RenderDevice.CreateSRV(handle, m_GpuDescriptorIndices.TextureShaderResourceViews[i]);
		}

		for (auto iter = m_GpuTextureAllocator.TextureStorage.TextureIndices.begin(); iter != m_GpuTextureAllocator.TextureStorage.TextureIndices.end(); ++iter)
		{
			m_RenderDevice.CreateSRV(m_GpuTextureAllocator.TextureStorage.TextureHandles[iter->first], m_GpuDescriptorIndices.TextureShaderResourceViews[iter->second]);
		}

		int shadowMapIndex = numSRVsToAllocate - 2;
		m_RenderDevice.CreateSRV(shadowPass->GetData().outputDepthTexture, m_GpuDescriptorIndices.TextureShaderResourceViews[shadowMapIndex]);

		tonemapPass->GetData().TonemapData.InputMapIndex = shadowMapIndex + 1;
		m_RenderDevice.CreateSRV(m_FrameBufferHandle, m_GpuDescriptorIndices.TextureShaderResourceViews[shadowMapIndex + 1]);

		m_GpuDescriptorIndices.ShadowMapIndex = shadowMapIndex;
		m_GpuDescriptorIndices.FrameBufferIndex = shadowMapIndex + 1;
	}

	m_CurrentBackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	tonemapPass->GetData().SwapChainRTV = m_SwapChainRTVs[m_CurrentBackBufferIndex];
	tonemapPass->GetData().pSwapChainTexture = m_pSwapChainTextures[m_CurrentBackBufferIndex];

	m_GpuBufferAllocator.Update(Scene);
	m_GpuTextureAllocator.Update(Scene);
	Scene.Camera.SetAspectRatio(m_AspectRatio);
	Scene.CascadeCameras = Scene.Sun.GenerateCascades(Scene.Camera, Constants::SunShadowMapResolution);

	// Update shadow render pass cbuffer
	for (UINT cascadeIndex = 0; cascadeIndex < NUM_CASCADES; ++cascadeIndex)
	{
		RenderPassConstants cascadeShadowPass;
		DirectX::XMStoreFloat4x4(&cascadeShadowPass.ViewProjection, DirectX::XMMatrixTranspose(Scene.CascadeCameras[cascadeIndex].ViewProjectionMatrix()));

		m_pRenderPassConstantBuffer->Update<RenderPassConstants>(cascadeIndex, cascadeShadowPass);
	}

	// Update render pass cbuffer
	RenderPassConstants renderPassCPU;
	XMStoreFloat4x4(&renderPassCPU.ViewProjection, XMMatrixTranspose(Scene.Camera.ViewProjectionMatrix()));
	renderPassCPU.EyePosition = Scene.Camera.GetTransform().Position;

	renderPassCPU.Sun = Scene.Sun;
	renderPassCPU.SunShadowMapIndex = m_GpuDescriptorIndices.ShadowMapIndex;
	renderPassCPU.BRDFLUTMapIndex = GpuTextureAllocator::BRDFLUT;
	renderPassCPU.IrradianceCubemapIndex = GpuTextureAllocator::SkyboxIrradianceCubemap;
	renderPassCPU.PrefilteredRadianceCubemapIndex = GpuTextureAllocator::SkyboxPrefilteredCubemap;

	const INT forwardRenderPassConstantsIndex = NUM_CASCADES; // 0 - NUM_CASCADES reserved for shadow map data
	m_pRenderPassConstantBuffer->Update<RenderPassConstants>(forwardRenderPassConstantsIndex, renderPassCPU);

	m_RenderGraph.Execute(Scene);
	m_RenderGraph.ThreadBarrier();
	m_RenderGraph.ExecuteCommandContexts();

	UINT syncInterval = m_Setting.VSync ? 1u : 0u;
	UINT presentFlags = (m_DXGIManager.TearingSupport() && !m_Setting.VSync) ? DXGI_PRESENT_ALLOW_TEARING : 0u;
	DXGI_PRESENT_PARAMETERS presentParameters = { 0u, NULL, NULL, NULL };
	HRESULT hr = m_pSwapChain->Present1(syncInterval, presentFlags, &presentParameters);
	if (hr == DXGI_ERROR_DEVICE_REMOVED)
	{
		CORE_ERROR("DXGI_ERROR_DEVICE_REMOVED");
		Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData> pDred;
		ThrowCOMIfFailed(m_RenderDevice.GetDevice().GetD3DDevice()->QueryInterface(IID_PPV_ARGS(&pDred)));
		D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DredAutoBreadcrumbsOutput;
		D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
		ThrowCOMIfFailed(pDred->GetAutoBreadcrumbsOutput(&DredAutoBreadcrumbsOutput));
		ThrowCOMIfFailed(pDred->GetPageFaultAllocationOutput(&DredPageFaultOutput));
	}
}

void Renderer::Resize(UINT Width, UINT Height)
{
	m_RenderDevice.GetGraphicsQueue()->WaitForIdle();
	{
		m_AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);

		// Release resources before resize swap chain
		for (UINT i = 0u; i < NumSwapChainBuffers; ++i)
			m_RenderDevice.DestroySwapChainTexture(&m_SwapChainTextureHandles[i]);

		// Resize backbuffer
		UINT nodes[NumSwapChainBuffers];
		IUnknown* commandQueues[NumSwapChainBuffers];
		for (UINT i = 0; i < NumSwapChainBuffers; ++i)
		{
			nodes[i] = NodeMask;
			commandQueues[i] = m_RenderDevice.GetGraphicsQueue()->GetD3DCommandQueue();
		}
		UINT swapChainFlags = m_DXGIManager.TearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		ThrowCOMIfFailed(m_pSwapChain->ResizeBuffers1(NumSwapChainBuffers, Width, Height, RendererFormats::SwapChainBufferFormat, swapChainFlags, nodes, commandQueues));

		// Recreate descriptors
		for (UINT i = 0; i < NumSwapChainBuffers; ++i)
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffer;
			ThrowCOMIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
			m_RenderDevice.ReplaceSwapChainTexture(pBackBuffer, D3D12_RESOURCE_STATE_COMMON, m_SwapChainTextureHandles[i]);
			m_RenderDevice.CreateRTVForSwapChainTexture(m_SwapChainTextureHandles[i], m_SwapChainRTVs[i]);
		}
		// Reset back buffer index
		m_CurrentBackBufferIndex = 0;

		// Recreate frame buffer
		Texture::Properties textureProp{};
		textureProp.Type = Resource::Type::Texture2D;
		textureProp.Format = RendererFormats::HDRBufferFormat;
		textureProp.Width = Width;
		textureProp.Height = Height;
		textureProp.DepthOrArraySize = 1;
		textureProp.MipLevels = 1;
		textureProp.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		textureProp.InitialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		textureProp.pOptimizedClearValue = &CD3DX12_CLEAR_VALUE(RendererFormats::HDRBufferFormat, DirectX::Colors::LightBlue);
		m_RenderDevice.ReplaceTexture(textureProp, m_FrameBufferHandle);

		textureProp.Format = RendererFormats::DepthStencilFormat;
		textureProp.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		textureProp.InitialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		textureProp.pOptimizedClearValue = &CD3DX12_CLEAR_VALUE(RendererFormats::DepthStencilFormat, 1.0f, 0);
		m_RenderDevice.ReplaceTexture(textureProp, m_FrameDepthStencilBufferHandle);

		// Create descriptor
		auto pForwardPass = m_RenderGraph.GetRenderPass<RenderPassType::Graphics, ForwardPassData>();
		m_RenderDevice.CreateRTV(m_FrameBufferHandle, pForwardPass->GetData().RenderTargetView[0], {}, {}, {});
		m_RenderDevice.CreateDSV(m_FrameDepthStencilBufferHandle, pForwardPass->GetData().DepthStencilView[0], {}, {}, {});

		m_RenderDevice.CreateSRV(m_FrameBufferHandle, m_GpuDescriptorIndices.TextureShaderResourceViews[m_GpuDescriptorIndices.FrameBufferIndex]);
	}
	m_RenderDevice.GetGraphicsQueue()->WaitForIdle();
}