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
	RootSignatures::GBufferMeshes = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<void>(0, 0, 1));	// RootConstants
		proxy.AddRootSRVParameter(RootSRV(0, 0));						// Vertex Buffer,			t0 | space0
		proxy.AddRootSRVParameter(RootSRV(1, 0));						// Index Buffer,			t1 | space0
		proxy.AddRootSRVParameter(RootSRV(2, 0));						// Geometry info Buffer,	t2 | space0
		proxy.AddRootSRVParameter(RootSRV(3, 0));						// Material Buffer,			t3 | space0

		proxy.AddStaticSampler(0, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	GraphicsPSOs::GBufferMeshes = pRenderDevice->CreateGraphicsPipelineState([=](GraphicsPipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::GBufferMeshes);
		proxy.pVS = &Shaders::VS::GBufferMeshes;
		proxy.pPS = &Shaders::PS::GBufferMeshes;

		proxy.DepthStencilState.SetDepthEnable(true);

		proxy.PrimitiveTopology = PrimitiveTopology::Triangle;

		proxy.AddRenderTargetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);	// Position
		proxy.AddRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);		// Normal
		proxy.AddRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);		// Albedo
		proxy.AddRenderTargetFormat(DXGI_FORMAT_R8_UINT);				// TypeAndIndex

		proxy.SetDepthStencilFormat(RenderDevice::DepthStencilFormat);
	});

	RootSignatures::GBufferLights = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<void>(0, 0, 1));	// RootConstants
		proxy.AddRootSRVParameter(RootSRV(0, 0));						// Light Buffer, t0 | space0

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	GraphicsPSOs::GBufferLights = pRenderDevice->CreateGraphicsPipelineState([=](GraphicsPipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::GBufferLights);
		proxy.pVS = &Shaders::VS::GBufferLights;
		proxy.pPS = &Shaders::PS::GBufferLights;

		proxy.DepthStencilState.SetDepthEnable(true);

		proxy.PrimitiveTopology = PrimitiveTopology::Triangle;

		proxy.AddRenderTargetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);	// Position
		proxy.AddRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);		// Normal
		proxy.AddRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);		// Albedo
		proxy.AddRenderTargetFormat(DXGI_FORMAT_R8_UINT);				// TypeAndIndex

		proxy.SetDepthStencilFormat(RenderDevice::DepthStencilFormat);
	});
}

void GBuffer::ScheduleResource(ResourceScheduler* pResourceScheduler)
{
	DXGI_FORMAT Formats[EResources::NumResources - 1] =
	{
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8_UINT
	};
	for (size_t i = 0; i < EResources::NumResources - 1; ++i)
	{
		pResourceScheduler->AllocateTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
		{
			proxy.SetFormat(Formats[i]);
			proxy.SetWidth(Properties.Width);
			proxy.SetHeight(Properties.Height);
			proxy.SetClearValue(CD3DX12_CLEAR_VALUE(Formats[i], DirectX::Colors::Black));
			proxy.BindFlags = DeviceResource::BindFlags::RenderTarget;
			proxy.InitialState = DeviceResource::State::RenderTarget;
		});
	}

	pResourceScheduler->AllocateTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
	{
		proxy.SetFormat(RenderDevice::DepthStencilFormat);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.SetClearValue(CD3DX12_CLEAR_VALUE(RenderDevice::DepthStencilFormat, 1.0f, 0));
		proxy.BindFlags = DeviceResource::BindFlags::DepthStencil;
		proxy.InitialState = DeviceResource::State::DepthWrite;
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
	PIXMarker(RenderContext->GetD3DCommandList(), L"GBuffer");

	for (size_t i = 0; i < EResources::NumResources - 1; ++i)
	{
		RenderContext.TransitionBarrier(Resources[i], DeviceResource::State::RenderTarget);
	}
	RenderContext.TransitionBarrier(Resources[EResources::DepthStencil], DeviceResource::State::DepthWrite);

	D3D12_VIEWPORT	vp = CD3DX12_VIEWPORT(0.0f, 0.0f, Properties.Width, Properties.Height);
	D3D12_RECT		sr = CD3DX12_RECT(0, 0, Properties.Width, Properties.Height);

	RenderContext->SetViewports(1, &vp);
	RenderContext->SetScissorRects(1, &sr);

	Descriptor RenderTargetViews[] =
	{
		RenderContext.GetRenderTargetView(Resources[Position]),
		RenderContext.GetRenderTargetView(Resources[Normal]),
		RenderContext.GetRenderTargetView(Resources[Albedo]),
		RenderContext.GetRenderTargetView(Resources[TypeAndIndex])
	};
	Descriptor DepthStencilView = RenderContext.GetDepthStencilView(Resources[EResources::DepthStencil]);

	RenderContext->SetRenderTargets(ARRAYSIZE(RenderTargetViews), RenderTargetViews, TRUE, DepthStencilView);

	for (size_t i = 0; i < EResources::NumResources - 1; ++i)
	{
		RenderContext->ClearRenderTarget(RenderTargetViews[i], DirectX::Colors::Black, 0, nullptr);
	}
	RenderContext->ClearDepthStencil(DepthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	RenderContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	RenderMeshes(RenderContext);
	RenderLights(RenderContext);

	for (size_t i = 0; i < EResources::NumResources; ++i)
	{
		RenderContext.TransitionBarrier(Resources[i], DeviceResource::State::NonPixelShaderResource | DeviceResource::State::PixelShaderResource);
	}
}

void GBuffer::StateRefresh()
{

}

void GBuffer::RenderMeshes(RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Render Meshes");

	RenderContext.SetPipelineState(GraphicsPSOs::GBufferMeshes);
	RenderContext.SetRootShaderResourceView(1, pGpuScene->GetVertexBufferHandle());
	RenderContext.SetRootShaderResourceView(2, pGpuScene->GetIndexBufferHandle());
	RenderContext.SetRootShaderResourceView(3, pGpuScene->GetGeometryInfoTableHandle());
	RenderContext.SetRootShaderResourceView(4, pGpuScene->GetMaterialTableHandle());

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

	RenderContext.SetPipelineState(GraphicsPSOs::GBufferLights);
	RenderContext.SetRootShaderResourceView(1, pGpuScene->GetLightTableHandle());

	const Scene& Scene = *pGpuScene->pScene;
	for (const auto& light : Scene.Lights)
	{
		RenderContext.SetRoot32BitConstants(0, 1, &light.GpuLightIndex, 0);
		RenderContext->DrawInstanced(6, 1, 0, 0);
	}
}