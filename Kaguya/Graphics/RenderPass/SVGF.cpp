#include "pch.h"
#include "SVGF.h"

#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

#include "GBuffer.h"
#include "Shading.h"

//----------------------------------------------------------------------------------------------------
SVGFReproject::SVGFReproject(UINT Width, UINT Height)
	: RenderPass("Spatiotemporal Variance-Guided Filtering: Reproject",
		{ Width, Height, RendererFormats::HDRBufferFormat })
{

}

void SVGFReproject::InitializePipeline(RenderDevice* pRenderDevice)
{
	RootSignatures::SVGF_Reproject = pRenderDevice->CreateRootSignature([&](RootSignatureProxy& proxy)
	{
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	ComputePSOs::SVGF_Reproject = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::SVGF_Reproject);
		proxy.pCS = &Shaders::CS::SVGF_Reproject;
	});
}

void SVGFReproject::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
{
	// PrevRenderTarget0, PrevRenderTarget1, PrevMoments, PrevLinearZ
	for (size_t i = 0; i < 4; ++i)
	{
		pResourceScheduler->AllocateTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
		{
			proxy.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
			proxy.SetWidth(Properties.Width);
			proxy.SetHeight(Properties.Height);
			proxy.InitialState = DeviceResource::State::CopyDest;
		});
	}

	// PrevHistoryLength
	pResourceScheduler->AllocateTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
	{
		proxy.SetFormat(DXGI_FORMAT_R16_FLOAT);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.InitialState = DeviceResource::State::CopyDest;
	});

	// RenderTarget0, RenderTarget1, Moments
	for (size_t i = 0; i < 3; ++i)
	{
		pResourceScheduler->AllocateTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
		{
			proxy.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
			proxy.SetWidth(Properties.Width);
			proxy.SetHeight(Properties.Height);
			proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
			proxy.InitialState = DeviceResource::State::UnorderedAccess;
		});
	}

	// HistoryLength
	pResourceScheduler->AllocateTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
	{
		proxy.SetFormat(DXGI_FORMAT_R16_FLOAT);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
		proxy.InitialState = DeviceResource::State::UnorderedAccess;
	});
}

void SVGFReproject::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{

}

void SVGFReproject::RenderGui()
{

}

void SVGFReproject::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"SVGF Reproject");

	RenderContext.TransitionBarrier(Resources[EResources::PrevRenderTarget0], DeviceResource::State::NonPixelShaderResource);
	RenderContext.TransitionBarrier(Resources[EResources::PrevRenderTarget1], DeviceResource::State::NonPixelShaderResource);
	RenderContext.TransitionBarrier(Resources[EResources::PrevLinearZ], DeviceResource::State::NonPixelShaderResource);
	RenderContext.TransitionBarrier(Resources[EResources::PrevMoments], DeviceResource::State::NonPixelShaderResource);
	RenderContext.TransitionBarrier(Resources[EResources::PrevHistoryLength], DeviceResource::State::NonPixelShaderResource);

	auto pGBufferRenderPass = pRenderGraph->GetRenderPass<GBuffer>();
	auto pShadingRenderPass = pRenderGraph->GetRenderPass<Shading>();

	struct RenderPassData
	{
		float2 RenderTargetDimension;

		float Alpha;
		float MomentsAlpha;

		// Input Textures
		uint Motion;
		uint LinearZ;

		uint Source0;
		uint Source1;

		uint PrevSource0;
		uint PrevSource1;
		uint PrevLinearZ;
		uint PrevMoments;
		uint PrevHistoryLength;

		// Output Textures
		uint RenderTarget0;
		uint RenderTarget1;
		uint Moments;
		uint HistoryLength;
	} Data;

	Data.RenderTargetDimension	= { float(Properties.Width), float(Properties.Height) };

	Data.Alpha					= SVGFSettings::Alpha;
	Data.MomentsAlpha			= SVGFSettings::MomentsAlpha;

	Data.Motion					= RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::SVGF_MotionVector]).HeapIndex;
	Data.LinearZ				= RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::SVGF_LinearZ]).HeapIndex;

	Data.Source0				= RenderContext.GetShaderResourceView(pShadingRenderPass->Resources[Shading::EResources::StochasticShadowed]).HeapIndex;
	Data.Source1				= RenderContext.GetShaderResourceView(pShadingRenderPass->Resources[Shading::EResources::StochasticUnshadowed]).HeapIndex;

	Data.PrevSource0			= RenderContext.GetShaderResourceView(Resources[EResources::PrevRenderTarget0]).HeapIndex;
	Data.PrevSource1			= RenderContext.GetShaderResourceView(Resources[EResources::PrevRenderTarget1]).HeapIndex;
	Data.PrevLinearZ			= RenderContext.GetShaderResourceView(Resources[EResources::PrevLinearZ]).HeapIndex;
	Data.PrevMoments			= RenderContext.GetShaderResourceView(Resources[EResources::PrevMoments]).HeapIndex;
	Data.PrevHistoryLength		= RenderContext.GetShaderResourceView(Resources[EResources::PrevHistoryLength]).HeapIndex;

	Data.RenderTarget0			= RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget0]).HeapIndex;
	Data.RenderTarget1			= RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget1]).HeapIndex;
	Data.Moments				= RenderContext.GetUnorderedAccessView(Resources[EResources::Moments]).HeapIndex;
	Data.HistoryLength			= RenderContext.GetUnorderedAccessView(Resources[EResources::HistoryLength]).HeapIndex;

	RenderContext.UpdateRenderPassData<RenderPassData>(Data);

	RenderContext.SetPipelineState(ComputePSOs::SVGF_Reproject);
	RenderContext->Dispatch2D(Properties.Width, Properties.Height, 8, 8);

	RenderContext.CopyResource(Resources[EResources::PrevRenderTarget0], Resources[EResources::RenderTarget0]);
	RenderContext.CopyResource(Resources[EResources::PrevRenderTarget1], Resources[EResources::RenderTarget1]);
	RenderContext.CopyResource(Resources[EResources::PrevLinearZ], pGBufferRenderPass->Resources[GBuffer::EResources::SVGF_LinearZ]);
	RenderContext.CopyResource(Resources[EResources::PrevMoments], Resources[EResources::Moments]);
	RenderContext.CopyResource(Resources[EResources::PrevHistoryLength], Resources[EResources::HistoryLength]);
}

