#include "pch.h"
#include "Renderer.h"
#include "Core/Window.h"
#include "Core/Time.h"

#include "RendererRegistry.h"

// Render passes
#include "RenderPass/GBuffer.h"
#include "RenderPass/Shading.h"
#include "RenderPass/SVGF.h"
#include "RenderPass/ShadingComposition.h"
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
		m_pRenderDevice->ShaderCompiler.SetIncludeDirectory(Application::ExecutableFolderPath / L"Shaders");

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

	BuildAccelerationStructureSignal = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	AccelerationStructureCompleteSignal = ::CreateEvent(NULL, TRUE, FALSE, NULL); // This is a manual-reset event
	AsyncComputeThread = ::CreateThread(NULL, 0, AsyncComputeThreadProc, this, 0, nullptr);

	m_pGpuScene->GpuTextureAllocator.StageSystemReservedTextures(m_RenderContext);

	CommandContext* pCommandContexts[] = { m_RenderContext.GetCommandContext() };
	m_pRenderDevice->ExecuteCommandContexts(CommandContext::Direct, 1, pCommandContexts);
	m_pGpuScene->GpuTextureAllocator.DisposeResources();

	SetScene(GenerateScene(SampleScene::PlaneWithLights));

	m_pRenderGraph->AddRenderPass(new GBuffer(Width, Height));
	m_pRenderGraph->AddRenderPass(new Shading(Width, Height));
	m_pRenderGraph->AddRenderPass(new SVGFReproject(Width, Height));
	m_pRenderGraph->AddRenderPass(new SVGFFilterMoments(Width, Height));
	m_pRenderGraph->AddRenderPass(new SVGFAtrous(Width, Height));
	m_pRenderGraph->AddRenderPass(new ShadingComposition(Width, Height));
	m_pRenderGraph->AddRenderPass(new Accumulation(Width, Height));
	//m_RenderGraph.AddRenderPass(new Pathtracing(Width, Height));
	//m_RenderGraph.AddRenderPass(new AmbientOcclusion(Width, Height));
	m_pRenderGraph->AddRenderPass(new PostProcess(Width, Height));
	m_pRenderGraph->Initialize(AccelerationStructureCompleteSignal);

	m_pRenderGraph->InitializeScene(m_pGpuScene);

	return true;
}

//----------------------------------------------------------------------------------------------------
void Renderer::HandleMouse(int32_t X, int32_t Y, float DeltaTime)
{
	m_Scene.Camera.Rotate(Y * DeltaTime, X * DeltaTime);
}

//----------------------------------------------------------------------------------------------------
void Renderer::HandleKeyboard(const Keyboard& Keyboard, float DeltaTime)
{
	if (Keyboard.IsKeyPressed('W'))
		m_Scene.Camera.Translate(0.0f, 0.0f, DeltaTime);
	if (Keyboard.IsKeyPressed('A'))
		m_Scene.Camera.Translate(-DeltaTime, 0.0f, 0.0f);
	if (Keyboard.IsKeyPressed('S'))
		m_Scene.Camera.Translate(0.0f, 0.0f, -DeltaTime);
	if (Keyboard.IsKeyPressed('D'))
		m_Scene.Camera.Translate(DeltaTime, 0.0f, 0.0f);
	if (Keyboard.IsKeyPressed('Q'))
		m_Scene.Camera.Translate(0.0f, DeltaTime, 0.0f);
	if (Keyboard.IsKeyPressed('E'))
		m_Scene.Camera.Translate(0.0f, -DeltaTime, 0.0f);
}

//----------------------------------------------------------------------------------------------------
void Renderer::Update(const Time& Time)
{
	m_pRenderDevice->FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	m_Scene.PreviousCamera = m_Scene.Camera;
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

	m_Scene.Camera.RelativeAperture = 0.8f;
	m_Scene.Camera.ShutterTime = 1.0f / 125.0f;
	m_Scene.Camera.SensorSensitivity = 200.0f;

	HLSL::SystemConstants HLSLSystemConstants	= {};
	HLSLSystemConstants.Camera					= m_pGpuScene->GetHLSLCamera();
	HLSLSystemConstants.PreviousCamera			= m_pGpuScene->GetHLSLPreviousCamera();
	HLSLSystemConstants.OutputSize				= { float(Width), float(Height), 1.0f / float(Width), 1.0f / float(Height) };
	HLSLSystemConstants.TotalFrameCount			= static_cast<unsigned int>(Statistics::TotalFrameCount);
	HLSLSystemConstants.NumPolygonalLights		= m_Scene.Lights.size();

	m_pGpuScene->RenderGui();
	bool Refresh = m_pGpuScene->Update(AspectRatio);
	SetEvent(BuildAccelerationStructureSignal); // Tell the AsyncComputeThreadProc to build our TLAS after we have updated any scene objects
	m_pRenderGraph->UpdateSystemConstants(HLSLSystemConstants);
	m_pRenderGraph->Execute(Refresh);
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
	if (AsyncComputeThread)
	{
		ExitAsyncComputeThread = true;
		::CloseHandle(AsyncComputeThread);
	}

	if (AccelerationStructureCompleteSignal)
	{
		::CloseHandle(AccelerationStructureCompleteSignal);
	}

	if (BuildAccelerationStructureSignal)
	{
		::CloseHandle(BuildAccelerationStructureSignal);
	}

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
	m_Scene.Camera.Transform.Position = { 0.0f, 5.0f, -20.0f };

	m_pGpuScene->pScene = &m_Scene;

	m_pRenderDevice->BindGpuDescriptorHeap(m_RenderContext.GetCommandContext());
	m_pGpuScene->UploadTextures(m_RenderContext);
	m_pGpuScene->UploadModels(m_RenderContext);
	m_pGpuScene->UploadLights();
	m_pGpuScene->UploadMaterials();
	m_pGpuScene->UploadMeshes();

	CommandContext* pCommandContexts[] = { m_RenderContext.GetCommandContext() };
	m_pRenderDevice->ExecuteCommandContexts(CommandContext::Direct, ARRAYSIZE(pCommandContexts), pCommandContexts);
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

		if (ImGui::TreeNode("Render Pipeline"))
		{
			m_pRenderGraph->RenderGui();
			ImGui::TreePop();
		}
	}
	ImGui::End();
}

//----------------------------------------------------------------------------------------------------
DWORD WINAPI Renderer::AsyncComputeThreadProc(_In_ PVOID pParameter)
{
	auto pRenderer = reinterpret_cast<Renderer*>(pParameter);
	auto pRenderDevice = pRenderer->m_pRenderDevice;

	RenderContext RenderContext(0, nullptr, nullptr, pRenderDevice, pRenderDevice->AllocateContext(CommandContext::Compute));

	while (!pRenderer->ExitAsyncComputeThread)
	{
		 WaitForSingleObject(pRenderer->BuildAccelerationStructureSignal, INFINITE);

		 ResetEvent(pRenderer->AccelerationStructureCompleteSignal);
		 {
			 pRenderer->m_pGpuScene->CreateTopLevelAS(RenderContext);

			 CommandContext* pCommandContexts[] = { RenderContext.GetCommandContext() };
			 pRenderDevice->ExecuteCommandContexts(CommandContext::Compute, 1, pCommandContexts);
		 }
		 SetEvent(pRenderer->AccelerationStructureCompleteSignal); // Set the event after AS build is complete
	}

	return 0;
}