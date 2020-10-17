#include "pch.h"
#include "LTC.h"
#include "../Renderer.h"
#include "GBuffer.h"

LTC::LTC(UINT Width, UINT Height)
	: RenderPass("LTC",
		{ Width, Height, RendererFormats::HDRBufferFormat })
{

}

void LTC::InitializePipeline(RenderDevice* pRenderDevice)
{
	RootSignatures::LTC = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		proxy.AddRootSRVParameter(RootSRV(0, 0));						// Light Buffer, t0 | space0

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16); // LinearClamp s0 | space0

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	GraphicsPSOs::LTC = pRenderDevice->CreateGraphicsPipelineState([=](GraphicsPipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::LTC);
		proxy.pVS = &Shaders::VS::Quad;
		proxy.pPS = &Shaders::PS::LTC;

		proxy.PrimitiveTopology = PrimitiveTopology::Triangle;

		proxy.AddRenderTargetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
	});
}

void LTC::ScheduleResource(ResourceScheduler* pResourceScheduler)
{
	pResourceScheduler->AllocateTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
	{
		proxy.SetFormat(Properties.Format);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = DeviceResource::BindFlags::RenderTarget;
		proxy.InitialState = DeviceResource::State::RenderTarget;
	});
}

void LTC::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;
}

void LTC::RenderGui()
{

}

void LTC::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"LTC");

	auto pGBufferRenderPass = pRenderGraph->GetRenderPass<GBuffer>();

	RenderContext.TransitionBarrier(Resources[EResources::RenderTarget], DeviceResource::State::RenderTarget);

	struct LTCData
	{
		GlobalConstants GlobalConstants;
		int Position;
		int Normal;
		int Albedo;
		int TypeAndIndex;
		int DepthStencil;

		int LTCLUT1;
		int LTCLUT2;
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
	Data.Position = RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::Position]).HeapIndex;
	Data.Normal = RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::Normal]).HeapIndex;
	Data.Albedo = RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::Albedo]).HeapIndex;
	Data.TypeAndIndex = RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::TypeAndIndex]).HeapIndex;
	Data.DepthStencil = RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::DepthStencil]).HeapIndex;

	Data.LTCLUT1 = RenderContext.GetShaderResourceView(pGpuScene->GpuTextureAllocator.SystemReservedTextures[GpuTextureAllocator::SystemReservedTextures::LTCLUT1]).HeapIndex;
	Data.LTCLUT2 = RenderContext.GetShaderResourceView(pGpuScene->GpuTextureAllocator.SystemReservedTextures[GpuTextureAllocator::SystemReservedTextures::LTCLUT2]).HeapIndex;

	RenderContext.UpdateRenderPassData<LTCData>(Data);

	RenderContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	RenderContext.SetPipelineState(GraphicsPSOs::LTC);
	RenderContext.SetRootShaderResourceView(0, pGpuScene->GetLightTableHandle());

	D3D12_VIEWPORT	vp = CD3DX12_VIEWPORT(0.0f, 0.0f, Properties.Width, Properties.Height);
	D3D12_RECT		sr = CD3DX12_RECT(0, 0, Properties.Width, Properties.Height);

	RenderContext->SetViewports(1, &vp);
	RenderContext->SetScissorRects(1, &sr);

	Descriptor RenderTargetViews[] =
	{
		RenderContext.GetRenderTargetView(Resources[EResources::RenderTarget])
	};

	RenderContext->SetRenderTargets(ARRAYSIZE(RenderTargetViews), RenderTargetViews, TRUE, Descriptor());
	RenderContext->DrawInstanced(3, 1, 0, 0);

	RenderContext.TransitionBarrier(Resources[EResources::RenderTarget], DeviceResource::State::NonPixelShaderResource | DeviceResource::State::PixelShaderResource);
}

void LTC::StateRefresh()
{

}