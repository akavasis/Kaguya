#include "pch.h"
#include "Renderer.h"
#include "Core/Application.h"
#include "Core/Window.h"
#include "Core/Time.h"

#include "RendererRegistry.h"

// Render passes
#include "RenderPass/GBuffer.h"
#include "RenderPass/LTC.h"
#include "RenderPass/Pathtracing.h"
#include "RenderPass/RaytraceGBuffer.h"
#include "RenderPass/AmbientOcclusion.h"
#include "RenderPass/Accumulation.h"
#include "RenderPass/PostProcess.h"

#include "Scene/SampleScene.h"

//----------------------------------------------------------------------------------------------------
Renderer::Renderer(Window* pWindow)
	: RenderSystem(pWindow->GetWindowWidth(), pWindow->GetWindowHeight()),
	m_RenderDevice(m_DXGIManager.QueryAdapter(API::API_D3D12).Get()),
	m_Gui(&m_RenderDevice),
	m_GpuScene(&m_RenderDevice),
	m_RenderGraph(&m_RenderDevice),

	m_RenderContext(0, nullptr, &m_RenderDevice, m_RenderDevice.AllocateContext(CommandContext::Direct))
{
	m_pSwapChain = m_DXGIManager.CreateSwapChain(m_RenderDevice.GraphicsQueue.GetD3DCommandQueue(), *pWindow, RendererFormats::SwapChainBufferFormat, RenderDevice::NumSwapChainBuffers);
}

//----------------------------------------------------------------------------------------------------
void Renderer::Initialize()
{
	Shaders::Register(&m_RenderDevice);
	Libraries::Register(&m_RenderDevice);
	RootSignatures::Register(&m_RenderDevice);
	GraphicsPSOs::Register(&m_RenderDevice);
	ComputePSOs::Register(&m_RenderDevice);

	SetScene(GenerateScene(SampleScene::PlaneWithLights));

	for (size_t i = 0; i < RenderDevice::NumSwapChainBuffers; ++i)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffer;
		ThrowCOMIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
		m_RenderDevice.SwapChainTextures[i] = m_RenderDevice.CreateDeviceTexture(pBackBuffer, DeviceResource::State::Common);
		m_RenderDevice.CreateRenderTargetView(m_RenderDevice.SwapChainTextures[i]);
	}

	m_RenderGraph.AddRenderPass(new GBuffer(Width, Height));
	m_RenderGraph.AddRenderPass(new LTC(Width, Height));
	//m_RenderGraph.AddRenderPass(new Pathtracing(Width, Height));
	//m_RenderGraph.AddRenderPass(new RaytraceGBuffer(Width, Height));
	//m_RenderGraph.AddRenderPass(new AmbientOcclusion(Width, Height));
	//m_RenderGraph.AddRenderPass(new Accumulation(Width, Height));
	m_RenderGraph.AddRenderPass(new PostProcess(Width, Height));
	m_RenderGraph.Initialize();

	m_RenderGraph.InitializeScene(&m_GpuScene);
}

//----------------------------------------------------------------------------------------------------
void Renderer::HandleMouse(int32_t X, int32_t Y, float DeltaTime)
{
	Scene& scene = *m_GpuScene.pScene;

	scene.Camera.Rotate(Y * DeltaTime, X * DeltaTime);
}

//----------------------------------------------------------------------------------------------------
void Renderer::HandleKeyboard(const Keyboard& Keyboard, float DeltaTime)
{
	Scene& scene = *m_GpuScene.pScene;

	if (Keyboard.IsKeyPressed('W'))
		scene.Camera.Translate(0.0f, 0.0f, DeltaTime);
	if (Keyboard.IsKeyPressed('A'))
		scene.Camera.Translate(-DeltaTime, 0.0f, 0.0f);
	if (Keyboard.IsKeyPressed('S'))
		scene.Camera.Translate(0.0f, 0.0f, -DeltaTime);
	if (Keyboard.IsKeyPressed('D'))
		scene.Camera.Translate(DeltaTime, 0.0f, 0.0f);
	if (Keyboard.IsKeyPressed('Q'))
		scene.Camera.Translate(0.0f, DeltaTime, 0.0f);
	if (Keyboard.IsKeyPressed('E'))
		scene.Camera.Translate(0.0f, -DeltaTime, 0.0f);
}

//----------------------------------------------------------------------------------------------------
void Renderer::Update(const Time& Time)
{
	m_RenderDevice.FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
}

