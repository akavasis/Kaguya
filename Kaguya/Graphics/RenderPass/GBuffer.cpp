#include "pch.h"
#include "GBuffer.h"
#include "../Renderer.h"

GBuffer::GBuffer(UINT Width, UINT Height)
	: RenderPass("GBuffer",
		{ Width, Height, DXGI_FORMAT_R32G32B32A32_FLOAT })
{

}

void GBuffer::InitializePipeline(RenderDevice* pRenderDevice)
{
	RootSignatures::GBuffer = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<void>(0, 0, 1));	// RootConstants
		proxy.AddRootSRVParameter(RootSRV(0, 0));						// Vertex Buffer,			t1 | space0
		proxy.AddRootSRVParameter(RootSRV(1, 0));						// Index Buffer,			t2 | space0
		proxy.AddRootSRVParameter(RootSRV(2, 0));						// Geometry info Buffer,	t3 | space0
		proxy.AddRootSRVParameter(RootSRV(3, 0));						// Material Buffer,			t4 | space0

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	GraphicsPSOs::GBuffer = pRenderDevice->CreateGraphicsPipelineState([=](GraphicsPipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::GBuffer);
		proxy.pVS = &Shaders::VS::GBuffer;
		proxy.pPS = &Shaders::PS::GBuffer;

		proxy.DepthStencilState.SetDepthEnable(true);

		proxy.PrimitiveTopology = PrimitiveTopology::Triangle;

		for (size_t i = 0; i < EResources::NumResources - 1; ++i)
		{
			proxy.AddRenderTargetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
		}

		proxy.SetDepthStencilFormat(RendererFormats::DepthStencilFormat);
	});
}

void GBuffer::ScheduleResource(ResourceScheduler* pResourceScheduler)
{
	for (size_t i = 0; i < EResources::NumResources - 1; ++i)
	{
		pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(Properties.Format);
			proxy.SetWidth(Properties.Width);
			proxy.SetHeight(Properties.Height);
			proxy.SetClearValue(CD3DX12_CLEAR_VALUE(Properties.Format, DirectX::Colors::Black));
			proxy.BindFlags = Resource::BindFlags::RenderTarget;
			proxy.InitialState = Resource::State::RenderTarget;
		});
	}

	pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(RendererFormats::DepthStencilFormat);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.SetClearValue(CD3DX12_CLEAR_VALUE(RendererFormats::DepthStencilFormat, 1.0f, 0));
		proxy.BindFlags = Resource::BindFlags::DepthStencil;
		proxy.InitialState = Resource::State::DepthWrite;
	});
}

void GBuffer::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;
}

void GBuffer::RenderGui()
{
	if (ImGui::TreeNode(Name.data()))
	{
		if (ImGui::Button("Restore Defaults"))
		{
			Settings = SSettings();
		}

		const char* GBufferOutputs[] = { "Position", "Normal", "Albedo", "Emissive", "Specular", "Refraction", "Extra" };
		ImGui::Combo("GBuffer Outputs", &Settings.GBuffer, GBufferOutputs, ARRAYSIZE(GBufferOutputs), ARRAYSIZE(GBufferOutputs));

		ImGui::TreePop();
	}
}

