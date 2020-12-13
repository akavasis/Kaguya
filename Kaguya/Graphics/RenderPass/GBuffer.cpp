#include "pch.h"
#include "GBuffer.h"

#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

GBuffer::GBuffer(UINT Width, UINT Height)
	: RenderPass("GBuffer", { Width, Height }, NumResources)
{

}

void GBuffer::InitializePipeline(RenderDevice* pRenderDevice)
{
	Resources[Albedo]				= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "Albedo");
	Resources[Normal]				= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "Normal");
	Resources[TypeAndIndex]			= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "TypeAndIndex");
	Resources[SVGF_LinearZ]			= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "SVGF_LinearZ");
	Resources[SVGF_MotionVector]	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "SVGF_MotionVector");
	Resources[SVGF_Compact]			= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "SVGF_Compact");
	Resources[Depth]				= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "Depth");

	pRenderDevice->CreateRootSignature(RootSignatures::GBufferMeshes, [](RootSignatureBuilder& Builder)
	{
		Builder.AddRootConstantsParameter(RootConstants<void>(0, 0, 1)); // InstanceID

		Builder.AddRootSRVParameter(RootSRV(0, 0)); // Meshlets
		Builder.AddRootSRVParameter(RootSRV(1, 0)); // UniqueVertexIndices
		Builder.AddRootSRVParameter(RootSRV(2, 0)); // PrimitiveIndices
		Builder.AddRootSRVParameter(RootSRV(3, 0)); // Vertices

		Builder.AddRootSRVParameter(RootSRV(4, 0)); // Materials
		Builder.AddRootSRVParameter(RootSRV(5, 0)); // Meshes

		Builder.AddStaticSampler(0, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);

		Builder.DenyTessellationShaderAccess();
		Builder.DenyGSAccess();
	});

	pRenderDevice->CreateGraphicsPipelineState(GraphicsPSOs::GBufferMeshes, [=](GraphicsPipelineStateBuilder& Builder)
	{
		Builder.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::GBufferMeshes);
		Builder.pMS = &Shaders::MS::GBufferMeshes;
		Builder.pPS = &Shaders::PS::GBufferMeshes;

		Builder.DepthStencilState.SetDepthEnable(true);

		Builder.AddRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);		// Albedo
		Builder.AddRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);		// Normal
		Builder.AddRenderTargetFormat(DXGI_FORMAT_R8_UINT);				// TypeAndIndex
		Builder.AddRenderTargetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);	// SVGF_LinearZ
		Builder.AddRenderTargetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);	// SVGF_MotionVector
		Builder.AddRenderTargetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);	// SVGF_Compact

		Builder.SetDepthStencilFormat(RenderDevice::DepthFormat);
	});

	pRenderDevice->CreateRootSignature(RootSignatures::GBufferLights, [](RootSignatureBuilder& Builder)
	{
		Builder.AddRootSRVParameter(RootSRV(0, 0)); // Light Buffer, t0 | space0

		Builder.DenyTessellationShaderAccess();
		Builder.DenyGSAccess();
	});

	pRenderDevice->CreateGraphicsPipelineState(GraphicsPSOs::GBufferLights, [=](GraphicsPipelineStateBuilder& Builder)
	{
		Builder.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::GBufferLights);
		Builder.pMS = &Shaders::MS::GBufferLights;
		Builder.pPS = &Shaders::PS::GBufferLights;

		Builder.DepthStencilState.SetDepthEnable(true);

		Builder.AddRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);		// Albedo
		Builder.AddRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);		// Normal
		Builder.AddRenderTargetFormat(DXGI_FORMAT_R8_UINT);				// TypeAndIndex
		Builder.AddRenderTargetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);	// SVGF_LinearZ
		Builder.AddRenderTargetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);	// SVGF_MotionVector
		Builder.AddRenderTargetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);	// SVGF_Compact

		Builder.SetDepthStencilFormat(RenderDevice::DepthFormat);
	});
}

