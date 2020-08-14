#include "pch.h"
#include "Renderer.h"
#include "Core/Application.h"
#include "Core/Window.h"
#include "Core/Time.h"

// Render passes
#include "RenderPass/CSM.h"
#include "RenderPass/ForwardRendering.h"
#include "RenderPass/Tonemap.h"

#include "RenderPass/Raytracing.h"

Renderer::Renderer(Window& Window)
	: pWindow(&Window),
	m_RenderDevice(m_DXGIManager.QueryAdapter(API::API_D3D12).Get()),
	m_RenderGraph(&m_RenderDevice),
	m_GpuBufferAllocator(&m_RenderDevice, 50_MiB, 50_MiB, 64_KiB, 64_KiB),
	m_GpuTextureAllocator(&m_RenderDevice, 100)
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
	Libraries::Register(&m_RenderDevice);
	RootSignatures::Register(&m_RenderDevice);
	GraphicsPSOs::Register(&m_RenderDevice);
	ComputePSOs::Register(&m_RenderDevice);
	RaytracingPSOs::Register(&m_RenderDevice);

	// Create swap chain after command objects have been created
	m_pSwapChain = m_DXGIManager.CreateSwapChain(m_RenderDevice.GetGraphicsQueue()->GetD3DCommandQueue(), Window, RendererFormats::SwapChainBufferFormat, NumSwapChainBuffers);
	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	// Initialize Non-transient resources
	m_SwapChainRTVs = m_RenderDevice.GetDescriptorAllocator()->AllocateRenderTargetDescriptors(NumSwapChainBuffers);
	for (UINT i = 0; i < NumSwapChainBuffers; ++i)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffer;
		ThrowCOMIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
		m_SwapChainTextureHandles[i] = m_RenderDevice.CreateTexture(pBackBuffer, Resource::State::Common);
		m_pSwapChainTextures[i] = m_RenderDevice.GetTexture(m_SwapChainTextureHandles[i]);
		m_RenderDevice.CreateRTV(m_SwapChainTextureHandles[i], m_SwapChainRTVs[i], {}, {}, {});

		std::string name = "Swap Chain: " + std::to_string(i);
		m_RenderGraph.AddNonTransientResource(name.data(), m_SwapChainTextureHandles[i]);
	}

	// Allocate upload context
	m_pUploadCommandContext = m_RenderDevice.AllocateContext(CommandContext::Direct);
}

Renderer::~Renderer()
{
	m_RenderDevice.GetGraphicsQueue()->WaitForIdle();
	m_RenderDevice.GetCopyQueue()->WaitForIdle();
	m_RenderDevice.GetComputeQueue()->WaitForIdle();
}

