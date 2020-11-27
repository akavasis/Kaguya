#include "pch.h"
#include "MeshShader.h"

#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

MeshShaderRenderPass::MeshShaderRenderPass(UINT Width, UINT Height)
	: RenderPass("GBuffer",
		{ Width, Height, DXGI_FORMAT_UNKNOWN },
		NumResources)
{

}

void MeshShaderRenderPass::InitializePipeline(RenderDevice* pRenderDevice)
{
	Resources[RenderTarget] = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "RenderTarget");
	Resources[Depth] = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "Depth");

	pRenderDevice->CreateRootSignature(RootSignatures::MeshShader, [](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<void>(0, 0, 1));
		proxy.AddRootSRVParameter(RootSRV(0, 0)); // Vertices
		proxy.AddRootSRVParameter(RootSRV(1, 0)); // Meshes
		proxy.AddRootSRVParameter(RootSRV(2, 0)); // Meshlets
		proxy.AddRootSRVParameter(RootSRV(3, 0)); // UniqueVertexIndices
		proxy.AddRootSRVParameter(RootSRV(4, 0)); // PrimitiveIndices

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	pRenderDevice->CreateGraphicsPipelineState(GraphicsPSOs::MeshShader, [=](GraphicsPipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::MeshShader);
		proxy.pMS = &Shaders::MS::Meshlet;
		proxy.pPS = &Shaders::MS::Pixel;

		proxy.DepthStencilState.SetDepthEnable(true);

		proxy.PrimitiveTopology = PrimitiveTopology::Triangle;

		proxy.AddRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
		proxy.SetDepthStencilFormat(RenderDevice::DepthStencilFormat);
	});
}

void MeshShaderRenderPass::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
{
	pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.SetClearValue(CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::Colors::Black));
		proxy.BindFlags = Resource::BindFlags::RenderTarget;
		proxy.InitialState = Resource::State::RenderTarget;
	});

	pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(RenderDevice::DepthStencilFormat);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.SetClearValue(CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0));
		proxy.BindFlags = Resource::BindFlags::DepthStencil;
		proxy.InitialState = Resource::State::DepthWrite;
	});
}

void MeshShaderRenderPass::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;
}

void MeshShaderRenderPass::RenderGui()
{

}

void MeshShaderRenderPass::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Mesh Shader");

	RenderContext.TransitionBarrier(Resources[RenderTarget], Resource::State::RenderTarget);
	RenderContext.TransitionBarrier(Resources[Depth], Resource::State::DepthWrite);

	D3D12_VIEWPORT	vp = CD3DX12_VIEWPORT(0.0f, 0.0f, Properties.Width, Properties.Height);
	D3D12_RECT		sr = CD3DX12_RECT(0, 0, Properties.Width, Properties.Height);

	RenderContext->SetViewports(1, &vp);
	RenderContext->SetScissorRects(1, &sr);

	Descriptor RenderTargetViews = RenderContext.GetRenderTargetView(Resources[RenderTarget]);
	Descriptor DepthStencilView = RenderContext.GetDepthStencilView(Resources[EResources::Depth]);

	RenderContext.SetPipelineState(GraphicsPSOs::MeshShader);

	RenderContext->SetRenderTargets(1, &RenderTargetViews, TRUE, DepthStencilView);

	RenderContext->ClearRenderTarget(RenderTargetViews, DirectX::Colors::Black, 0, nullptr);
	RenderContext->ClearDepthStencil(DepthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	RenderContext.SetRootShaderResourceView(1, pGpuScene->GetVertexBuffer());
	RenderContext.SetRootShaderResourceView(2, pGpuScene->GetMeshTable());
	for (auto& modelInstance : pGpuScene->pScene->ModelInstances)
	{
		for (auto& meshInstance : modelInstance)
		{
			RenderContext.SetRoot32BitConstants(0, 1, &meshInstance.InstanceID, 0);

			RenderContext.SetRootShaderResourceView(3, modelInstance.pModel->MeshletResource);
			RenderContext.SetRootShaderResourceView(4, modelInstance.pModel->UniqueVertexIndexResource);
			RenderContext.SetRootShaderResourceView(5, modelInstance.pModel->PrimitiveIndexResource);
		
			RenderContext->DispatchMesh(meshInstance.pMesh->Meshlets.size(), 1, 1);
		}
	}

	RenderContext.TransitionBarrier(Resources[RenderTarget], Resource::State::NonPixelShaderResource);
	RenderContext.TransitionBarrier(Resources[Depth], Resource::State::NonPixelShaderResource);
}

void MeshShaderRenderPass::StateRefresh()
{

}