//----------------------------------------------------------------------------------------------------
void Renderer::Render()
{
	RenderGui();

	m_GpuScene.RenderGui();
	m_GpuScene.Update(AspectRatio, m_RenderContext);
	m_RenderGraph.RenderGui();
	m_RenderGraph.Execute();
	m_RenderGraph.ExecuteCommandContexts(m_RenderContext, &m_Gui);

	UINT syncInterval = Settings::VSync ? 1u : 0u;
	UINT presentFlags = (m_DXGIManager.TearingSupport() && !Settings::VSync) ? DXGI_PRESENT_ALLOW_TEARING : 0u;
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

//----------------------------------------------------------------------------------------------------
void Renderer::Resize(uint32_t Width, uint32_t Height)
{
	m_RenderDevice.GraphicsQueue.WaitForIdle();
	{
		// Release resources before resize swap chain
		for (size_t i = 0; i < RenderDevice::NumSwapChainBuffers; ++i)
		{
			m_RenderDevice.Destroy(&m_RenderDevice.SwapChainTextures[i]);
		}

		// Resize backbuffer
		constexpr UINT	NodeMask = 0;
		UINT			Nodes[RenderDevice::NumSwapChainBuffers];
		IUnknown*		CommandQueues[RenderDevice::NumSwapChainBuffers];
		for (UINT i = 0; i < RenderDevice::NumSwapChainBuffers; ++i)
		{
			Nodes[i]			= NodeMask;
			CommandQueues[i]	= m_RenderDevice.GraphicsQueue.GetD3DCommandQueue();
		}
		UINT swapChainFlags		= m_DXGIManager.TearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		ThrowCOMIfFailed(m_pSwapChain->ResizeBuffers1(0, Width, Height, DXGI_FORMAT_UNKNOWN, swapChainFlags, Nodes, CommandQueues));

		// Recreate descriptors
		for (size_t i = 0; i < RenderDevice::NumSwapChainBuffers; ++i)
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffer;
			ThrowCOMIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
			m_RenderDevice.SwapChainTextures[i] = m_RenderDevice.CreateDeviceTexture(pBackBuffer, DeviceResource::State::Common);
			m_RenderDevice.CreateRenderTargetView(m_RenderDevice.SwapChainTextures[i]);
		}

		// Reset back buffer index
		m_RenderDevice.FrameIndex = 0;
	}
	m_RenderDevice.GraphicsQueue.WaitForIdle();
}

//----------------------------------------------------------------------------------------------------
void Renderer::Destroy()
{
	m_RenderDevice.GraphicsQueue.WaitForIdle();
	m_RenderDevice.CopyQueue.WaitForIdle();
	m_RenderDevice.ComputeQueue.WaitForIdle();
}

//----------------------------------------------------------------------------------------------------
void Renderer::SetScene(Scene Scene)
{
	PIXCapture();
	m_Scene				= std::move(Scene);
	m_Scene.Skybox.Path = Application::ExecutableFolderPath / "Assets/IBL/ChiricahuaPath.hdr";

	m_Scene.Camera.SetLens(DirectX::XM_PIDIV4, 1.0f, 0.1f, 500.0f);
	m_Scene.Camera.SetPosition(0.0f, 5.0f, -20.0f);

	m_GpuScene.pScene = &m_Scene;

	m_RenderDevice.BindUniversalGpuDescriptorHeap(m_RenderContext.GetCommandContext());
	m_GpuScene.UploadLights(m_RenderContext);
	m_GpuScene.UploadMaterials(m_RenderContext);
	m_GpuScene.UploadModels(m_RenderContext);
	m_GpuScene.UploadModelInstances(m_RenderContext);

	CommandContext* pCommandContexts[] = { m_RenderContext.GetCommandContext() };
	m_RenderDevice.ExecuteRenderCommandContexts(ARRAYSIZE(pCommandContexts), pCommandContexts);
	m_GpuScene.DisposeResources();
}

//----------------------------------------------------------------------------------------------------
void Renderer::RenderGui()
{
	m_Gui.BeginFrame(); // End frame will be called at the end of the Render method by RenderGraph

	if (ImGui::Begin("Renderer"))
	{
		auto localVideoMemoryInfo = m_DXGIManager.QueryLocalVideoMemoryInfo();
		auto usageInMiB = ToMiB(localVideoMemoryInfo.CurrentUsage);
		ImGui::Text("VRAM Usage: %d Mib", usageInMiB);

		ImGui::Text("");
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
}