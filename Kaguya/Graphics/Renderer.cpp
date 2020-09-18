#include "pch.h"
#include "Renderer.h"
#include "Core/Application.h"
#include "Core/Window.h"
#include "Core/Time.h"

// Render passes
#include "RenderPass/Raytracing.h"
#include "RenderPass/Accumulation.h"
#include "RenderPass/Tonemap.h"

Renderer::Renderer(const Application& Application, Window& Window)
	: pWindow(&Window),
	m_RenderDevice(m_DXGIManager.QueryAdapter(API::API_D3D12).Get()),
	m_Gui(&m_RenderDevice),
	m_RenderGraph(&m_RenderDevice),
	m_GpuBufferAllocator(&m_RenderDevice, 200_MiB, 200_MiB, 64_KiB),
	m_GpuTextureAllocator(&m_RenderDevice, 1000)
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

	Shaders::Register(&m_RenderDevice, Application.ExecutableFolderPath());
	Libraries::Register(&m_RenderDevice, Application.ExecutableFolderPath());
	RootSignatures::Register(&m_RenderDevice);
	GraphicsPSOs::Register(&m_RenderDevice);
	ComputePSOs::Register(&m_RenderDevice);
	RaytracingPSOs::Register(&m_RenderDevice);

	// Create swap chain after command objects have been created
	m_pSwapChain = m_DXGIManager.CreateSwapChain(m_RenderDevice.GraphicsQueue.GetD3DCommandQueue(), Window, RendererFormats::SwapChainBufferFormat, NumSwapChainBuffers);
	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	// Initialize Non-transient resources
	m_SwapChainRTVs = m_RenderDevice.DescriptorAllocator.AllocateRenderTargetDescriptors(NumSwapChainBuffers);
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
	m_RenderDevice.GraphicsQueue.WaitForIdle();
	m_RenderDevice.CopyQueue.WaitForIdle();
	m_RenderDevice.ComputeQueue.WaitForIdle();
}

