#include "pch.h"
#include "Renderer.h"
#include "Core/Application.h"
#include "Core/Window.h"
#include "Core/Time.h"

// Render passes
#include "RenderPass/Pathtracing.h"
#include "RenderPass/RaytraceGBuffer.h"
#include "RenderPass/AmbientOcclusion.h"
#include "RenderPass/Accumulation.h"
#include "RenderPass/PostProcess.h"

Renderer::Renderer(const Application& Application, Window& Window)
	: pWindow(&Window),
	m_RenderDevice(m_DXGIManager.QueryAdapter(API::API_D3D12).Get()),
	m_Gui(&m_RenderDevice),
	m_GpuScene(&m_RenderDevice),
	m_RenderGraph(&m_RenderDevice, &m_GpuScene)
{
	auto adapterDesc = m_DXGIManager.GetAdapterDesc();
	AdapterDescription = UTF16ToUTF8(adapterDesc.Description);

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
	m_pSwapChain = m_DXGIManager.CreateSwapChain(m_RenderDevice.GraphicsQueue.GetD3DCommandQueue(), Window, RendererFormats::SwapChainBufferFormat, RenderDevice::NumSwapChainBuffers);

	// Initialize Non-transient resources
	for (size_t i = 0; i < RenderDevice::NumSwapChainBuffers; ++i)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffer;
		ThrowCOMIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
		m_RenderDevice.SwapChainTextures[i] = m_RenderDevice.CreateTexture(pBackBuffer, Resource::State::Common);
		m_RenderDevice.CreateRTV(m_RenderDevice.SwapChainTextures[i]);
	}
}

Renderer::~Renderer()
{
	m_RenderDevice.GraphicsQueue.WaitForIdle();
	m_RenderDevice.CopyQueue.WaitForIdle();
	m_RenderDevice.ComputeQueue.WaitForIdle();
}

void Renderer::SetScene(Scene* pScene)
{
	PIXCapture();

	m_GpuScene.pScene = pScene;

	m_GpuScene.UploadMaterials();
	m_GpuScene.UploadModels();
	m_GpuScene.UploadModelInstances();
	m_GpuScene.Commit(m_RenderDevice.pUploadCommandContext);
	m_RenderDevice.ExecuteRenderCommandContexts(1, &m_RenderDevice.pUploadCommandContext);

	//m_RenderGraph.AddRenderPass(new Pathtracing(pWindow->GetWindowWidth(), pWindow->GetWindowHeight()));
	m_RenderGraph.AddRenderPass(new RaytraceGBuffer(pWindow->GetWindowWidth(), pWindow->GetWindowHeight()));
	m_RenderGraph.AddRenderPass(new AmbientOcclusion(pWindow->GetWindowWidth(), pWindow->GetWindowHeight()));
	m_RenderGraph.AddRenderPass(new Accumulation(pWindow->GetWindowWidth(), pWindow->GetWindowHeight()));
	m_RenderGraph.AddRenderPass(new PostProcess(pWindow->GetWindowWidth(), pWindow->GetWindowHeight()));

	m_RenderGraph.Initialize();
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

	m_RenderDevice.FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
}

void Renderer::RenderGui()
{
	m_Gui.BeginFrame(); // End frame will be called at the end of the Render method by RenderGraph

	if (ImGui::Begin("Adapter Info"))
	{
		auto localVideoMemoryInfo = m_DXGIManager.QueryLocalVideoMemoryInfo();
		auto usageInMiB = ToMiB(localVideoMemoryInfo.CurrentUsage);

		ImGui::Text("Description: %s", AdapterDescription.data());
		ImGui::Text("Current Usage: %d Mib", usageInMiB);
	}
	ImGui::End();

	if (ImGui::Begin("Renderer"))
	{
		ImGui::Text("Statistics");
		ImGui::Text("Total Frame Count: %d", Statistics::TotalFrameCount);
		ImGui::Text("FPS: %f", Statistics::FPS);
		ImGui::Text("FPMS: %f", Statistics::FPMS);

		if (ImGui::TreeNode("Settings"))
		{
			if (ImGui::Button("Restore Defaults"))
			{
				Settings::VSync = false;
			}
			ImGui::Checkbox("V-Sync", &Settings::VSync);
			ImGui::TreePop();
		}
	}
	ImGui::End();

	if (ImGui::Begin("Scene"))
	{
		if (ImGui::TreeNode("Camera"))
		{
			if (ImGui::Button("Restore Defaults"))
			{
				m_GpuScene.pScene->Camera.Aperture = 0.0f;
				m_GpuScene.pScene->Camera.FocalLength = 10.0f;
			}
			ImGui::DragFloat("Aperture", &m_GpuScene.pScene->Camera.Aperture, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Focal Length", &m_GpuScene.pScene->Camera.FocalLength, 0.01f, 0.01f, FLT_MAX);
			ImGui::TreePop();
		}
	}
	ImGui::End();
}

void Renderer::Render()
{
	//PIXCapture();
	m_GpuScene.pScene->Camera.SetAspectRatio(m_AspectRatio);

	m_GpuScene.Update();
	m_RenderGraph.RenderGui();
	m_RenderGraph.Execute();
	m_RenderGraph.ExecuteCommandContexts(&m_Gui);

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
		for (size_t i = 0; i < RenderDevice::NumSwapChainBuffers; ++i)
		{
			m_RenderDevice.Destroy(&m_RenderDevice.SwapChainTextures[i]);
		}

		// Resize backbuffer
		constexpr UINT NodeMask = 0;
		UINT nodes[RenderDevice::NumSwapChainBuffers];
		IUnknown* commandQueues[RenderDevice::NumSwapChainBuffers];
		for (UINT i = 0; i < RenderDevice::NumSwapChainBuffers; ++i)
		{
			nodes[i] = NodeMask;
			commandQueues[i] = m_RenderDevice.GraphicsQueue.GetD3DCommandQueue();
		}
		UINT swapChainFlags = m_DXGIManager.TearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		ThrowCOMIfFailed(m_pSwapChain->ResizeBuffers1(0, Width, Height, DXGI_FORMAT_UNKNOWN, swapChainFlags, nodes, commandQueues));

		// Recreate descriptors
		for (size_t i = 0; i < RenderDevice::NumSwapChainBuffers; ++i)
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffer;
			ThrowCOMIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
			m_RenderDevice.SwapChainTextures[i] = m_RenderDevice.CreateTexture(pBackBuffer, Resource::State::Common);
			m_RenderDevice.CreateRTV(m_RenderDevice.SwapChainTextures[i]);
		}

		// Reset back buffer index
		m_RenderDevice.FrameIndex = 0;
	}
	m_RenderDevice.GraphicsQueue.WaitForIdle();
}