void SVGFReproject::StateRefresh()
{

}

//----------------------------------------------------------------------------------------------------
SVGFFilterMoments::SVGFFilterMoments(UINT Width, UINT Height)
	: RenderPass("Spatiotemporal Variance-Guided Filtering: Filter Moments",
		{ Width, Height, RendererFormats::HDRBufferFormat })
{

}

void SVGFFilterMoments::InitializePipeline(RenderDevice* pRenderDevice)
{
	RootSignatures::SVGF_FilterMoments = pRenderDevice->CreateRootSignature([&](RootSignatureProxy& proxy)
	{
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	ComputePSOs::SVGF_FilterMoments = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::SVGF_FilterMoments);
		proxy.pCS = &Shaders::CS::SVGF_FilterMoments;
	});
}

void SVGFFilterMoments::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
{
	// RenderTarget0, RenderTarget1
	for (size_t i = 0; i < 2; ++i)
	{
		pResourceScheduler->AllocateTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
		{
			proxy.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
			proxy.SetWidth(Properties.Width);
			proxy.SetHeight(Properties.Height);
			proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
			proxy.InitialState = DeviceResource::State::UnorderedAccess;
		});
	}
}

void SVGFFilterMoments::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{

}

void SVGFFilterMoments::RenderGui()
{

}

void SVGFFilterMoments::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"SVGF Filter Moments");

	auto pGBufferRenderPass = pRenderGraph->GetRenderPass<GBuffer>();
	auto pSVGFReprojectRenderPass = pRenderGraph->GetRenderPass<SVGFReproject>();

	struct RenderPassData
	{
		float2 RenderTargetDimension;

		float PhiColor;
		float PhiNormal;

		// Input Textures
		uint Source0;
		uint Source1;
		uint Moments;
		uint HistoryLength;
		uint Compact;

		// Output Textures
		uint RenderTarget0;
		uint RenderTarget1;
	} Data;

	Data.RenderTargetDimension	= { float(Properties.Width), float(Properties.Height) };

	Data.PhiColor				= SVGFSettings::PhiColor;
	Data.PhiNormal				= SVGFSettings::PhiNormal;

	Data.Source0				= RenderContext.GetShaderResourceView(pSVGFReprojectRenderPass->Resources[SVGFReproject::EResources::RenderTarget0]).HeapIndex;
	Data.Source1				= RenderContext.GetShaderResourceView(pSVGFReprojectRenderPass->Resources[SVGFReproject::EResources::RenderTarget1]).HeapIndex;
	Data.Moments				= RenderContext.GetShaderResourceView(pSVGFReprojectRenderPass->Resources[SVGFReproject::EResources::Moments]).HeapIndex;
	Data.HistoryLength			= RenderContext.GetShaderResourceView(pSVGFReprojectRenderPass->Resources[SVGFReproject::EResources::HistoryLength]).HeapIndex;
	Data.Compact				= RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::SVGF_Compact]).HeapIndex;

	Data.RenderTarget0			= RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget0]).HeapIndex;
	Data.RenderTarget1			= RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget1]).HeapIndex;

	RenderContext.UpdateRenderPassData<RenderPassData>(Data);

	RenderContext.SetPipelineState(ComputePSOs::SVGF_FilterMoments);
	RenderContext->Dispatch2D(Properties.Width, Properties.Height, 8, 8);
}

