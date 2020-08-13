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
			RenderTarget,
		};
	};

	struct ResourceViews
	{
		enum
		{
			GeometryTable,
			RenderTarget
		};
	};

	GpuBufferAllocator* pGpuBufferAllocator;
	GpuTextureAllocator* pGpuTextureAllocator;
	Buffer* pRenderPassConstantBuffer;
};

void AddRaytracingRenderPass(
	UINT Width, UINT Height,
	GpuBufferAllocator* pGpuBufferAllocator,
	GpuTextureAllocator* pGpuTextureAllocator,
	Buffer* pRenderPassConstantBuffer,
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
			proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});

		This.Outputs.push_back(raytracingOutput);

		auto srv = pRenderDevice->GetDescriptorAllocator()->AllocateSRDescriptors(3);
		pRenderDevice->CreateSRV(pGpuBufferAllocator->GetRTTLASResourceHandle(), srv[0]);
		pRenderDevice->CreateSRV(pGpuBufferAllocator->GetVertexBufferHandle(), srv[1]);
		pRenderDevice->CreateSRV(pGpuBufferAllocator->GetIndexBufferHandle(), srv[2]);

		auto uav = pRenderDevice->GetDescriptorAllocator()->AllocateUADescriptors(1);
		pRenderDevice->CreateUAV(raytracingOutput, uav[0], {}, {});

		This.ResourceViews.push_back(std::move(srv));
		This.ResourceViews.push_back(std::move(uav));

		This.Data.pGpuBufferAllocator = pGpuBufferAllocator;
		This.Data.pGpuTextureAllocator = pGpuTextureAllocator;
		This.Data.pRenderPassConstantBuffer = pRenderPassConstantBuffer;
		return [](const RenderPass<RaytracingRenderPassData>& This, const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
		{
			PIXMarker(pCommandContext->GetD3DCommandList(), L"Raytracing");
			auto pOutput = RenderGraphRegistry.GetTexture(This.Outputs[RaytracingRenderPassData::Outputs::RenderTarget]);
			auto pRayGenerationShaderTable = RenderGraphRegistry.GetBuffer(This.Data.pGpuBufferAllocator->GetRayGenerationShaderTableHandle());
			auto pMissShaderTable = RenderGraphRegistry.GetBuffer(This.Data.pGpuBufferAllocator->GetMissShaderTableHandle());
			auto pHitGroupShaderTable = RenderGraphRegistry.GetBuffer(This.Data.pGpuBufferAllocator->GetHitGroupShaderTableHandle());

			auto pRaytracingPipelineState = RenderGraphRegistry.GetRaytracingPSO(RaytracingPSOs::Raytracing);

			pCommandContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

			pCommandContext->SetComputeRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::Raytracing::Global));
			
			pCommandContext->SetComputeRootDescriptorTable(RootParameters::Raytracing::GeometryTable, This.ResourceViews[0].GetStartDescriptor().GPUHandle);
			pCommandContext->SetComputeRootDescriptorTable(RootParameters::Raytracing::RenderTarget, This.ResourceViews[1].GetStartDescriptor().GPUHandle);
			pCommandContext->SetComputeRootConstantBufferView(RootParameters::Raytracing::Camera, This.Data.pRenderPassConstantBuffer->GetGpuVirtualAddressAt(0));

			D3D12_DISPATCH_RAYS_DESC desc = {};

			// The ray generation shaders are always at the beginning of the ST. 
			desc.RayGenerationShaderRecord.StartAddress = pRayGenerationShaderTable->GetGpuVirtualAddress();
			desc.RayGenerationShaderRecord.SizeInBytes = pRayGenerationShaderTable->GetMemoryRequested();

			// The miss shaders are in the second SB section, right after ray generation record
			desc.MissShaderTable.StartAddress = pMissShaderTable->GetGpuVirtualAddress();
			desc.MissShaderTable.SizeInBytes = pMissShaderTable->GetMemoryRequested();
			desc.MissShaderTable.StrideInBytes = pMissShaderTable->GetStride();

			// The hit groups section start after the miss shaders
			desc.HitGroupTable.StartAddress = pHitGroupShaderTable->GetGpuVirtualAddress();
			desc.HitGroupTable.SizeInBytes = pHitGroupShaderTable->GetMemoryRequested();
			desc.HitGroupTable.StrideInBytes = pHitGroupShaderTable->GetStride();

			desc.Width = pOutput->GetWidth();
			desc.Height = pOutput->GetHeight();
			desc.Depth = 1;

			pCommandContext->SetRaytracingPipelineState(pRaytracingPipelineState);
			pCommandContext->DispatchRays(&desc);
			pCommandContext->UAVBarrier(pOutput);
			pCommandContext->TransitionBarrier(pOutput, Resource::State::PixelShaderResource);
		};
	},
		[](RenderPass<RaytracingRenderPassData>& This, UINT Width, UINT Height, RenderDevice* pRenderDevice)
	{
		// Destroy render target
		for (auto& output : This.Outputs)
		{
			pRenderDevice->Destroy(&output);
		}

		// Recreate render target
		This.Outputs[RaytracingRenderPassData::Outputs::RenderTarget] = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(RendererFormats::HDRBufferFormat);
			proxy.SetWidth(Width);
			proxy.SetHeight(Height);
			proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});

		const auto& uav = This.ResourceViews[1];
		pRenderDevice->CreateUAV(This.Outputs[RaytracingRenderPassData::Outputs::RenderTarget], uav[0], {}, {});
	});
}