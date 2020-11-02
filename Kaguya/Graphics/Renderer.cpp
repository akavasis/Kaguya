#include "pch.h"
#include "Renderer.h"
#include "Core/Application.h"
#include "Core/Window.h"
#include "Core/Time.h"

#include "RendererRegistry.h"

// Render passes
#include "RenderPass/GBuffer.h"
#include "RenderPass/Shading.h"
#include "RenderPass/Pathtracing.h"
#include "RenderPass/AmbientOcclusion.h"
#include "RenderPass/Accumulation.h"
#include "RenderPass/PostProcess.h"

#include "Scene/SampleScene.h"

#define SHOW_IMGUI_DEMO_WINDOW 0

//----------------------------------------------------------------------------------------------------
Renderer::Renderer()
	: RenderSystem(Application::pWindow->GetWindowWidth(), Application::pWindow->GetWindowHeight())
{

}

//----------------------------------------------------------------------------------------------------
bool Renderer::Initialize()
{
	try
	{
		m_pRenderDevice	= new RenderDevice(m_DXGIManager.QueryAdapter(API::API_D3D12).Get());
		m_pRenderGraph	= new RenderGraph(m_pRenderDevice);
		m_pGpuScene		= new GpuScene(m_pRenderDevice);
		m_pSwapChain	= m_DXGIManager.CreateSwapChain(m_pRenderDevice->GraphicsQueue.GetD3DCommandQueue(), Application::pWindow, RenderDevice::SwapChainBufferFormat, RenderDevice::NumSwapChainBuffers);

		Shaders::Register(m_pRenderDevice);
		Libraries::Register(m_pRenderDevice);
		RootSignatures::Register(m_pRenderDevice);
		GraphicsPSOs::Register(m_pRenderDevice);
		ComputePSOs::Register(m_pRenderDevice);
	}
	catch (std::exception& e)
	{
		MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
		return false;
	}
	catch (...)
	{
		MessageBoxA(nullptr, nullptr, "Unknown Error", MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
		return false;
	}

	m_RenderContext = RenderContext(0, nullptr, nullptr, m_pRenderDevice, m_pRenderDevice->AllocateContext(CommandContext::Direct));

	m_pGpuScene->GpuTextureAllocator.StageSystemReservedTextures(m_RenderContext);

	CommandContext* pCommandContexts[] = { m_RenderContext.GetCommandContext() };
	m_pRenderDevice->ExecuteRenderCommandContexts(1, pCommandContexts);
	m_pGpuScene->GpuTextureAllocator.DisposeResources();

	SetScene(GenerateScene(SampleScene::PlaneWithLights));

	m_pRenderGraph->AddRenderPass(new GBuffer(Width, Height));
	m_pRenderGraph->AddRenderPass(new Shading(Width, Height));
	//m_RenderGraph.AddRenderPass(new Pathtracing(Width, Height));
	//m_RenderGraph.AddRenderPass(new AmbientOcclusion(Width, Height));
	//m_RenderGraph.AddRenderPass(new Accumulation(Width, Height));
	m_pRenderGraph->AddRenderPass(new PostProcess(Width, Height));
	m_pRenderGraph->Initialize();

	m_pRenderGraph->InitializeScene(m_pGpuScene);

	return true;
}

//----------------------------------------------------------------------------------------------------
void Renderer::HandleMouse(int32_t X, int32_t Y, float DeltaTime)
{
	Scene& scene = *m_pGpuScene->pScene;

	scene.Camera.Rotate(Y * DeltaTime, X * DeltaTime);
}

//----------------------------------------------------------------------------------------------------
void Renderer::HandleKeyboard(const Keyboard& Keyboard, float DeltaTime)
{
	Scene& scene = *m_pGpuScene->pScene;

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
	m_pRenderDevice->FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
}

//----------------------------------------------------------------------------------------------------
void Renderer::Render()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#if SHOW_IMGUI_DEMO_WINDOW
	ImGui::ShowDemoWindow();
#endif

	RenderGui();

	Scene& scene = *m_pGpuScene->pScene;
	scene.Camera.RelativeAperture = 0.8f;
	scene.Camera.ShutterTime = 1.0f / 125.0f;
	scene.Camera.SensorSensitivity = 200.0f;

	HLSL::SystemConstants HLSLSystemConstants	= {};
	HLSLSystemConstants.Camera					= m_pGpuScene->GetHLSLCamera();
	HLSLSystemConstants.TotalFrameCount			= static_cast<unsigned int>(Statistics::TotalFrameCount);
	HLSLSystemConstants.NumPolygonalLights		= scene.Lights.size();

	m_pGpuScene->RenderGui();
	m_pGpuScene->Update(AspectRatio, m_RenderContext);
	m_pRenderGraph->RenderGui();
	m_pRenderGraph->UpdateSystemConstants(HLSLSystemConstants);
	m_pRenderGraph->Execute();
	m_pRenderGraph->ExecuteCommandContexts(m_RenderContext);

	uint32_t				SyncInterval		= Settings::VSync ? 1u : 0u;
	uint32_t				PresentFlags		= (m_DXGIManager.TearingSupport() && !Settings::VSync) ? DXGI_PRESENT_ALLOW_TEARING : 0u;
	DXGI_PRESENT_PARAMETERS PresentParameters	= { 0u, nullptr, nullptr, nullptr };
	HRESULT hr = m_pSwapChain->Present1(SyncInterval, PresentFlags, &PresentParameters);
	if (hr == DXGI_ERROR_DEVICE_REMOVED)
	{
		LOG_ERROR("DXGI_ERROR_DEVICE_REMOVED");
	}
}