void GBuffer::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"GBuffer");

	for (size_t i = 0; i < EResources::NumResources - 1; ++i)
	{
		RenderContext.TransitionBarrier(Resources[i], Resource::State::RenderTarget);
	}
	RenderContext.TransitionBarrier(Resources[EResources::DepthStencil], Resource::State::DepthWrite);

	struct GBufferData
	{
		GlobalConstants GlobalConstants;
	} Data;

	GlobalConstants globalConstants;
	XMStoreFloat3(&globalConstants.CameraU, pGpuScene->pScene->Camera.GetUVector());
	XMStoreFloat3(&globalConstants.CameraV, pGpuScene->pScene->Camera.GetVVector());
	XMStoreFloat3(&globalConstants.CameraW, pGpuScene->pScene->Camera.GetWVector());
	XMStoreFloat4x4(&globalConstants.View, XMMatrixTranspose(pGpuScene->pScene->Camera.ViewMatrix()));
	XMStoreFloat4x4(&globalConstants.Projection, XMMatrixTranspose(pGpuScene->pScene->Camera.ProjectionMatrix()));
	XMStoreFloat4x4(&globalConstants.InvView, XMMatrixTranspose(pGpuScene->pScene->Camera.InverseViewMatrix()));
	XMStoreFloat4x4(&globalConstants.InvProjection, XMMatrixTranspose(pGpuScene->pScene->Camera.InverseProjectionMatrix()));
	XMStoreFloat4x4(&globalConstants.ViewProjection, XMMatrixTranspose(pGpuScene->pScene->Camera.ViewProjectionMatrix()));
	globalConstants.EyePosition = pGpuScene->pScene->Camera.Transform.Position;
	globalConstants.TotalFrameCount = static_cast<unsigned int>(Renderer::Statistics::TotalFrameCount);

	globalConstants.MaxDepth = 1;
	globalConstants.FocalLength = pGpuScene->pScene->Camera.FocalLength;
	globalConstants.LensRadius = pGpuScene->pScene->Camera.Aperture;

	Data.GlobalConstants = globalConstants;

	RenderContext.UpdateRenderPassData<GBufferData>(Data);

	RenderContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	RenderContext.SetPipelineState(GraphicsPSOs::GBuffer);
	RenderContext.SetRootShaderResourceView(1, pGpuScene->GetVertexBufferHandle());
	RenderContext.SetRootShaderResourceView(2, pGpuScene->GetIndexBufferHandle());
	RenderContext.SetRootShaderResourceView(3, pGpuScene->GetGeometryInfoTableHandle());
	RenderContext.SetRootShaderResourceView(4, pGpuScene->GetMaterialTableHandle());

	D3D12_VIEWPORT	vp = CD3DX12_VIEWPORT(0.0f, 0.0f, Properties.Width, Properties.Height);
	D3D12_RECT		sr = CD3DX12_RECT(0, 0, Properties.Width, Properties.Height);

	RenderContext->SetViewports(1, &vp);
	RenderContext->SetScissorRects(1, &sr);

	Descriptor RenderTargetViews[] =
	{
		RenderContext.GetRenderTargetView(Resources[WorldPosition]),
		RenderContext.GetRenderTargetView(Resources[WorldNormal]),
		RenderContext.GetRenderTargetView(Resources[MaterialAlbedo]),
		RenderContext.GetRenderTargetView(Resources[MaterialEmissive]),
		RenderContext.GetRenderTargetView(Resources[MaterialSpecular]),
		RenderContext.GetRenderTargetView(Resources[MaterialRefraction]),
		RenderContext.GetRenderTargetView(Resources[MaterialExtra])
	};
	Descriptor DepthStencilView = RenderContext.GetDepthStencilView(Resources[EResources::DepthStencil]);

	RenderContext->SetRenderTargets(ARRAYSIZE(RenderTargetViews), RenderTargetViews, TRUE, DepthStencilView);

	for (size_t i = 0; i < EResources::NumResources - 1; ++i)
	{
		RenderContext->ClearRenderTarget(RenderTargetViews[i], DirectX::Colors::Black, 0, nullptr);
	}
	RenderContext->ClearDepthStencil(DepthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	RenderMeshes(RenderContext);

	for (size_t i = 0; i < EResources::NumResources; ++i)
	{
		RenderContext.TransitionBarrier(Resources[i], Resource::State::NonPixelShaderResource | Resource::State::PixelShaderResource);
	}
}

void GBuffer::StateRefresh()
{

}

void GBuffer::RenderMeshes(RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Render Meshes");

	const Scene& Scene = *pGpuScene->pScene;
	std::vector<const ModelInstance*> visibleModelInstanceIndices;
	std::unordered_map<const ModelInstance*, std::vector<UINT>> visibleMeshesIndices;
	visibleModelInstanceIndices.resize(Scene.ModelInstances.size());
	for (const auto& modelInstance : Scene.ModelInstances)
	{
		visibleMeshesIndices[&modelInstance].resize(modelInstance.MeshInstances.size());
	}

	const uint32_t numVisibleModels = CullModels(&Scene.Camera, Scene.ModelInstances, visibleModelInstanceIndices);
	for (uint32_t i = 0; i < numVisibleModels; ++i)
	{
		auto& pModelInstance = visibleModelInstanceIndices[i];
		const uint32_t numVisibleMeshes = CullMeshes(&Scene.Camera, pModelInstance->MeshInstances, visibleMeshesIndices[pModelInstance]);

		for (uint32_t j = 0; j < numVisibleMeshes; ++j)
		{
			const uint32_t MeshIndex = visibleMeshesIndices[pModelInstance][j];
			const MeshInstance& MeshInstance = pModelInstance->MeshInstances[MeshIndex];

			RenderContext.SetRoot32BitConstants(0, 1, &MeshInstance.InstanceID, 0);

			/*
				In the shader we use SV_VertexID (No need to bind VB or IB because it is already a StructuredBuffer)
				to draw objects and in MSDN: VertexID is a 32-bit unsigned integer scalar counter value coming out of Draw*() calls identifying to Shaders each vertex.
				This value can be declared(22.3.11) for input by the Vertex Shader only.

				For Draw() and DrawInstanced(), VertexID starts at 0, and it increments for every vertex. Across instances in DrawInstanced(),
				the count resets back to the start value. Should the 32-bit VertexID calculation overflow, it simply wraps.

				For DrawIndexed() and DrawIndexedInstanced(), VertexID represents the index value.
			*/
			RenderContext->DrawInstanced(MeshInstance.pMesh->IndexCount, 1, MeshInstance.pMesh->BaseVertexLocation, 0);
		}
	}
}

void GBuffer::RenderLights(RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Render Lights");

}