void SVGFFilterMoments::StateRefresh()
{

}

//----------------------------------------------------------------------------------------------------
SVGFAtrous::SVGFAtrous(UINT Width, UINT Height)
	: RenderPass("Spatiotemporal Variance-Guided Filtering: Atrous",
		{ Width, Height, RendererFormats::HDRBufferFormat })
{

}

void SVGFAtrous::InitializePipeline(RenderDevice* pRenderDevice)
{
	RootSignatures::SVGF_Atrous = pRenderDevice->CreateRootSignature([&](RootSignatureProxy& proxy)
	{
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	ComputePSOs::SVGF_Atrous = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::SVGF_Atrous);
		proxy.pCS = &Shaders::CS::SVGF_Atrous;
	});
}

void SVGFAtrous::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
{
	// RenderTarget0, RenderTarget1
	for (size_t i = 0; i < 2; ++i)
	{
		pResourceScheduler->AllocateTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
		{
			proxy.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
			proxy.SetWidth(Properties.Width);
			proxy.SetHeight(Properties.Height);
			proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
			proxy.InitialState = DeviceResource::State::UnorderedAccess;
		});
	}
}

void SVGFAtrous::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{

}

void SVGFAtrous::RenderGui()
{

}

void SVGFAtrous::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"SVGF Atrous");

	auto pGBufferRenderPass = pRenderGraph->GetRenderPass<GBuffer>();
	auto pSVGFReprojectRenderPass = pRenderGraph->GetRenderPass<SVGFReproject>();
	auto pSVGFFilterMomentsRenderPass = pRenderGraph->GetRenderPass<SVGFFilterMoments>();

	struct RenderPassData
	{
		float2 RenderTargetDimension;

		float PhiColor;
		float PhiNormal;
		uint StepSize;

		// Input Textures
		uint DenoisedSource0;
		uint DenoisedSource1;
		uint Moments;
		uint HistoryLength;
		uint Compact;

		// Output Textures
		uint RenderTarget0;
		uint RenderTarget1;
	} Data;

	Data.RenderTargetDimension	= { float(Properties.Width), float(Properties.Height) };

	Data.PhiColor				= SVGFSettings::PhiColor;
	Data.PhiNormal				= SVGFSettings::PhiNormal;
	Data.StepSize				= 1;

	Data.DenoisedSource0		= RenderContext.GetShaderResourceView(pSVGFFilterMomentsRenderPass->Resources[SVGFFilterMoments::EResources::RenderTarget0]).HeapIndex;
	Data.DenoisedSource1		= RenderContext.GetShaderResourceView(pSVGFFilterMomentsRenderPass->Resources[SVGFFilterMoments::EResources::RenderTarget1]).HeapIndex;
	Data.Moments				= RenderContext.GetShaderResourceView(pSVGFReprojectRenderPass->Resources[SVGFReproject::EResources::Moments]).HeapIndex;
	Data.HistoryLength			= RenderContext.GetShaderResourceView(pSVGFReprojectRenderPass->Resources[SVGFReproject::EResources::HistoryLength]).HeapIndex;
	Data.Compact				= RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::SVGF_Compact]).HeapIndex;

	Data.RenderTarget0			= RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget0]).HeapIndex;
	Data.RenderTarget1			= RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget1]).HeapIndex;

	RenderContext.UpdateRenderPassData<RenderPassData>(Data);

	RenderContext.SetPipelineState(ComputePSOs::SVGF_Atrous);
	RenderContext->Dispatch2D(Properties.Width, Properties.Height, 8, 8);
}

void SVGFAtrous::StateRefresh()
{

}