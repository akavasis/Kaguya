#include "pch.h"
#include "Renderer.h"
#include "Core/Window.h"
#include "Core/Time.h"

#include <wincodec.h> // GUID_ContainerFormatJpeg, needed for ScreenGrab
#include <ScreenGrab.h> // DirectX::SaveWICTextureToFile

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
#include "RenderPass/Picking.h"

#include "Scene/SampleScene.h"

#define SHOW_IMGUI_DEMO_WINDOW 0

using Microsoft::WRL::ComPtr;

//----------------------------------------------------------------------------------------------------
Renderer::Renderer()
	: RenderSystem(Application::Window.GetWindowWidth(), Application::Window.GetWindowHeight())
{

}

//----------------------------------------------------------------------------------------------------
bool Renderer::Initialize()
{
	try
	{
		m_pRenderDevice	= std::make_unique<RenderDevice>(Application::Window);
		m_pRenderDevice->ShaderCompiler.SetIncludeDirectory(Application::ExecutableFolderPath / L"Shaders");

		m_pRenderGraph = std::make_unique<RenderGraph>(m_pRenderDevice.get());
		m_pGpuScene = std::make_unique<GpuScene>(m_pRenderDevice.get());

		Shaders::Register(m_pRenderDevice->ShaderCompiler);
		Libraries::Register(m_pRenderDevice->ShaderCompiler);
		RootSignatures::Register(m_pRenderDevice.get());
		GraphicsPSOs::Register(m_pRenderDevice.get());
		ComputePSOs::Register(m_pRenderDevice.get());
		RaytracingPSOs::Register(m_pRenderDevice.get());
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

	BuildAccelerationStructureEvent.create();
	AccelerationStructureCompleteEvent.create(wil::EventOptions::ManualReset);

	//m_pRenderGraph->AddRenderPass(new GBuffer(Width, Height));
	//m_pRenderGraph->AddRenderPass(new Shading(Width, Height));
	//m_pRenderGraph->AddRenderPass(new SVGFReproject(Width, Height));
	//m_pRenderGraph->AddRenderPass(new SVGFFilterMoments(Width, Height));
	//m_pRenderGraph->AddRenderPass(new SVGFAtrous(Width, Height));
	//m_pRenderGraph->AddRenderPass(new ShadingComposition(Width, Height));
	m_pRenderGraph->AddRenderPass(new Pathtracing(Width, Height));
	//m_RenderGraph.AddRenderPass(new AmbientOcclusion(Width, Height));
	m_pRenderGraph->AddRenderPass(new Accumulation(Width, Height));
	m_pRenderGraph->AddRenderPass(new PostProcess(Width, Height));
	m_pRenderGraph->AddRenderPass(new Picking());

	m_pRenderDevice->CreateCommandContexts(m_pRenderGraph->NumRenderPass());

	m_pRenderGraph->Initialize(AccelerationStructureCompleteEvent.get());

	m_GraphicsContext = RenderContext(0, nullptr, nullptr, m_pRenderDevice.get(), m_pRenderDevice->GetDefaultGraphicsContext());

	SetScene(GenerateScene(SampleScene::CornellBox));

	m_pRenderGraph->InitializeScene(m_pGpuScene.get());

	AsyncComputeThread = wil::unique_handle(::CreateThread(NULL, 0, AsyncComputeThreadProc, this, 0, nullptr));
	AsyncCopyThread = wil::unique_handle(::CreateThread(NULL, 0, AsyncCopyThreadProc, this, 0, nullptr));

	return true;
}

//----------------------------------------------------------------------------------------------------
void Renderer::Update(const Time& Time)
{
	m_Scene.PreviousCamera = m_Scene.Camera;
}

//----------------------------------------------------------------------------------------------------
void Renderer::HandleInput(float DeltaTime)
{
	auto& Mouse = Application::InputHandler.Mouse;

	// If LMB is pressed and if we are not hovering over any imgui stuff then we update the
	// instance id for editor
	if (Mouse.IsLMBPressed() && !ImGui::GetIO().WantCaptureMouse)
	{
		m_pGpuScene->SetSelectedInstanceID(InstanceID);
	}
}

//----------------------------------------------------------------------------------------------------
void Renderer::HandleRawInput(float DeltaTime)
{
	auto& Mouse = Application::InputHandler.Mouse;
	auto& Keyboard = Application::InputHandler.Keyboard;

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

	while (const auto RawInput = Mouse.ReadRawInput())
	{
		m_Scene.Camera.Rotate(RawInput->Y * DeltaTime, RawInput->X * DeltaTime);
	}
}

//----------------------------------------------------------------------------------------------------
void Renderer::Render()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();
#if SHOW_IMGUI_DEMO_WINDOW
	ImGui::ShowDemoWindow();
#endif

	RenderGui();
	m_pGpuScene->RenderGui();

	//auto AsyncComputeContext = m_pRenderDevice->GetDefaultAsyncComputeContext();
	//AsyncComputeContext->Reset(m_pRenderDevice->ComputeFenceValue, m_pRenderDevice->ComputeFence->GetCompletedValue(), &m_pRenderDevice->ComputeQueue);

	//m_pGpuScene->CreateTopLevelAS(AsyncComputeContext);

	//CommandContext* pCommandContexts[] = { AsyncComputeContext };
	//m_pRenderDevice->ExecuteAsyncComputeContexts(1, pCommandContexts);
	//m_pRenderDevice->ComputeQueue.GetApiHandle()->Signal(m_pRenderDevice->ComputeFence.Get(), m_pRenderDevice->ComputeFenceValue);
	//m_pRenderDevice->GraphicsQueue.GetApiHandle()->Wait(m_pRenderDevice->ComputeFence.Get(), m_pRenderDevice->ComputeFenceValue);
	//m_pRenderDevice->ComputeFenceValue++;

	HLSL::SystemConstants HLSLSystemConstants	= {};
	HLSLSystemConstants.Camera					= m_pGpuScene->GetHLSLCamera();
	HLSLSystemConstants.PreviousCamera			= m_pGpuScene->GetHLSLPreviousCamera();
	HLSLSystemConstants.OutputSize				= { float(Width), float(Height), 1.0f / float(Width), 1.0f / float(Height) };
	HLSLSystemConstants.TotalFrameCount			= static_cast<unsigned int>(Statistics::TotalFrameCount);
	HLSLSystemConstants.NumPolygonalLights		= m_Scene.Lights.size();
	HLSLSystemConstants.Skybox					= m_pRenderDevice->GetShaderResourceView(m_pGpuScene->TextureManager.GetSkybox()).HeapIndex;

	bool Refresh = m_pGpuScene->Update(AspectRatio);
	BuildAccelerationStructureEvent.SetEvent(); // Tell the AsyncComputeThreadProc to build our TLAS after we have updated any scene objects

	auto& CommandContexts = m_pRenderGraph->GetCommandContexts();
	for (auto CommandContext : CommandContexts)
	{
		CommandContext->Reset(m_pRenderDevice->GraphicsFenceValue, m_pRenderDevice->GraphicsFence->GetCompletedValue(), &m_pRenderDevice->GraphicsQueue);
	}
	
	m_pRenderGraph->UpdateSystemConstants(HLSLSystemConstants);
	m_pRenderGraph->Execute(Refresh);

	m_pRenderDevice->ExecuteGraphicsContexts(CommandContexts.size(), CommandContexts.data());
	m_pRenderDevice->Present(Settings::VSync);

	UINT64 Value = ++m_pRenderDevice->GraphicsFenceValue;
	ThrowIfFailed(m_pRenderDevice->GraphicsQueue.GetApiHandle()->Signal(m_pRenderDevice->GraphicsFence.Get(), Value));
	ThrowIfFailed(m_pRenderDevice->GraphicsFence->SetEventOnCompletion(Value, m_pRenderDevice->GraphicsFenceCompletionEvent.get()));
	m_pRenderDevice->GraphicsFenceCompletionEvent.wait();

	auto PickingRenderPass = m_pRenderGraph->GetRenderPass<Picking>();
	InstanceID = PickingRenderPass->GetInstanceID(m_pRenderDevice.get());

	if (Screenshot)
	{
		Screenshot = false;

		auto pTexture = m_pRenderDevice->GetTexture(m_pRenderDevice->GetCurrentBackBufferHandle());

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
	m_pRenderDevice->Resize(Width, Height);
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

	m_pGpuScene.reset();
	m_pRenderGraph.reset();

	if (m_pRenderDevice)
	{
		m_pRenderDevice->FlushGraphicsQueue();
		m_pRenderDevice->FlushComputeQueue();
		m_pRenderDevice->FlushCopyQueue();
	}

	m_pRenderDevice.reset();

#ifdef _DEBUG
	ComPtr<IDXGIDebug> DXGIDebug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&DXGIDebug))))
	{
		DXGIDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
	}