void GBuffer::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
{
	DXGI_FORMAT Formats[EResources::NumResources - 1] =
	{
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8_UINT,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		DXGI_FORMAT_R32G32B32A32_FLOAT
	};
	for (size_t i = 0; i < EResources::NumResources - 1; ++i)
	{
		pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(Formats[i]);
			proxy.SetWidth(Properties.Width);
			proxy.SetHeight(Properties.Height);
			proxy.SetClearValue(CD3DX12_CLEAR_VALUE(Formats[i], DirectX::Colors::Black));
			proxy.BindFlags = Resource::Flags::RenderTarget;
			proxy.InitialState = Resource::State::RenderTarget;
		});
	}

	pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(RenderDevice::DepthFormat);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.SetClearValue(CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0));
		proxy.BindFlags = Resource::Flags::DepthStencil;
		proxy.InitialState = Resource::State::DepthWrite;
	});
}

void GBuffer::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;
}

void GBuffer::RenderGui()
{

}

void GBuffer::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	D3D12_VIEWPORT	vp = CD3DX12_VIEWPORT(0.0f, 0.0f, Properties.Width, Properties.Height);
	D3D12_RECT		sr = CD3DX12_RECT(0, 0, Properties.Width, Properties.Height);

	RenderContext->SetViewports(1, &vp);
	RenderContext->SetScissorRects(1, &sr);

	Descriptor RenderTargetViews[] =
	{
		RenderContext.GetRenderTargetView(Resources[Albedo]),
		RenderContext.GetRenderTargetView(Resources[Normal]),
		RenderContext.GetRenderTargetView(Resources[TypeAndIndex]),
		RenderContext.GetRenderTargetView(Resources[SVGF_LinearZ]),
		RenderContext.GetRenderTargetView(Resources[SVGF_MotionVector]),
		RenderContext.GetRenderTargetView(Resources[SVGF_Compact]),
	};
	Descriptor DepthStencilView = RenderContext.GetDepthStencilView(Resources[EResources::Depth]);

	RenderContext->SetRenderTargets(ARRAYSIZE(RenderTargetViews), RenderTargetViews, TRUE, DepthStencilView);

	for (size_t i = 0; i < EResources::NumResources - 1; ++i)
	{
		RenderContext->ClearRenderTarget(RenderTargetViews[i], DirectX::Colors::Black, 0, nullptr);
	}
	RenderContext->ClearDepthStencil(DepthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	RenderMeshes(RenderContext);
	RenderLights(RenderContext);
}

void GBuffer::StateRefresh()
{

}

void GBuffer::RenderMeshes(RenderContext& RenderContext)
{
	PIXScopedEvent(RenderContext->GetApiHandle(), 0, L"Render Meshes");

	RenderContext.SetPipelineState(GraphicsPSOs::GBufferMeshes);

	RenderContext.SetRootShaderResourceView(5, pGpuScene->GetMaterialTable());
	RenderContext.SetRootShaderResourceView(6, pGpuScene->GetMeshTable());

	for (auto& modelInstance : pGpuScene->pScene->ModelInstances)
	{
		RenderContext.SetRootShaderResourceView(4, modelInstance.pModel->VertexResource);

		for (auto& meshInstance : modelInstance)
		{
			RenderContext.SetRoot32BitConstants(0, 1, &meshInstance.InstanceID, 0);

			RenderContext.SetRootShaderResourceView(1, meshInstance.pMesh->MeshletResource);
			RenderContext.SetRootShaderResourceView(2, meshInstance.pMesh->VertexIndexResource);
			RenderContext.SetRootShaderResourceView(3, meshInstance.pMesh->PrimitiveIndexResource);

			RenderContext->DispatchMesh(meshInstance.pMesh->Meshlets.size(), 1, 1);
		}
	}
}

void GBuffer::RenderLights(RenderContext& RenderContext)
{
	PIXScopedEvent(RenderContext->GetApiHandle(), 0, L"Render Lights");

	RenderContext.SetPipelineState(GraphicsPSOs::GBufferLights);
	RenderContext.SetRootShaderResourceView(0, pGpuScene->GetLightTable());

	RenderContext->DispatchMesh(pGpuScene->pScene->Lights.size(), 1, 1);
}