#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

struct RaytracingRenderPassData
{
	struct Outputs
	{
		enum
		{
			Raytracing,
		};
	};

	struct ResourceViews
	{
		enum
		{
			Raytracing,
		};
	};
};

void AddRaytracingRenderPass(
	UINT Width, UINT Height,
	RenderGraph* pRenderGraph)
{
	pRenderGraph->AddRenderPass<RaytracingRenderPassData>(
		RenderPassType::Graphics,
		[=](RenderPass<RaytracingRenderPassData>& This, RenderDevice* pRenderDevice)
	{
		auto raytracingOutput = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(RendererFormats::HDRBufferFormat);
			proxy.SetWidth(Width);
			proxy.SetHeight(Height);
			proxy.SetClearValue(CD3DX12_CLEAR_VALUE(RendererFormats::HDRBufferFormat, DirectX::Colors::LightBlue));
			proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});

		This.Outputs.push_back(raytracingOutput);

		auto uav = pRenderDevice->GetDescriptorAllocator().AllocateUADescriptors(1);
		pRenderDevice->CreateUAV(raytracingOutput, uav[0], {}, {});

		This.ResourceViews.push_back(uav);

		return [](const RenderPass<RaytracingRenderPassData>& This, const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
		{
			
		};
	},
		[=](RenderPass<RaytracingRenderPassData>& This, UINT Width, UINT Height, RenderDevice* pRenderDevice)
	{
	});
}