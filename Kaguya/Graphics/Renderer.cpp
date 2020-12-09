#include "pch.h"
#include "Renderer.h"
#include "Core/Window.h"
#include "Core/Time.h"

#include <wincodec.h>
#include <ScreenGrab.h>

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
		m_pSwapChain	= m_DXGIManager.CreateSwapChain(m_pRenderDevice->GraphicsQueue.GetApiHandle(), Application::pWindow, RenderDevice::SwapChainBufferFormat, RenderDevice::NumSwapChainBuffers);

		Shaders::Register(m_pRenderDevice);
		Libraries::Register(m_pRenderDevice);
		RootSignatures::Register(m_pRenderDevice);
		GraphicsPSOs::Register(m_pRenderDevice);
		ComputePSOs::Register(m_pRenderDevice);
		RaytracingPSOs::Register(m_pRenderDevice);
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

	m_RenderContext = RenderContext(0, nullptr, nullptr, m_pRenderDevice, m_pRenderDevice->AllocateContext(CommandContext::Graphics));
	m_RenderContext->Reset(m_pRenderDevice->GraphicsFenceValue, m_pRenderDevice->GraphicsFence->GetCompletedValue(), &m_pRenderDevice->GraphicsQueue);

	BuildAccelerationStructureEvent.create();
	AccelerationStructureCompleteEvent.create(wil::EventOptions::ManualReset);
	AsyncComputeThread	= wil::unique_handle(::CreateThread(NULL, 0, AsyncComputeThreadProc, this, 0, nullptr));
	AsyncCopyThread		= wil::unique_handle(::CreateThread(NULL, 0, AsyncCopyThreadProc, this, 0, nullptr));

	m_pGpuScene->TextureManager.StageSystemReservedTextures(m_RenderContext);

	CommandContext* pCommandContexts[] = { m_RenderContext.GetCommandContext() };
	m_pRenderDevice->ExecuteCommandContexts(CommandContext::Graphics, 1, pCommandContexts);
	m_pRenderDevice->FlushGraphicsQueue();
	m_pGpuScene->TextureManager.DisposeResources();

	SetScene(GenerateScene(SampleScene::PlaneWithLights));

	m_pRenderGraph->AddRenderPass(new GBuffer(Width, Height));
	m_pRenderGraph->AddRenderPass(new Shading(Width, Height));
	m_pRenderGraph->AddRenderPass(new SVGFReproject(Width, Height));
	m_pRenderGraph->AddRenderPass(new SVGFFilterMoments(Width, Height));
	m_pRenderGraph->AddRenderPass(new SVGFAtrous(Width, Height));
	m_pRenderGraph->AddRenderPass(new ShadingComposition(Width, Height));
	//m_RenderGraph.AddRenderPass(new Pathtracing(Width, Height));
	//m_RenderGraph.AddRenderPass(new AmbientOcclusion(Width, Height));
	m_pRenderGraph->AddRenderPass(new Accumulation(Width, Height));
	m_pRenderGraph->AddRenderPass(new PostProcess(Width, Height));
	m_pRenderGraph->Initialize(AccelerationStructureCompleteEvent.get());

	m_pRenderGraph->InitializeScene(m_pGpuScene);

	return true;
}

//----------------------------------------------------------------------------------------------------
void Renderer::Update(const Time& Time)
{
	m_pRenderDevice->BackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	m_Scene.PreviousCamera = m_Scene.Camera;
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
	BuildAccelerationStructureEvent.SetEvent(); // Tell the AsyncComputeThreadProc to build our TLAS after we have updated any scene objects

	auto& CommandContexts = m_pRenderGraph->GetCommandContexts();
	for (auto CommandContext : CommandContexts)
	{
		CommandContext->Reset(m_pRenderDevice->GraphicsFenceValue, m_pRenderDevice->GraphicsFence->GetCompletedValue(), &m_pRenderDevice->GraphicsQueue);
	}
	
	m_pRenderGraph->UpdateSystemConstants(HLSLSystemConstants);
	m_pRenderGraph->Execute(Refresh);

	m_pRenderDevice->ExecuteCommandContexts(CommandContext::Graphics, CommandContexts.size(), CommandContexts.data());

	uint32_t				SyncInterval		= Settings::VSync ? 1u : 0u;
	uint32_t				PresentFlags		= (m_DXGIManager.TearingSupport() && !Settings::VSync) ? DXGI_PRESENT_ALLOW_TEARING : 0u;
	HRESULT hr = m_pSwapChain->Present(SyncInterval, PresentFlags);
	if (hr == DXGI_ERROR_DEVICE_REMOVED)
	{
		LOG_ERROR("DXGI_ERROR_DEVICE_REMOVED");
	}

	UINT64 Value = ++m_pRenderDevice->GraphicsFenceValue;
	ThrowCOMIfFailed(m_pRenderDevice->GraphicsQueue->Signal(m_pRenderDevice->GraphicsFence.Get(), Value));
	ThrowCOMIfFailed(m_pRenderDevice->GraphicsFence->SetEventOnCompletion(Value, m_pRenderDevice->GraphicsFenceCompletionEvent.get()));
	m_pRenderDevice->GraphicsFenceCompletionEvent.wait();

	if (Screenshot)
	{
		Screenshot = false;

		auto pTexture = m_pRenderDevice->GetTexture(m_pRenderDevice->BackBufferHandle[m_pRenderDevice->BackBufferIndex]);

		auto FileName = Application::ExecutableFolderPath / L"Screenshot.jpg";
		HRESULT hr = DirectX::SaveWICTextureToFile(m_pRenderDevice->GraphicsQueue.GetApiHandle(), pTexture->GetApiHandle(),
			GUID_ContainerFormatJpeg, FileName.c_str(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT);
		if (FAILED(hr))
		{
			LOG_WARN("Failed to save screenshot");
		}
	}
}

//----------------------------------------------------------------------------------------------------
bool Renderer::Resize(uint32_t Width, uint32_t Height)
{
	m_pRenderDevice->FlushGraphicsQueue();
	{
		// Release resources before resize swap chain
		for (auto SwapChainTexture : m_pRenderDevice->BackBufferHandle)
		{
			m_pRenderDevice->Destroy(SwapChainTexture);
		}

		// Resize backbuffer
		// Note: Cannot use ResizeBuffers1 when debugging in Nsight Graphics, it will crash
		uint32_t SwapChainFlags	= m_DXGIManager.TearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		ThrowCOMIfFailed(m_pSwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, SwapChainFlags));

		// Recreate descriptors
		for (uint32_t i = 0; i < RenderDevice::NumSwapChainBuffers; ++i)
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffer;
			ThrowCOMIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(pBackBuffer.ReleaseAndGetAddressOf())));
			m_pRenderDevice->CreateTexture(m_pRenderDevice->BackBufferHandle[i], pBackBuffer, Resource::State::Common);
			m_pRenderDevice->CreateRenderTargetView(m_pRenderDevice->BackBufferHandle[i]);
		}

		// Reset back buffer index
		m_pRenderDevice->BackBufferIndex = 0;
	}
	m_pRenderDevice->FlushGraphicsQueue();

	return true;
}