#endif
}

//----------------------------------------------------------------------------------------------------
void Renderer::SetScene(Scene Scene)
{
	PIXCapture();
	m_Scene				= std::move(Scene);
	//m_Scene.Skybox.Path = Application::ExecutableFolderPath / "Assets/IBL/ChiricahuaPath.hdr";

	m_Scene.Camera.Transform.Position = { 0.0f, 5.0f, -20.0f };

	m_pGpuScene->pScene = &m_Scene;

	m_GraphicsContext->Reset(m_pRenderDevice->GraphicsFenceValue, m_pRenderDevice->GraphicsFence->GetCompletedValue(), &m_pRenderDevice->GraphicsQueue);

	m_pRenderDevice->BindGpuDescriptorHeap(m_GraphicsContext.GetCommandContext());
	m_pGpuScene->UploadTextures(m_GraphicsContext);
	m_pGpuScene->UploadModels(m_GraphicsContext);

	CommandContext* pCommandContexts[] = { m_GraphicsContext.GetCommandContext() };
	m_pRenderDevice->ExecuteGraphicsContexts(ARRAYSIZE(pCommandContexts), pCommandContexts);
	m_pRenderDevice->FlushGraphicsQueue();
	m_pGpuScene->DisposeResources();
}

//----------------------------------------------------------------------------------------------------
void Renderer::RenderGui()
{
	if (ImGui::Begin("Renderer"))
	{
		const auto& AdapterDesc = m_pRenderDevice->GetAdapterDesc();
		auto LocalVideoMemoryInfo = m_pRenderDevice->QueryLocalVideoMemoryInfo();
		auto UsageInMiB = ToMiB(LocalVideoMemoryInfo.CurrentUsage);
		ImGui::Text("GPU: %ls", AdapterDesc.Description);
		ImGui::Text("VRAM Usage: %d Mib", UsageInMiB);

		ImGui::Text("");
		ImGui::Text("Total Frame Count: %d", Statistics::TotalFrameCount);
		ImGui::Text("FPS: %f", Statistics::FPS);
		ImGui::Text("FPMS: %f", Statistics::FPMS);

		ImGui::Text("");

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

	if (ImGui::Begin("Debug"))
	{
		// Debug gui here
		ImGui::Text("InstanceID: %i", InstanceID);
		ImGui::Text("Mouse::X: %i Mouse::Y: %i", Application::InputHandler.Mouse.X, Application::InputHandler.Mouse.Y);
		ImGui::Text("ImGui::X: %f ImGui::Y: %f", ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
		ImGui::Text("ImGui::WantCaptureMouse: %i", ImGui::GetIO().WantCaptureMouse);
	}
	ImGui::End();
}

//----------------------------------------------------------------------------------------------------
DWORD WINAPI Renderer::AsyncComputeThreadProc(_In_ PVOID pParameter)
{
	SetThreadDescription(GetCurrentThread(), __FUNCTIONW__);

	auto pRenderer = reinterpret_cast<Renderer*>(pParameter);
	auto pRenderDevice = pRenderer->m_pRenderDevice.get();

	RenderContext RenderContext(0, nullptr, nullptr, pRenderDevice, pRenderDevice->GetDefaultAsyncComputeContext());

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

			pRenderer->m_pGpuScene->CreateTopLevelAS(RenderContext.GetCommandContext());

			CommandContext* pCommandContexts[] = { RenderContext.GetCommandContext() };
			pRenderDevice->ExecuteAsyncComputeContexts(1, pCommandContexts);
			pRenderDevice->FlushComputeQueue();
		}
		pRenderer->AccelerationStructureCompleteEvent.SetEvent(); // Set the event after AS build is complete
	}

	return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------------------------------
DWORD WINAPI Renderer::AsyncCopyThreadProc(_In_ PVOID pParameter)
{
	SetThreadDescription(GetCurrentThread(), __FUNCTIONW__);

	auto pRenderer = reinterpret_cast<Renderer*>(pParameter);
	auto pRenderDevice = pRenderer->m_pRenderDevice.get();

	RenderContext RenderContext(0, nullptr, nullptr, pRenderDevice, pRenderDevice->GetDefaultCopyContext());

	while (true)
	{
		if (pRenderer->ExitAsyncQueuesThread)
		{
			LOG_INFO("Exiting {}", __FUNCTION__);
			break;
		}

		// TODO: Add async upload for resources
	}

	return EXIT_SUCCESS;
}