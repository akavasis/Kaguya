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
			AccelerationStructure,
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
			auto pShaderTableBuffer = RenderGraphRegistry.GetBuffer(RaytracingPSOs::RaytracingShaderTableBuffer);
			auto pRaytracingPipelineState = RenderGraphRegistry.GetRaytracingPSO(RaytracingPSOs::Raytracing);
			auto& ShaderTable = pRaytracingPipelineState->GetShaderTable();

			pCommandContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

			pCommandContext->SetComputeRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::Raytracing::Global));
			
			pCommandContext->SetComputeRootDescriptorTable(RootParameters::Raytracing::GeometryTable, This.ResourceViews[0].GetStartDescriptor().GPUHandle);
			pCommandContext->SetComputeRootDescriptorTable(RootParameters::Raytracing::RenderTarget, This.ResourceViews[1].GetStartDescriptor().GPUHandle);
			pCommandContext->SetComputeRootConstantBufferView(RootParameters::Raytracing::Camera, This.Data.pRenderPassConstantBuffer->GetGpuVirtualAddressAt(0));

			// Setup the raytracing task
			D3D12_DISPATCH_RAYS_DESC desc = {};
			// The layout of the SB is as follows: ray generation shader, miss
			// shaders, hit groups. all SB entries of a given type have the same size to allow a fixed stride.

			// The ray generation shaders are always at the beginning of the ST. 
			UINT64 rayGenerationShaderRecordSizeInBytes = ShaderTable.GetShaderRecordSize(ShaderTable::RayGeneration);
			desc.RayGenerationShaderRecord.StartAddress = pShaderTableBuffer->GetGpuVirtualAddress();
			desc.RayGenerationShaderRecord.SizeInBytes = rayGenerationShaderRecordSizeInBytes;

			// The miss shaders are in the second SB section, right after ray generation record
			UINT64 missShaderTableSizeInBytes = ShaderTable.GetShaderRecordSize(ShaderTable::Miss);
			UINT64 missShaderTableStride = ShaderTable.GetShaderRecordStride(ShaderTable::Miss);
			desc.MissShaderTable.StartAddress = pShaderTableBuffer->GetGpuVirtualAddress() + rayGenerationShaderRecordSizeInBytes;
			desc.MissShaderTable.SizeInBytes = missShaderTableSizeInBytes;
			desc.MissShaderTable.StrideInBytes = missShaderTableStride;

			// The hit groups section start after the miss shaders
			UINT64 hitGroupShaderTableSizeInBytes = ShaderTable.GetShaderRecordSize(ShaderTable::HitGroup);
			UINT64 hitGroupShaderTableStride = ShaderTable.GetShaderRecordStride(ShaderTable::HitGroup);
			desc.HitGroupTable.StartAddress = pShaderTableBuffer->GetGpuVirtualAddress() + rayGenerationShaderRecordSizeInBytes + missShaderTableSizeInBytes;
			desc.HitGroupTable.SizeInBytes = hitGroupShaderTableSizeInBytes;
			desc.HitGroupTable.StrideInBytes = hitGroupShaderTableStride;

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