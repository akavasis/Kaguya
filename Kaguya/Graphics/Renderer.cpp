#include "pch.h"
#include "Renderer.h"
#include "../Core/Application.h"
#include "../Core/Window.h"
#include "../Core/Time.h"

Renderer::Renderer(Application& RefApplication, Window& RefWindow)
	: m_RefWindow(RefWindow),
	m_RenderDevice(m_DXGIManager.QueryAdapter(API::API_D3D12).Get()),
	m_RenderGraph(m_RenderDevice),
	m_GpuBufferAllocator(256_MiB, 256_MiB, 256_MiB, m_RenderDevice),
	m_GpuTextureAllocator(256_MiB, m_RenderDevice)
{
	m_EventReceiver.Register(m_RefWindow, [&](const Event& event)
	{
		Window::Event windowEvent;
		event.Read(windowEvent.type, windowEvent.data);
		switch (windowEvent.type)
		{
		case Window::Event::Type::Maximize:
		case Window::Event::Type::Resize:
			Resize();
			break;
		}
	});

	m_AspectRatio = static_cast<float>(m_RefWindow.GetWindowWidth()) / static_cast<float>(m_RefWindow.GetWindowHeight());

	Shaders::Register(&m_RenderDevice);
	RootSignatures::Register(&m_RenderDevice);
	GraphicsPSOs::Register(&m_RenderDevice);
	ComputePSOs::Register(&m_RenderDevice);

	// Create swap chain after command objects have been created
	m_pSwapChain = m_DXGIManager.CreateSwapChain(m_RenderDevice.GetGraphicsQueue()->GetD3DCommandQueue(), m_RefWindow, RendererFormats::SwapChainBufferFormat, SwapChainBufferCount);

	// Initialize Non-transient resources
	for (UINT i = 0; i < SwapChainBufferCount; ++i)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffer;
		ThrowCOMIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
		m_BackBufferTexture[i] = Texture(pBackBuffer);
		m_RenderDevice.GetGlobalResourceStateTracker().AddResourceState(m_BackBufferTexture[i].GetD3DResource(), D3D12_RESOURCE_STATE_COMMON);
	}

	Texture::Properties textureProp{};
	textureProp.Type = Resource::Type::Texture2D;
	textureProp.Format = RendererFormats::HDRBufferFormat;
	textureProp.Width = m_RefWindow.GetWindowWidth();
	textureProp.Height = m_RefWindow.GetWindowHeight();
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

	struct SceneStagingData
	{
		GpuBufferAllocator* pGpuBufferAllocator;
		GpuTextureAllocator* pGpuTextureAllocator;
	};

	auto pSceneStagingPass = m_RenderGraph.AddRenderPass<RenderPassType::Graphics, SceneStagingData>(
		[&](SceneStagingData& Data, RenderDevice& RenderDevice)
	{
		Data.pGpuBufferAllocator = &m_GpuBufferAllocator;
		Data.pGpuTextureAllocator = &m_GpuTextureAllocator;

		return [](const SceneStagingData& Data, Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* pRenderCommandContext)
		{
			Data.pGpuBufferAllocator->Stage(Scene, pRenderCommandContext);
			Data.pGpuBufferAllocator->Bind(pRenderCommandContext);
			Data.pGpuTextureAllocator->Stage(Scene, pRenderCommandContext);

			RenderGraphRegistry.GetRenderPass<RenderPassType::Graphics, SceneStagingData>()->Enabled = false;
		};
	});

	struct ShadowPassData
	{
		const GpuBufferAllocator* pGpuBufferAllocator;
		const GpuTextureAllocator* pGpuTextureAllocator;

		Buffer* pRenderPassConstantBuffer;

		RenderResourceHandle outputDepthTexture;
		Descriptor depthStencilView;
	};

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
		Data.depthStencilView = RenderDevice.GetDevice().GetDescriptorAllocator<D3D12_DEPTH_STENCIL_VIEW_DESC>().Allocate(NUM_CASCADES);
		for (UINT i = 0; i < NUM_CASCADES; ++i)
		{
			RenderDevice.CreateDSV(Data.outputDepthTexture, Data.depthStencilView[i], i, {}, 1);
		}

		return [](const ShadowPassData& Data, Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* pRenderCommandContext)
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

			pRenderCommandContext->SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::Shadow));
			pRenderCommandContext->SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::Shadow));

			Data.pGpuBufferAllocator->Bind(pRenderCommandContext);
			pRenderCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pRenderCommandContext->TransitionBarrier(RenderGraphRegistry.GetTexture(Data.outputDepthTexture), D3D12_RESOURCE_STATE_DEPTH_WRITE);

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
				pRenderCommandContext->SetGraphicsRootCBV(1, Data.pRenderPassConstantBuffer->GetGpuVAAt(cascadeIndex));

				pRenderCommandContext->SetRenderTargets(0, 0, Descriptor(), FALSE, cascadeIndex, Data.depthStencilView);
				pRenderCommandContext->ClearDepthStencil(cascadeIndex, Data.depthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

				const UINT numVisibleModels = CullModelsOrthographic(CascadeCamera, true, Scene.Models, visibleModelsIndices);
				for (UINT i = 0; i < numVisibleModels; ++i)
				{
					auto& pModel = visibleModelsIndices[i];
					const UINT numVisibleMeshes = CullMeshesOrthographic(CascadeCamera, true, pModel->Meshes, visibleMeshesIndices[pModel]);

					for (UINT j = 0; j < numVisibleMeshes; ++j)
					{
						const UINT meshIndex = visibleMeshesIndices[pModel][j];
						auto& mesh = pModel->Meshes[meshIndex];

						pRenderCommandContext->SetGraphicsRootCBV(0, Data.pGpuBufferAllocator->GetConstantBuffer()->GetGpuVAAt(mesh.ConstantBufferIndex));
						pRenderCommandContext->DrawIndexedInstanced(mesh.IndexCount, 1, mesh.StartIndexLocation, mesh.BaseVertexLocation, 0);
					}
				}
			}
			pRenderCommandContext->TransitionBarrier(RenderGraphRegistry.GetTexture(Data.outputDepthTexture), D3D12_RESOURCE_STATE_DEPTH_READ);
		};
	});

	struct ForwardPassData
	{
		const GpuBufferAllocator* pGpuBufferAllocator;
		const GpuTextureAllocator* pGpuTextureAllocator;

		Texture* pShadowDepthBuffer;
		Texture* pFrameBuffer;
		Texture* pFrameDepthStencilBuffer;
		Buffer* pRenderPassConstantBuffer;
		Descriptor RenderTargetView;
		Descriptor DepthStencilView;
	};

	auto pForwardPass = m_RenderGraph.AddRenderPass<RenderPassType::Graphics, ForwardPassData>(
		[&](ForwardPassData& Data, RenderDevice& RenderDevice)
	{
		Data.pGpuBufferAllocator = &m_GpuBufferAllocator;
		Data.pGpuTextureAllocator = &m_GpuTextureAllocator;

		Data.pFrameBuffer = RenderDevice.GetTexture(m_FrameBufferHandle);
		Data.pFrameDepthStencilBuffer = RenderDevice.GetTexture(m_FrameDepthStencilBufferHandle);
		Data.pRenderPassConstantBuffer = RenderDevice.GetBuffer(m_RenderPassConstantBufferHandle);

		Data.RenderTargetView = RenderDevice.GetDevice().GetDescriptorAllocator<D3D12_RENDER_TARGET_VIEW_DESC>().Allocate(1);
		Data.DepthStencilView = RenderDevice.GetDevice().GetDescriptorAllocator<D3D12_DEPTH_STENCIL_VIEW_DESC>().Allocate(1);

		RenderDevice.CreateRTV(m_FrameBufferHandle, Data.RenderTargetView[0], {}, {}, {});
		RenderDevice.CreateDSV(m_FrameDepthStencilBufferHandle, Data.DepthStencilView[0], {}, {}, {});

		return [=](const ForwardPassData& Data, Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* pRenderCommandContext)
		{
			std::vector<const Model*> visibleModelsIndices;
			std::unordered_map<const Model*, std::vector<UINT>> visibleMeshesIndices;
			visibleModelsIndices.resize(Scene.Models.size());
			for (const auto& model : Scene.Models)
			{
				visibleMeshesIndices[&model].resize(model.Meshes.size());
			}

			PIXMarker(pRenderCommandContext->GetD3DCommandList(), L"Scene Render");

			pRenderCommandContext->SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::PBR));
			pRenderCommandContext->SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::PBR));

			Data.pGpuBufferAllocator->Bind(pRenderCommandContext);
			pRenderCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

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

			pRenderCommandContext->SetRenderTargets(0, 1, Data.RenderTargetView, TRUE, 0, Data.DepthStencilView);
			pRenderCommandContext->ClearRenderTarget(0, Data.RenderTargetView, DirectX::Colors::LightBlue, 0, nullptr);
			pRenderCommandContext->ClearDepthStencil(0, Data.DepthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

			/*pRenderCommandContext->SetSRV(RootParameters::PBR::PBRRootParameter_ShadowMapAndEnvironmentMapsSRV, 0, Data.pInputDepthBuffer->Type(), &Data.InputDepthBufferSRVDesc);
			pRenderCommandContext->SetSRV(RootParameters::PBR::PBRRootParameter_ShadowMapAndEnvironmentMapsSRV, 1, m_Skybox.IrradianceTexture, &m_Skybox.IrradianceSRVDesc);
			pRenderCommandContext->SetSRV(RootParameters::PBR::PBRRootParameter_ShadowMapAndEnvironmentMapsSRV, 2, m_Skybox.PrefilterdTexture, &m_Skybox.PrefilterdSRVDesc);
			pRenderCommandContext->SetSRV(RootParameters::PBR::PBRRootParameter_ShadowMapAndEnvironmentMapsSRV, 3, m_Skybox.BRDFLUT, &m_Skybox.BRDFLUTSRVDesc);
			pRenderCommandContext->SetGraphicsRootCBV(RootParameters::PBR::PBRRootParameter_RenderPassCBuffer, m_pRenderPassCBs->BufferLocationAt(0));*/

			const UINT numVisibleModels = CullModels(&Scene.Camera, Scene.Models, visibleModelsIndices);
			for (UINT i = 0; i < numVisibleModels; ++i)
			{
				auto& pModel = visibleModelsIndices[i];
				const UINT numVisibleMeshes = CullMeshes(&Scene.Camera, pModel->Meshes, visibleMeshesIndices[pModel]);

				for (UINT j = 0; j < numVisibleMeshes; ++j)
				{
					const UINT meshIndex = visibleMeshesIndices[pModel][j];
					auto& mesh = pModel->Meshes[meshIndex];

					pRenderCommandContext->SetGraphicsRootCBV(0, Data.pGpuBufferAllocator->GetConstantBuffer()->GetGpuVAAt(mesh.ConstantBufferIndex));
					pRenderCommandContext->DrawIndexedInstanced(mesh.IndexCount, 1, mesh.StartIndexLocation, mesh.BaseVertexLocation, 0);
				}
			}
		};
	});

	m_RenderGraph.Setup();
}