//----------------------------------------------------------------------------------------------------
void Renderer::Destroy()
{
	ExitAsyncQueuesThread = true;
	BuildAccelerationStructureEvent.SetEvent();

	HANDLE Handles[] = 
	{
		AsyncComputeThread.get(),
		AsyncCopyThread.get()
	};

	::WaitForMultipleObjects(ARRAYSIZE(Handles), Handles, TRUE, INFINITE);

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
		m_pRenderDevice->FlushGraphicsQueue();
		m_pRenderDevice->FlushComputeQueue();
		m_pRenderDevice->FlushCopyQueue();
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
	m_Scene.Camera.SetAspectRatio(AspectRatio);
	m_Scene.Camera.Transform.Position = { 0.0f, 5.0f, -20.0f };

	m_pGpuScene->pScene = &m_Scene;

	m_RenderContext->Reset(m_pRenderDevice->GraphicsFenceValue, m_pRenderDevice->GraphicsFence->GetCompletedValue(), &m_pRenderDevice->GraphicsQueue);
	m_pRenderDevice->BindGpuDescriptorHeap(m_RenderContext.GetCommandContext());
	m_pGpuScene->UploadTextures(m_RenderContext);
	m_pGpuScene->UploadModels(m_RenderContext);
	m_pGpuScene->UploadLights();
	m_pGpuScene->UploadMaterials();
	m_pGpuScene->UploadMeshes();

	CommandContext* pCommandContexts[] = { m_RenderContext.GetCommandContext() };
	m_pRenderDevice->ExecuteCommandContexts(CommandContext::Graphics, ARRAYSIZE(pCommandContexts), pCommandContexts);
	m_pRenderDevice->FlushGraphicsQueue();
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

		if (ImGui::Button("Screenshot"))
		{
			Screenshot = true;
		}

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
	SetThreadDescription(GetCurrentThread(), __FUNCTIONW__);

	auto pRenderer = reinterpret_cast<Renderer*>(pParameter);
	auto pRenderDevice = pRenderer->m_pRenderDevice;

	RenderContext RenderContext(0, nullptr, nullptr, pRenderDevice, pRenderDevice->AllocateContext(CommandContext::Compute));

	while (true)
	{
		 // We wait here so other render pass threads can get the signal that the AS building is done, then we can call ResetEvent.
		pRenderer->BuildAccelerationStructureEvent.wait();

		if (pRenderer->ExitAsyncQueuesThread)
		{
			LOG_INFO("Exiting {}", __FUNCTION__);
			break;
		}

		pRenderer->AccelerationStructureCompleteEvent.ResetEvent();
		{
			RenderContext->Reset(pRenderDevice->ComputeFenceValue, pRenderDevice->ComputeFence->GetCompletedValue(), &pRenderDevice->ComputeQueue);

			pRenderer->m_pGpuScene->CreateTopLevelAS(RenderContext);

			CommandContext* pCommandContexts[] = { RenderContext.GetCommandContext() };
			pRenderDevice->ExecuteCommandContexts(CommandContext::Compute, 1, pCommandContexts);
			pRenderDevice->FlushComputeQueue();
		}
		pRenderer->AccelerationStructureCompleteEvent.SetEvent(); // Set the event after AS build is complete
	}

	return 0;
}

DWORD WINAPI Renderer::AsyncCopyThreadProc(_In_ PVOID pParameter)
{
	SetThreadDescription(GetCurrentThread(), __FUNCTIONW__);

	auto pRenderer = reinterpret_cast<Renderer*>(pParameter);
	auto pRenderDevice = pRenderer->m_pRenderDevice;

	RenderContext RenderContext(0, nullptr, nullptr, pRenderDevice, pRenderDevice->AllocateContext(CommandContext::Copy));

	while (true)
	{
		if (pRenderer->ExitAsyncQueuesThread)
		{
			LOG_INFO("Exiting {}", __FUNCTION__);
			break;
		}

		// TODO: Add async upload for resources
	}

	return 0;
}