void Renderer::UploadScene(Scene& Scene)
{
	PIXCapture();

	// Create texture srvs
	{
		m_GpuBufferAllocator.Stage(Scene, m_pUploadCommandContext);
		m_GpuBufferAllocator.Bind(m_pUploadCommandContext);
		m_GpuTextureAllocator.Stage(Scene, m_pUploadCommandContext);
		m_RenderDevice.ExecuteRenderCommandContexts(1, &m_pUploadCommandContext);

		const std::size_t numSRVsToAllocate = m_GpuTextureAllocator.GetNumTextures();
		m_GpuDescriptorIndices.TextureShaderResourceViews = m_RenderDevice.GetDescriptorAllocator()->GetUniversalGpuDescriptorHeap()->AllocateSRDescriptors(numSRVsToAllocate);

		for (size_t i = 0; i < GpuTextureAllocator::NumAssetTextures; ++i)
		{
			auto handle = m_GpuTextureAllocator.AssetTextures[i];
			m_RenderDevice.CreateSRV(handle, m_GpuDescriptorIndices.TextureShaderResourceViews[i]);
		}

		for (auto iter = m_GpuTextureAllocator.TextureStorage.TextureIndices.begin(); iter != m_GpuTextureAllocator.TextureStorage.TextureIndices.end(); ++iter)
		{
			m_RenderDevice.CreateSRV(m_GpuTextureAllocator.TextureStorage.TextureHandles[iter->first], m_GpuDescriptorIndices.TextureShaderResourceViews[iter->second]);
		}
	}

	m_RenderPassConstantBufferHandle = m_RenderDevice.CreateBuffer([](BufferProxy& proxy)
	{
		// + 1 for forward pass render constants
		if (Renderer::Settings::Rasterization)
			proxy.SetSizeInBytes((NUM_CASCADES + 1) * Math::AlignUp<UINT64>(sizeof(RenderPassConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
		else
			proxy.SetSizeInBytes((1) * Math::AlignUp<UINT64>(sizeof(RenderPassConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));

		proxy.SetStride(Math::AlignUp<UINT64>(sizeof(RenderPassConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
		proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});
	m_pRenderPassConstantBuffer = m_RenderDevice.GetBuffer(m_RenderPassConstantBufferHandle);
	m_pRenderPassConstantBuffer->Map();

	if constexpr (Settings::Rasterization)
	{
		AddCSMRenderPass(&m_GpuBufferAllocator, &m_GpuTextureAllocator, m_pRenderPassConstantBuffer, &m_RenderGraph);
		AddForwardRenderingRenderPass(pWindow->GetWindowWidth(), pWindow->GetWindowHeight(), &m_GpuBufferAllocator, &m_GpuTextureAllocator, m_pRenderPassConstantBuffer, &m_RenderGraph);
	}
	else
	{
		AddRaytracingRenderPass(pWindow->GetWindowWidth(), pWindow->GetWindowHeight(), &m_GpuBufferAllocator, &m_GpuTextureAllocator, m_pRenderPassConstantBuffer, &m_RenderGraph);
	}
	AddTonemapRenderPass(m_FrameIndex, &m_RenderGraph);

	m_RenderGraph.Setup();

	// Create render target srvs
	if (Renderer::Settings::Rasterization)
	{
		const std::size_t numSRVsToAllocate = 2;
		m_GpuDescriptorIndices.RenderTargetShaderResourceViews = m_RenderDevice.GetDescriptorAllocator()->AllocateSRDescriptors(numSRVsToAllocate);

		auto pCSMRenderPass = m_RenderGraph.GetRenderPass<CSMRenderPassData>();
		auto pForwardRenderingRenderPass = m_RenderGraph.GetRenderPass<ForwardRenderingRenderPassData>();
		auto pTonemapRenderPass = m_RenderGraph.GetRenderPass<TonemapRenderPassData>();

		m_RenderDevice.CreateSRV(pCSMRenderPass->Outputs[CSMRenderPassData::Outputs::DepthBuffer], m_GpuDescriptorIndices.RenderTargetShaderResourceViews[0]);
		m_RenderDevice.CreateSRV(pForwardRenderingRenderPass->Outputs[ForwardRenderingRenderPassData::Outputs::RenderTarget], m_GpuDescriptorIndices.RenderTargetShaderResourceViews[1]);

		pTonemapRenderPass->Data.TonemapData.InputMapIndex = m_GpuDescriptorIndices.RenderTargetShaderResourceViews[1].HeapIndex -
			m_GpuDescriptorIndices.TextureShaderResourceViews.GetStartDescriptor().HeapIndex;
	}
	else
	{
		const std::size_t numSRVsToAllocate = 1;
		m_GpuDescriptorIndices.RenderTargetShaderResourceViews = m_RenderDevice.GetDescriptorAllocator()->AllocateSRDescriptors(numSRVsToAllocate);

		auto pRaytracingRenderPass = m_RenderGraph.GetRenderPass<RaytracingRenderPassData>();
		auto pTonemapRenderPass = m_RenderGraph.GetRenderPass<TonemapRenderPassData>();

		m_RenderDevice.CreateSRV(pRaytracingRenderPass->Outputs[RaytracingRenderPassData::Outputs::RenderTarget], m_GpuDescriptorIndices.RenderTargetShaderResourceViews[0]);

		pTonemapRenderPass->Data.TonemapData.InputMapIndex = m_GpuDescriptorIndices.RenderTargetShaderResourceViews[0].HeapIndex -
			m_GpuDescriptorIndices.TextureShaderResourceViews.GetStartDescriptor().HeapIndex;
	}
}

void Renderer::Update(const Time& Time)
{
	Renderer::Statistics::TotalFrameCount++;
	Renderer::Statistics::FrameCount++;
	if (Time.TotalTime() - Renderer::Statistics::TimeElapsed >= 1.0)
	{
		Renderer::Statistics::FPS = static_cast<DOUBLE>(Renderer::Statistics::FrameCount);
		Renderer::Statistics::FPMS = 1000.0 / Renderer::Statistics::FPS;
		Renderer::Statistics::FrameCount = 0;
		Renderer::Statistics::TimeElapsed += 1.0;
	}

	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
}

void Renderer::Render(Scene& Scene)
{
	auto pTonemapRenderPass = m_RenderGraph.GetRenderPass<TonemapRenderPassData>();

	pTonemapRenderPass->Data.pDestination = m_pSwapChainTextures[m_FrameIndex];
	pTonemapRenderPass->Data.DestinationRTV = m_SwapChainRTVs[m_FrameIndex];

	m_GpuBufferAllocator.Update(Scene);
	m_GpuTextureAllocator.Update(Scene);
	Scene.Camera.SetAspectRatio(m_AspectRatio);
	Scene.CascadeCameras = Scene.Sun.GenerateCascades(Scene.Camera, Resolutions::SunShadowMapResolution);

	// Update render pass cbuffer
	RenderPassConstants renderPassCPU;
	XMStoreFloat4x4(&renderPassCPU.View, XMMatrixTranspose(Scene.Camera.ViewMatrix()));
	XMStoreFloat4x4(&renderPassCPU.Projection, XMMatrixTranspose(Scene.Camera.ProjectionMatrix()));
	XMStoreFloat4x4(&renderPassCPU.InvView, XMMatrixTranspose(Scene.Camera.InverseViewMatrix()));
	XMStoreFloat4x4(&renderPassCPU.InvProjection, XMMatrixTranspose(Scene.Camera.InverseProjectionMatrix()));
	XMStoreFloat4x4(&renderPassCPU.ViewProjection, XMMatrixTranspose(Scene.Camera.ViewProjectionMatrix()));
	renderPassCPU.EyePosition = Scene.Camera.GetTransform().Position;

	renderPassCPU.Sun = Scene.Sun;
	renderPassCPU.SunShadowMapIndex = m_GpuDescriptorIndices.RenderTargetShaderResourceViews[0].HeapIndex -
		m_GpuDescriptorIndices.TextureShaderResourceViews.GetStartDescriptor().HeapIndex;
	renderPassCPU.BRDFLUTMapIndex = GpuTextureAllocator::AssetTextures::BRDFLUT;
	renderPassCPU.RadianceCubemapIndex = GpuTextureAllocator::AssetTextures::SkyboxCubemap;
	renderPassCPU.IrradianceCubemapIndex = GpuTextureAllocator::AssetTextures::SkyboxIrradianceCubemap;
	renderPassCPU.PrefilteredRadianceCubemapIndex = GpuTextureAllocator::AssetTextures::SkyboxPrefilteredCubemap;

	// Update shadow render pass cbuffer
	if constexpr (Renderer::Settings::Rasterization)
	{
		for (UINT cascadeIndex = 0; cascadeIndex < NUM_CASCADES; ++cascadeIndex)
		{
			RenderPassConstants cascadeShadowPass;
			DirectX::XMStoreFloat4x4(&cascadeShadowPass.ViewProjection, DirectX::XMMatrixTranspose(Scene.CascadeCameras[cascadeIndex].ViewProjectionMatrix()));

			m_pRenderPassConstantBuffer->Update<RenderPassConstants>(cascadeIndex, cascadeShadowPass);
		}

		const INT forwardRenderPassConstantsIndex = NUM_CASCADES; // 0 - NUM_CASCADES reserved for shadow map data
		m_pRenderPassConstantBuffer->Update<RenderPassConstants>(forwardRenderPassConstantsIndex, renderPassCPU);
	}
	else
	{
		const INT raytracingRenderPassConstantsIndex = 0; // 0
		m_pRenderPassConstantBuffer->Update<RenderPassConstants>(raytracingRenderPassConstantsIndex, renderPassCPU);
	}

	m_RenderGraph.Execute(m_FrameIndex, Scene);
	m_RenderGraph.ThreadBarrier();
	m_RenderGraph.ExecuteCommandContexts();

	UINT syncInterval = Renderer::Settings::VSync ? 1u : 0u;
	UINT presentFlags = (m_DXGIManager.TearingSupport() && !Renderer::Settings::VSync) ? DXGI_PRESENT_ALLOW_TEARING : 0u;
	DXGI_PRESENT_PARAMETERS presentParameters = { 0u, NULL, NULL, NULL };
	HRESULT hr = m_pSwapChain->Present1(syncInterval, presentFlags, &presentParameters);
	if (hr == DXGI_ERROR_DEVICE_REMOVED)
	{
		CORE_ERROR("DXGI_ERROR_DEVICE_REMOVED");
		Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData> pDred;
		ThrowCOMIfFailed(m_RenderDevice.GetDevice()->GetD3DDevice()->QueryInterface(IID_PPV_ARGS(&pDred)));
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
			m_RenderDevice.Destroy(&m_SwapChainTextureHandles[i]);

		// Resize backbuffer
		constexpr UINT NodeMask = 0;
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
			m_SwapChainTextureHandles[i] = m_RenderDevice.CreateTexture(pBackBuffer, Resource::State::Common);
			m_pSwapChainTextures[i] = m_RenderDevice.GetTexture(m_SwapChainTextureHandles[i]);
			m_RenderDevice.CreateRTV(m_SwapChainTextureHandles[i], m_SwapChainRTVs[i], {}, {}, {});

			std::string name = "Swap Chain: " + std::to_string(i);
			m_RenderGraph.RemoveNonTransientResource(name.data());
		}

		// Reset back buffer index
		m_FrameIndex = 0;

		m_RenderGraph.Resize(Width, Height);

		// Recreate SRV for render targets
		if constexpr (Renderer::Settings::Rasterization)
		{
			auto pForwardRenderingRenderPass = m_RenderGraph.GetRenderPass<ForwardRenderingRenderPassData>();
			m_RenderDevice.CreateSRV(pForwardRenderingRenderPass->Outputs[ForwardRenderingRenderPassData::Outputs::RenderTarget], m_GpuDescriptorIndices.RenderTargetShaderResourceViews[1]);
		}
		else
		{
			auto pRaytracingRenderPass = m_RenderGraph.GetRenderPass<RaytracingRenderPassData>();
			m_RenderDevice.CreateSRV(pRaytracingRenderPass->Outputs[RaytracingRenderPassData::Outputs::RenderTarget], m_GpuDescriptorIndices.RenderTargetShaderResourceViews[0]);
		}
	}
	m_RenderDevice.GetGraphicsQueue()->WaitForIdle();
}