Renderer::~Renderer()
{
}

void Renderer::TEST(Scene& Scene)
{
	PIXCapture();
	auto pRenderCommandContext = m_RenderDevice.AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
	m_GpuBufferAllocator.Stage(Scene, pRenderCommandContext);
	m_GpuBufferAllocator.Bind(pRenderCommandContext);
	m_GpuBufferAllocator.Update(Scene);
	m_GpuTextureAllocator.Stage(Scene, pRenderCommandContext);
	m_RenderDevice.ExecuteRenderCommandContexts(1, &pRenderCommandContext);
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
	Scene.Camera.SetAspectRatio(m_AspectRatio);
	m_GpuBufferAllocator.Update(Scene);
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

	const INT forwardRenderPassConstantsIndex = NUM_CASCADES; // 0 - NUM_CASCADES reserved for shadow map data
	m_pRenderPassConstantBuffer->Update<RenderPassConstants>(forwardRenderPassConstantsIndex, renderPassCPU);

	m_RenderGraph.Execute(Scene);
	m_RenderGraph.ThreadBarrier();
	m_RenderGraph.ExecuteCommandContexts();
}

void Renderer::Present()
{
	UINT syncInterval = m_Setting.VSync ? 1u : 0u;
	UINT presentFlags = (m_DXGIManager.TearingSupport() && !m_Setting.VSync) ? DXGI_PRESENT_ALLOW_TEARING : 0u;
	DXGI_PRESENT_PARAMETERS presentParameters = { 0u, NULL, NULL, NULL };
	HRESULT hr = m_pSwapChain->Present1(syncInterval, presentFlags, &presentParameters);
	if (hr == DXGI_ERROR_DEVICE_REMOVED)
	{
		CORE_ERROR("DXGI_ERROR_DEVICE_REMOVED");
		Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData> pDRED;
		ThrowCOMIfFailed(m_RenderDevice.GetDevice().GetD3DDevice()->QueryInterface(IID_PPV_ARGS(&pDRED)));
		D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DREDAutoBreadcrumbsOutput;
		D3D12_DRED_PAGE_FAULT_OUTPUT DREDPageFaultOutput;
		ThrowCOMIfFailed(pDRED->GetAutoBreadcrumbsOutput(&DREDAutoBreadcrumbsOutput));
		ThrowCOMIfFailed(pDRED->GetPageFaultAllocationOutput(&DREDPageFaultOutput));
	}
}

