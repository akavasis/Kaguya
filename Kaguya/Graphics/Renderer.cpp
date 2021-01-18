#include "pch.h"
#include "Renderer.h"
#include "Core/Window.h"
#include "Core/Time.h"

#include <wincodec.h> // GUID for different file formats, needed for ScreenGrab
#include <ScreenGrab.h> // DirectX::SaveWICTextureToFile

#include "RendererRegistry.h"

#define SHOW_IMGUI_DEMO_WINDOW 1

using Microsoft::WRL::ComPtr;

//----------------------------------------------------------------------------------------------------
Renderer::Renderer()
	: RenderSystem(Application::Window.GetWindowWidth(), Application::Window.GetWindowHeight())
	, RenderDevice(Application::Window)
{
	RenderDevice.ShaderCompiler.SetIncludeDirectory(Application::ExecutableFolderPath / L"Shaders");
	//ResourceManager.Create(&RenderDevice);

	atexit(Device::ReportLiveObjects);
}

Renderer::~Renderer()
{
	
}

//----------------------------------------------------------------------------------------------------
bool Renderer::Initialize()
{
	try
	{
		Shaders::Register(RenderDevice.ShaderCompiler);
		Libraries::Register(RenderDevice.ShaderCompiler);
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

	RenderDevice.CreateCommandContexts(5);

	//SetScene(GenerateScene(SampleScene::Hyperion));

	return true;
}

//----------------------------------------------------------------------------------------------------
void Renderer::Update(const Time& Time)
{
	Scene.PreviousCamera = Scene.Camera;
}

//----------------------------------------------------------------------------------------------------
void Renderer::HandleInput(float DeltaTime)
{
	auto& Mouse = Application::InputHandler.Mouse;

	// If LMB is pressed and we are not handling raw input and if we are not hovering over any imgui stuff then we update the
	// instance id for editor
	if (Mouse.IsLMBPressed() && !Mouse.UseRawInput && !ImGui::GetIO().WantCaptureMouse)
	{
		//m_pGpuScene->SetSelectedInstanceID(InstanceID);
	}
}

//----------------------------------------------------------------------------------------------------
void Renderer::HandleRawInput(float DeltaTime)
{
	auto& Mouse = Application::InputHandler.Mouse;
	auto& Keyboard = Application::InputHandler.Keyboard;

	if (Keyboard.IsKeyPressed('W'))
		Scene.Camera.Translate(0.0f, 0.0f, DeltaTime);
	if (Keyboard.IsKeyPressed('A'))
		Scene.Camera.Translate(-DeltaTime, 0.0f, 0.0f);
	if (Keyboard.IsKeyPressed('S'))
		Scene.Camera.Translate(0.0f, 0.0f, -DeltaTime);
	if (Keyboard.IsKeyPressed('D'))
		Scene.Camera.Translate(DeltaTime, 0.0f, 0.0f);
	if (Keyboard.IsKeyPressed('E'))
		Scene.Camera.Translate(0.0f, DeltaTime, 0.0f);
	if (Keyboard.IsKeyPressed('Q'))
		Scene.Camera.Translate(0.0f, -DeltaTime, 0.0f);

	while (const auto RawInput = Mouse.ReadRawInput())
	{
		Scene.Camera.Rotate(RawInput->Y * DeltaTime, RawInput->X * DeltaTime);
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

	auto& GraphicsContext = RenderDevice.GetDefaultGraphicsContext();
	GraphicsContext.Reset(RenderDevice.GraphicsFenceValue, RenderDevice.GraphicsFence->GetCompletedValue(), &RenderDevice.GraphicsQueue);

	auto pBackBuffer = RenderDevice.GetCurrentBackBuffer();
	auto RTV = RenderDevice.GetCurrentBackBufferRenderTargetView();

	GraphicsContext.TransitionBarrier(pBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	{
		D3D12_VIEWPORT	Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, Width, Height);
		D3D12_RECT		ScissorRect = CD3DX12_RECT(0, 0, Width, Height);

		RenderDevice.BindGlobalDescriptorHeap(GraphicsContext);
		GraphicsContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GraphicsContext->RSSetViewports(1, &Viewport);
		GraphicsContext->RSSetScissorRects(1, &ScissorRect);
		GraphicsContext->OMSetRenderTargets(1, &RTV.CpuHandle, TRUE, nullptr);
		GraphicsContext->ClearRenderTargetView(RTV.CpuHandle, DirectX::Colors::White, 0, nullptr);
		// ImGui Render
		{
			PIXScopedEvent(GraphicsContext.GetApiHandle(), 0, L"ImGui Render");

			ImGui::Render();
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GraphicsContext);
		}
	}
	GraphicsContext.TransitionBarrier(pBackBuffer, D3D12_RESOURCE_STATE_PRESENT);

//	//m_pGpuScene->RenderGui();
//
//	auto& AsyncComputeContext = m_pRenderDevice->GetDefaultAsyncComputeContext();
//	AsyncComputeContext.Reset(m_pRenderDevice->ComputeFenceValue, m_pRenderDevice->ComputeFence->GetCompletedValue(), &m_pRenderDevice->ComputeQueue);
//
//	m_pGpuScene->CreateTopLevelAS(AsyncComputeContext);
//
//	CommandList* pCommandContexts[] = { &AsyncComputeContext };
//	m_pRenderDevice->ExecuteAsyncComputeContexts(1, pCommandContexts);
//	m_pRenderDevice->ComputeQueue->Signal(m_pRenderDevice->ComputeFence.Get(), m_pRenderDevice->ComputeFenceValue);
//	m_pRenderDevice->GraphicsQueue->Wait(m_pRenderDevice->ComputeFence.Get(), m_pRenderDevice->ComputeFenceValue);
//	m_pRenderDevice->ComputeFenceValue++;
//
//	HLSL::SystemConstants HLSLSystemConstants = {};
//	HLSLSystemConstants.Camera = m_pGpuScene->GetHLSLCamera();
//	HLSLSystemConstants.PreviousCamera = m_pGpuScene->GetHLSLPreviousCamera();
//	HLSLSystemConstants.OutputSize = { float(Width), float(Height), 1.0f / float(Width), 1.0f / float(Height) };
//	HLSLSystemConstants.TotalFrameCount = static_cast<unsigned int>(Statistics::TotalFrameCount);
//	HLSLSystemConstants.NumPolygonalLights = m_Scene.Lights.size();
//
//	bool Refresh = m_pGpuScene->Update(AspectRatio);
//
//	auto& CommandContexts = m_pRenderGraph->GetCommandContexts();
//	for (auto CommandContext : CommandContexts)
//	{
//		CommandContext->Reset(m_pRenderDevice->GraphicsFenceValue, m_pRenderDevice->GraphicsFence->GetCompletedValue(), &m_pRenderDevice->GraphicsQueue);
//	}
//
//	m_pRenderGraph->UpdateSystemConstants(HLSLSystemConstants);
//	m_pRenderGraph->Execute(Refresh);
//

	CommandList* CommandLists[] = { &GraphicsContext };
	RenderDevice.ExecuteGraphicsContexts(1, CommandLists);
	RenderDevice.Present(Settings::VSync);
	RenderDevice.FlushGraphicsQueue();
//
//	InstanceID = Picking->GetInstanceID(m_pRenderDevice.get());
//
	if (Screenshot)
	{
		Screenshot = false;

		auto pTexture = RenderDevice.GetCurrentBackBuffer();

		auto FileName = Application::ExecutableFolderPath / L"Screenshot.png";
		if (FAILED(DirectX::SaveWICTextureToFile(RenderDevice.GraphicsQueue, pTexture,
			GUID_ContainerFormatPng, FileName.c_str(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT, nullptr, nullptr, true)))
		{
			LOG_WARN("Failed to save screenshot");
		}
	}
}

//----------------------------------------------------------------------------------------------------
bool Renderer::Resize(uint32_t Width, uint32_t Height)
{
	RenderDevice.Resize(Width, Height);
	return true;
}

//----------------------------------------------------------------------------------------------------
void Renderer::Destroy()
{
	RenderDevice.FlushGraphicsQueue();
	RenderDevice.FlushComputeQueue();
	RenderDevice.FlushCopyQueue();
}

//----------------------------------------------------------------------------------------------------
//void Renderer::SetScene(Scene Scene)
//{
//	ScopedPIXCapture();
//	m_Scene = std::move(Scene);
//
//	m_pGpuScene->pScene = &m_Scene;
//
//	auto& GraphicsContext = m_pRenderDevice->GetDefaultGraphicsContext();
//
//	GraphicsContext.Reset(m_pRenderDevice->GraphicsFenceValue, m_pRenderDevice->GraphicsFence->GetCompletedValue(), &m_pRenderDevice->GraphicsQueue);
//
//	m_pRenderDevice->BindGpuDescriptorHeap(GraphicsContext);
//	m_pGpuScene->UploadTextures(GraphicsContext);
//	m_pGpuScene->UploadModels(GraphicsContext);
//
//	CommandList* pCommandContexts[] = { &GraphicsContext };
//	m_pRenderDevice->ExecuteGraphicsContexts(ARRAYSIZE(pCommandContexts), pCommandContexts);
//	m_pRenderDevice->FlushGraphicsQueue();
//	m_pGpuScene->DisposeResources();
//}

//----------------------------------------------------------------------------------------------------
void Renderer::RenderGui()
{
	if (ImGui::Begin("Renderer"))
	{
		const auto& AdapterDesc = RenderDevice.GetAdapterDesc();
		auto LocalVideoMemoryInfo = RenderDevice.QueryLocalVideoMemoryInfo();
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