//----------------------------------------------------------------------------------------------------
bool Renderer::Resize(uint32_t Width, uint32_t Height)
{
	m_pRenderDevice->GraphicsQueue.WaitForIdle();
	{
		// Release resources before resize swap chain
		for (uint32_t i = 0; i < RenderDevice::NumSwapChainBuffers; ++i)
		{
			m_pRenderDevice->Destroy(&m_pRenderDevice->SwapChainTextures[i]);
		}

		// Resize backbuffer
		constexpr uint32_t	NodeMask = 0;
		uint32_t			Nodes[RenderDevice::NumSwapChainBuffers];
		IUnknown*			CommandQueues[RenderDevice::NumSwapChainBuffers];

		for (auto& node : Nodes) { node = NodeMask; }
		for (auto& commandQueue : CommandQueues) { commandQueue = m_pRenderDevice->GraphicsQueue.GetD3DCommandQueue(); }

		uint32_t SwapChainFlags	= m_DXGIManager.TearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		ThrowCOMIfFailed(m_pSwapChain->ResizeBuffers1(0, Width, Height, DXGI_FORMAT_UNKNOWN, SwapChainFlags, Nodes, CommandQueues));

		// Recreate descriptors
		for (uint32_t i = 0; i < RenderDevice::NumSwapChainBuffers; ++i)
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffer;
			ThrowCOMIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
			m_pRenderDevice->SwapChainTextures[i] = m_pRenderDevice->CreateDeviceTexture(pBackBuffer, DeviceResource::State::Common);
			m_pRenderDevice->CreateRenderTargetView(m_pRenderDevice->SwapChainTextures[i]);
		}

		// Reset back buffer index
		m_pRenderDevice->FrameIndex = 0;
	}
	m_pRenderDevice->GraphicsQueue.WaitForIdle();

	return true;
}

//----------------------------------------------------------------------------------------------------
void Renderer::Destroy()
{
	if (m_pGpuScene)
	{
		delete m_pGpuScene;
		m_pGpuScene = nullptr;
	}

	if (m_pRenderGraph)
	{
		delete m_pRenderGraph;
		m_pRenderGraph = nullptr;
	}

	if (m_pRenderDevice)
	{
		m_pRenderDevice->GraphicsQueue.WaitForIdle();
		m_pRenderDevice->CopyQueue.WaitForIdle();
		m_pRenderDevice->ComputeQueue.WaitForIdle();
		delete m_pRenderDevice;
		m_pRenderDevice = nullptr;
	}
}

//----------------------------------------------------------------------------------------------------
void Renderer::SetScene(Scene Scene)
{
	PIXCapture();
	m_Scene				= std::move(Scene);
	//m_Scene.Skybox.Path = Application::ExecutableFolderPath / "Assets/IBL/ChiricahuaPath.hdr";

	m_Scene.Camera.SetLens(DirectX::XM_PIDIV4, 1.0f, 0.1f, 500.0f);
	m_Scene.Camera.SetPosition(0.0f, 5.0f, -20.0f);

	m_pGpuScene->pScene = &m_Scene;

	m_pRenderDevice->BindGpuDescriptorHeap(m_RenderContext.GetCommandContext());
	m_pGpuScene->UploadLights(m_RenderContext);
	m_pGpuScene->UploadMaterials(m_RenderContext);
	m_pGpuScene->UploadModels(m_RenderContext);
	m_pGpuScene->UploadModelInstances(m_RenderContext);

	CommandContext* pCommandContexts[] = { m_RenderContext.GetCommandContext() };
	m_pRenderDevice->ExecuteRenderCommandContexts(ARRAYSIZE(pCommandContexts), pCommandContexts);
	m_pGpuScene->DisposeResources();
}

//----------------------------------------------------------------------------------------------------
void Renderer::RenderGui()
{
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