Texture& Renderer::CurrentBackBuffer()
{
	return m_BackBufferTexture[m_pSwapChain->GetCurrentBackBufferIndex()];
}

void Renderer::Resize()
{
	m_RenderDevice.GetGraphicsQueue()->WaitForIdle();
	{
		m_AspectRatio = static_cast<float>(m_RefWindow.GetWindowWidth()) / static_cast<float>(m_RefWindow.GetWindowHeight());

		// Release resources before resize swap chain
		for (UINT i = 0u; i < SwapChainBufferCount; ++i)
			m_BackBufferTexture[i].Release();

		// Resize backbuffer
		UINT Nodes[SwapChainBufferCount];
		IUnknown* Queues[SwapChainBufferCount];
		for (UINT i = 0; i < SwapChainBufferCount; ++i)
		{
			Nodes[i] = NodeMask;
			Queues[i] = m_RenderDevice.GetGraphicsQueue()->GetD3DCommandQueue();
		}
		ThrowCOMIfFailed(m_pSwapChain->ResizeBuffers1(SwapChainBufferCount, m_RefWindow.GetWindowWidth(), m_RefWindow.GetWindowHeight(), RendererFormats::SwapChainBufferFormat,
			m_DXGIManager.TearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH, Nodes, Queues));
		// Recreate descriptors
		for (UINT i = 0; i < SwapChainBufferCount; ++i)
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffer;
			ThrowCOMIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
			m_BackBufferTexture[i] = Texture(pBackBuffer);
			m_RenderDevice.GetGlobalResourceStateTracker().AddResourceState(m_BackBufferTexture[i].GetD3DResource(), D3D12_RESOURCE_STATE_COMMON);
		}
	}
	// Wait until resize is complete.
	m_RenderDevice.GetGraphicsQueue()->WaitForIdle();
}