void Renderer::UploadScene(Scene* Scene)
{
	PIXCapture();

	// Create texture srvs
	{
		m_GpuBufferAllocator.Stage(*Scene, m_pUploadCommandContext);
		m_GpuBufferAllocator.Bind(m_pUploadCommandContext);
		m_GpuTextureAllocator.Stage(*Scene, m_pUploadCommandContext);
		m_RenderDevice.ExecuteRenderCommandContexts(1, &m_pUploadCommandContext);

		const std::size_t numSRVsToAllocate = m_GpuTextureAllocator.GetNumTextures();
		m_GpuDescriptorIndices.TextureShaderResourceViews = m_RenderDevice.DescriptorAllocator.AllocateSRDescriptors(numSRVsToAllocate);

		for (size_t i = 0; i < GpuTextureAllocator::NumAssetTextures; ++i)
		{
			auto handle = m_GpuTextureAllocator.RendererReseveredTextures[i];
			m_RenderDevice.CreateSRV(handle, m_GpuDescriptorIndices.TextureShaderResourceViews[i]);
		}

		for (auto iter = m_GpuTextureAllocator.TextureStorage.TextureIndices.begin(); iter != m_GpuTextureAllocator.TextureStorage.TextureIndices.end(); ++iter)
		{
			m_RenderDevice.CreateSRV(m_GpuTextureAllocator.TextureStorage.TextureHandles[iter->first], m_GpuDescriptorIndices.TextureShaderResourceViews[iter->second]);
		}
	}

	m_RenderPassConstantBufferHandle = m_RenderDevice.CreateBuffer([](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes((1) * Math::AlignUp<UINT64>(sizeof(RenderPassConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));

		proxy.SetStride(Math::AlignUp<UINT64>(sizeof(RenderPassConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
		proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});
	m_pRenderPassConstantBuffer = m_RenderDevice.GetBuffer(m_RenderPassConstantBufferHandle);
	m_pRenderPassConstantBuffer->Map();

	m_RenderGraph.AddRenderPass(new Raytracing(pWindow->GetWindowWidth(), pWindow->GetWindowHeight(), &m_GpuBufferAllocator, &m_GpuTextureAllocator, m_pRenderPassConstantBuffer));
	m_RenderGraph.AddRenderPass(new Accumulation(pWindow->GetWindowWidth(), pWindow->GetWindowHeight()));
	m_RenderGraph.AddRenderPass(new Tonemap());

	m_RenderGraph.Setup();

	// Create render target srvs
	const std::size_t numSRVsToAllocate = 1;
	m_GpuDescriptorIndices.RenderTargetShaderResourceViews = m_RenderDevice.DescriptorAllocator.AllocateSRDescriptors(numSRVsToAllocate);

	auto pAccumulationRenderPass = m_RenderGraph.GetRenderPass<Accumulation>();
	auto pTonemapRenderPass = m_RenderGraph.GetRenderPass<Tonemap>();

	m_RenderDevice.CreateSRV(pAccumulationRenderPass->Outputs[Accumulation::EOutputs::RenderTarget], m_GpuDescriptorIndices.RenderTargetShaderResourceViews[0]);

	pAccumulationRenderPass->LastCameraTransform = Scene->Camera.Transform;
	pTonemapRenderPass->Settings.InputMapIndex = m_GpuDescriptorIndices.RenderTargetShaderResourceViews.GetStartDescriptor().HeapIndex -
		m_GpuDescriptorIndices.TextureShaderResourceViews.GetStartDescriptor().HeapIndex;
}

void Renderer::Update(const Time& Time)
{
	Statistics::TotalFrameCount++;
	Statistics::FrameCount++;
	if (Time.TotalTime() - Statistics::TimeElapsed >= 1.0)
	{
		Statistics::FPS = static_cast<DOUBLE>(Statistics::FrameCount);
		Statistics::FPMS = 1000.0 / Statistics::FPS;
		Statistics::FrameCount = 0;
		Statistics::TimeElapsed += 1.0;
	}

	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
}

void Renderer::RenderUI(Scene* Scene)
{
	m_Gui.BeginFrame(); // End frame will be called at the end of the Render method by RenderGraph

	if (ImGui::Begin("Renderer"))
	{
		ImGui::Text("Statistics");
		ImGui::Text("Total Frame Count: %d", Statistics::TotalFrameCount);
		ImGui::Text("FPS: %f", Statistics::FPS);
		ImGui::Text("FPMS: %f", Statistics::FPMS);
		ImGui::Text("Accumulation: %f", Statistics::Accumulation);

		if (ImGui::TreeNode("Setting"))
		{
			ImGui::Checkbox("V-Sync", &Settings::VSync);
			ImGui::TreePop();
		}

		m_RenderGraph.RenderGui();
	}
	ImGui::End();

	if (ImGui::Begin("Scene"))
	{
		if (ImGui::TreeNode("Camera"))
		{
			ImGui::DragFloat("Aperture", &Scene->Camera.Aperture, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Focal Length", &Scene->Camera.FocalLength, 0.01f, 0.01f, FLT_MAX);
			ImGui::TreePop();
		}
	}

	ImGui::End();
}

void Renderer::Render(Scene* Scene)
{
	PIXCapture();

	auto pAccumulationRenderPass = m_RenderGraph.GetRenderPass<Accumulation>();
	auto pTonemapRenderPass = m_RenderGraph.GetRenderPass<Tonemap>();

	// If the camera has moved
	if (Scene->Camera.Aperture != pAccumulationRenderPass->LastAperture ||
		Scene->Camera.FocalLength != pAccumulationRenderPass->LastFocalLength ||
		Scene->Camera.Transform != pAccumulationRenderPass->LastCameraTransform)
	{
		Renderer::Statistics::Accumulation = 0;
		pAccumulationRenderPass->LastAperture = Scene->Camera.Aperture;
		pAccumulationRenderPass->LastFocalLength = Scene->Camera.FocalLength;
		pAccumulationRenderPass->LastCameraTransform = Scene->Camera.Transform;
	}
	pAccumulationRenderPass->Settings.AccumulationCount = Renderer::Statistics::Accumulation++;

	pTonemapRenderPass->pDestination = m_pSwapChainTextures[m_FrameIndex];
	pTonemapRenderPass->DestinationRTV = m_SwapChainRTVs[m_FrameIndex];

	m_GpuBufferAllocator.Update(*Scene);
	m_GpuTextureAllocator.Update(*Scene);
	Scene->Camera.SetAspectRatio(m_AspectRatio);

	// Update render pass cbuffer
	RenderPassConstants renderPassCPU;
	XMStoreFloat3(&renderPassCPU.CameraU, Scene->Camera.GetUVector());
	XMStoreFloat3(&renderPassCPU.CameraV, Scene->Camera.GetVVector());
	XMStoreFloat3(&renderPassCPU.CameraW, Scene->Camera.GetWVector());
	XMStoreFloat4x4(&renderPassCPU.View, XMMatrixTranspose(Scene->Camera.ViewMatrix()));
	XMStoreFloat4x4(&renderPassCPU.Projection, XMMatrixTranspose(Scene->Camera.ProjectionMatrix()));
	XMStoreFloat4x4(&renderPassCPU.InvView, XMMatrixTranspose(Scene->Camera.InverseViewMatrix()));
	XMStoreFloat4x4(&renderPassCPU.InvProjection, XMMatrixTranspose(Scene->Camera.InverseProjectionMatrix()));
	XMStoreFloat4x4(&renderPassCPU.ViewProjection, XMMatrixTranspose(Scene->Camera.ViewProjectionMatrix()));
	renderPassCPU.EyePosition = Scene->Camera.Transform.Position;
	renderPassCPU.TotalFrameCount = static_cast<unsigned int>(Renderer::Statistics::TotalFrameCount);

	renderPassCPU.Sun = Scene->Sun;
	renderPassCPU.SunShadowMapIndex = m_GpuDescriptorIndices.RenderTargetShaderResourceViews[0].HeapIndex - m_GpuDescriptorIndices.TextureShaderResourceViews.GetStartDescriptor().HeapIndex;
	renderPassCPU.BRDFLUTMapIndex = GpuTextureAllocator::RendererReseveredTextures::BRDFLUT;
	renderPassCPU.RadianceCubemapIndex = GpuTextureAllocator::RendererReseveredTextures::SkyboxCubemap;
	renderPassCPU.IrradianceCubemapIndex = GpuTextureAllocator::RendererReseveredTextures::SkyboxIrradianceCubemap;
	renderPassCPU.PrefilteredRadianceCubemapIndex = GpuTextureAllocator::RendererReseveredTextures::SkyboxPrefilteredCubemap;

	renderPassCPU.MaxDepth = 4;
	renderPassCPU.FocalLength = Scene->Camera.FocalLength;
	renderPassCPU.LensRadius = Scene->Camera.Aperture;

	const INT raytracingRenderPassConstantsIndex = 0; // 0
	m_pRenderPassConstantBuffer->Update<RenderPassConstants>(raytracingRenderPassConstantsIndex, renderPassCPU);

	m_RenderGraph.Update();
	m_RenderGraph.Execute(m_FrameIndex, *Scene);
	m_RenderGraph.ThreadBarrier();
	m_RenderGraph.ExecuteCommandContexts(m_pSwapChainTextures[m_FrameIndex], m_SwapChainRTVs[m_FrameIndex], &m_Gui);

	UINT syncInterval = Renderer::Settings::VSync ? 1u : 0u;
	UINT presentFlags = (m_DXGIManager.TearingSupport() && !Renderer::Settings::VSync) ? DXGI_PRESENT_ALLOW_TEARING : 0u;
	DXGI_PRESENT_PARAMETERS presentParameters = { 0u, NULL, NULL, NULL };
	HRESULT hr = m_pSwapChain->Present1(syncInterval, presentFlags, &presentParameters);
	if (hr == DXGI_ERROR_DEVICE_REMOVED)
	{
		CORE_ERROR("DXGI_ERROR_DEVICE_REMOVED");
		Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData> pDred;
		ThrowCOMIfFailed(m_RenderDevice.Device.GetD3DDevice()->QueryInterface(IID_PPV_ARGS(&pDred)));
		D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DredAutoBreadcrumbsOutput;
		D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
		ThrowCOMIfFailed(pDred->GetAutoBreadcrumbsOutput(&DredAutoBreadcrumbsOutput));
		ThrowCOMIfFailed(pDred->GetPageFaultAllocationOutput(&DredPageFaultOutput));
	}
}

void Renderer::Resize(UINT Width, UINT Height)
{
	m_RenderDevice.GraphicsQueue.WaitForIdle();
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
			commandQueues[i] = m_RenderDevice.GraphicsQueue.GetD3DCommandQueue();
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
		auto pAccumulationRenderPass = m_RenderGraph.GetRenderPass<Accumulation>();
		m_RenderDevice.CreateSRV(pAccumulationRenderPass->Outputs[Accumulation::EOutputs::RenderTarget], m_GpuDescriptorIndices.RenderTargetShaderResourceViews[0]);
	}
	m_RenderDevice.GraphicsQueue.WaitForIdle();
}