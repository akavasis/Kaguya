#include "pch.h"
#include "Renderer.h"
#include "Core/Window.h"
#include "Core/Time.h"

#include <wincodec.h> // GUID for different file formats, needed for ScreenGrab
#include <ScreenGrab.h> // DirectX::SaveWICTextureToFile

#include "RendererRegistry.h"

#define SHOW_IMGUI_DEMO_WINDOW 1

using namespace DirectX;
using Microsoft::WRL::ComPtr;

Renderer::Renderer()
	: RenderSystem(Application::Window.GetWindowWidth(), Application::Window.GetWindowHeight())
	, RenderDevice(Application::Window)
	, ResourceManager(&RenderDevice)
{
	RenderDevice.ShaderCompiler.SetIncludeDirectory(Application::ExecutableFolderPath / L"Shaders");
}

Renderer::~Renderer()
{

}

bool Renderer::Initialize()
{
	Shaders::Register(RenderDevice.ShaderCompiler);
	Libraries::Register(RenderDevice.ShaderCompiler);

	RenderDevice.CreateCommandContexts(5);

	RaytracingAccelerationStructure.Create(&RenderDevice, PathIntegrator::NumHitGroups);

	Editor.SetContext(&ResourceManager, &Scene);

	PathIntegrator.Create(&RenderDevice);
	PathIntegrator.SetResolution(Application::Window.GetWindowWidth(), Application::Window.GetWindowHeight());

	ToneMapper.Create(&RenderDevice);

	D3D12MA::ALLOCATION_DESC Desc = {};
	Desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
	Materials = RenderDevice.CreateBuffer(&Desc, sizeof(HLSL::Material) * Scene::MAX_MATERIAL_SUPPORTED, D3D12_RESOURCE_FLAG_NONE, 0, D3D12_RESOURCE_STATE_GENERIC_READ);
	ThrowIfFailed(Materials->pResource->Map(0, nullptr, reinterpret_cast<void**>(&pMaterials)));

	return true;
}

void Renderer::Update(const Time& Time)
{
	Scene.Camera.AspectRatio = AspectRatio;

	Scene.PreviousCamera = Scene.Camera;
}

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
	Editor.RenderGui();

	Scene.Update();

	UINT MaterialIndex = 0;
	RaytracingAccelerationStructure.Clear();
	auto view = Scene.Registry.view<MeshFilter, MeshRenderer>();
	for (auto [handle, meshFilter, meshRenderer] : view.each())
	{
		if (meshFilter.pMesh)
		{
			RaytracingAccelerationStructure.AddInstance(&meshRenderer);
			pMaterials[MaterialIndex++] = GetHLSLMaterialDesc(meshRenderer.Material);
		}
	}

	if (!RaytracingAccelerationStructure.Empty())
	{
		auto& AsyncComputeContext = RenderDevice.GetDefaultAsyncComputeContext();
		AsyncComputeContext.Reset(RenderDevice.ComputeFenceValue, RenderDevice.ComputeFence->GetCompletedValue(), &RenderDevice.ComputeQueue);

		RaytracingAccelerationStructure.Build(AsyncComputeContext);

		CommandList* pCommandContexts[] = { &AsyncComputeContext };
		RenderDevice.ExecuteAsyncComputeContexts(1, pCommandContexts);
		RenderDevice.ComputeQueue->Signal(RenderDevice.ComputeFence.Get(), RenderDevice.ComputeFenceValue);
		RenderDevice.GraphicsQueue->Wait(RenderDevice.ComputeFence.Get(), RenderDevice.ComputeFenceValue);
		RenderDevice.ComputeFenceValue++;
	}

	// Begin recording graphics command
	auto& GraphicsContext = RenderDevice.GetDefaultGraphicsContext();
	GraphicsContext.Reset(RenderDevice.GraphicsFenceValue, RenderDevice.GraphicsFence->GetCompletedValue(), &RenderDevice.GraphicsQueue);

	struct SystemConstants
	{
		HLSL::Camera Camera;

		// x, y = Resolution
		// z, w = 1 / Resolution
		float4 Resolution;

		uint TotalFrameCount;
	} g_SystemConstants;

	g_SystemConstants.Camera = GetHLSLCameraDesc(Scene.Camera);
	g_SystemConstants.Resolution = { float(Width), float(Height), 1.0f / float(Width), 1.0f / float(Height) };
	g_SystemConstants.TotalFrameCount = static_cast<unsigned int>(Statistics::TotalFrameCount);

	GraphicsResource SceneConstants = RenderDevice.Device.GraphicsMemory()->AllocateConstant(g_SystemConstants);

	RenderDevice.BindGlobalDescriptorHeap(GraphicsContext);

	if (!RaytracingAccelerationStructure.Empty())
	{
		if (Scene.SceneState == Scene::SCENE_STATE_UPDATED)
		{
			PathIntegrator.Reset();
		}

		PathIntegrator.UpdateShaderTable(RaytracingAccelerationStructure, GraphicsContext);
		PathIntegrator.Render(SceneConstants.GpuAddress(), RaytracingAccelerationStructure, Materials->pResource->GetGPUVirtualAddress(), GraphicsContext);
	}

	auto pBackBuffer = RenderDevice.GetCurrentBackBuffer();
	auto RTV = RenderDevice.GetCurrentBackBufferRenderTargetView();

	GraphicsContext.TransitionBarrier(pBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	{
		auto Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, Width, Height);
		auto ScissorRect = CD3DX12_RECT(0, 0, Width, Height);

		GraphicsContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GraphicsContext->RSSetViewports(1, &Viewport);
		GraphicsContext->RSSetScissorRects(1, &ScissorRect);
		GraphicsContext->OMSetRenderTargets(1, &RTV.CpuHandle, TRUE, nullptr);
		GraphicsContext->ClearRenderTargetView(RTV.CpuHandle, DirectX::Colors::White, 0, nullptr);
		
		// Tone Map
		{
			if (!RaytracingAccelerationStructure.Empty())
			{
				ToneMapper.Apply(PathIntegrator.GetSRV(), RTV, GraphicsContext);
			}
		}
		
		// ImGui Render
		{
			PIXScopedEvent(GraphicsContext.GetApiHandle(), 0, L"ImGui Render");

			ImGui::Render();
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GraphicsContext);
		}
	}
	GraphicsContext.TransitionBarrier(pBackBuffer, D3D12_RESOURCE_STATE_PRESENT);

	CommandList* CommandLists[] = { &GraphicsContext };
	RenderDevice.ExecuteGraphicsContexts(1, CommandLists);
	RenderDevice.Present(Settings::VSync);
	RenderDevice.Device.GraphicsMemory()->Commit(RenderDevice.GraphicsQueue);
	RenderDevice.FlushGraphicsQueue();

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

bool Renderer::Resize(uint32_t Width, uint32_t Height)
{
	RenderDevice.Resize(Width, Height);
	return true;
}

void Renderer::Destroy()
{
	RenderDevice.FlushGraphicsQueue();
	RenderDevice.FlushComputeQueue();
	RenderDevice.FlushCopyQueue();
}

void Renderer::RenderGui()
{
	constexpr ImGuiWindowFlags Flags =
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize;

	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(240, 480), ImGuiCond_Always);
	if (ImGui::Begin("Renderer", nullptr, Flags))
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
			PathIntegrator.RenderGui();
			ImGui::TreePop();
		}
	}
	ImGui::End();
}