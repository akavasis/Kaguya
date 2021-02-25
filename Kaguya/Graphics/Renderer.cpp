#include "pch.h"
#include "Renderer.h"
#include "Core/Window.h"
#include "Core/Time.h"

#include "RenderDevice.h"
#include "Scene/Entity.h"

#include <wincodec.h> // GUID for different file formats, needed for ScreenGrab
#include <ScreenGrab.h> // DirectX::SaveWICTextureToFile

#include "RendererRegistry.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

Renderer::Renderer()
	: RenderSystem(Application::Window.GetWindowWidth(), Application::Window.GetWindowHeight())
{
	
}

Renderer::~Renderer()
{

}

Descriptor Renderer::GetViewportDescriptor()
{
	return ToneMapper.GetSRV();
}

Entity Renderer::GetSelectedEntity()
{
	return Picking.GetSelectedEntity().value_or(Entity());
}

void Renderer::SetViewportMousePosition(float MouseX, float MouseY)
{
	this->ViewportMouseX = MouseX;
	this->ViewportMouseY = MouseY;
}

void Renderer::SetViewportResolution(uint32_t Width, uint32_t Height)
{
	this->ViewportWidth = Width;
	this->ViewportHeight = Height;
}

void Renderer::Initialize()
{
	auto& RenderDevice = RenderDevice::Instance();

	Shaders::Register(RenderDevice.ShaderCompiler);
	Libraries::Register(RenderDevice.ShaderCompiler);

	RenderDevice::Instance().CreateCommandContexts(5);

	RaytracingAccelerationStructure.Create(PathIntegrator::NumHitGroups);

	PathIntegrator.Create();
	Picking.Create();
	ToneMapper.Create();

	D3D12MA::ALLOCATION_DESC Desc = {};
	Desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
	Materials = RenderDevice.CreateBuffer(&Desc, sizeof(HLSL::Material) * Scene::MAX_MATERIAL_SUPPORTED, D3D12_RESOURCE_FLAG_NONE, 0, D3D12_RESOURCE_STATE_GENERIC_READ);
	ThrowIfFailed(Materials->pResource->Map(0, nullptr, reinterpret_cast<void**>(&pMaterials)));
}

void Renderer::Update(const Time& Time)
{

}

void Renderer::Render(Scene& Scene)
{
	auto& RenderDevice = RenderDevice::Instance();

	UINT materialIndex = 0;
	RaytracingAccelerationStructure.Clear();
	auto view = Scene.Registry.view<MeshFilter, MeshRenderer>();
	for (auto [handle, meshFilter, meshRenderer] : view.each())
	{
		if (meshFilter.Mesh)
		{
			RaytracingAccelerationStructure.AddInstance(&meshRenderer);
			pMaterials[materialIndex++] = GetHLSLMaterialDesc(meshRenderer.Material);
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

		PathIntegrator.SetResolution(ViewportWidth, ViewportHeight);
		ToneMapper.SetResolution(ViewportWidth, ViewportHeight);

		if (Scene.SceneState == Scene::SCENE_STATE_UPDATED)
		{
			PathIntegrator.Reset();
		}
	}

	// Begin recording graphics command
	auto& GraphicsContext = RenderDevice.GetDefaultGraphicsContext();
	GraphicsContext.Reset(RenderDevice::Instance().GraphicsFenceValue, RenderDevice::Instance().GraphicsFence->GetCompletedValue(), &RenderDevice.GraphicsQueue);

	struct SystemConstants
	{
		HLSL::Camera Camera;

		// x, y = Resolution
		// z, w = 1 / Resolution
		float4 Resolution;

		float2 MousePosition;

		uint TotalFrameCount;
	} g_SystemConstants = {};

	g_SystemConstants.Camera = GetHLSLCameraDesc(Scene.Camera);
	g_SystemConstants.Resolution = { float(ViewportWidth), float(ViewportHeight), 1.0f / float(ViewportWidth), 1.0f / float(ViewportHeight) };
	g_SystemConstants.MousePosition = { ViewportMouseX, ViewportMouseY };
	g_SystemConstants.TotalFrameCount = static_cast<unsigned int>(Statistics::TotalFrameCount);

	GraphicsResource SceneConstants = RenderDevice::Instance().Device.GraphicsMemory()->AllocateConstant(g_SystemConstants);

	RenderDevice::Instance().BindGlobalDescriptorHeap(GraphicsContext);

	if (!RaytracingAccelerationStructure.Empty())
	{
		PathIntegrator.UpdateShaderTable(RaytracingAccelerationStructure, GraphicsContext);
		PathIntegrator.Render(SceneConstants.GpuAddress(), RaytracingAccelerationStructure, Materials->pResource->GetGPUVirtualAddress(), GraphicsContext);

		Picking.UpdateShaderTable(RaytracingAccelerationStructure, GraphicsContext);
		Picking.ShootPickingRay(SceneConstants.GpuAddress(), RaytracingAccelerationStructure, GraphicsContext);

		ToneMapper.Apply(PathIntegrator.GetSRV(), GraphicsContext);
	}

	auto pBackBuffer = RenderDevice::Instance().GetCurrentBackBuffer();
	auto RTV = RenderDevice::Instance().GetCurrentBackBufferRenderTargetView();

	GraphicsContext.TransitionBarrier(pBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	{
		Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(Width), float(Height));
		ScissorRect = CD3DX12_RECT(0, 0, Width, Height);

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

	CommandList* CommandLists[] = { &GraphicsContext };
	RenderDevice::Instance().ExecuteGraphicsContexts(1, CommandLists);
	RenderDevice::Instance().Present(Settings::VSync);
	RenderDevice::Instance().Device.GraphicsMemory()->Commit(RenderDevice::Instance().GraphicsQueue);
	RenderDevice::Instance().FlushGraphicsQueue();
}

void Renderer::Resize(uint32_t Width, uint32_t Height)
{
	RenderDevice::Instance().FlushGraphicsQueue();

	RenderDevice::Instance().Resize(Width, Height);
	PathIntegrator.SetResolution(Width, Height);
	ToneMapper.SetResolution(Width, Height);

	RenderDevice::Instance().FlushGraphicsQueue();
}

void Renderer::Destroy()
{

}

void Renderer::RequestCapture()
{
	auto pTexture = ToneMapper.GetRenderTarget();

	auto FileName = Application::ExecutableFolderPath / L"Capture.png";
	if (FAILED(DirectX::SaveWICTextureToFile(RenderDevice::Instance().GraphicsQueue, pTexture,
		GUID_ContainerFormatPng, FileName.c_str(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, nullptr, true)))
	{
		LOG_WARN("Failed to capture");
	}
}