#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

#include "Graphics/GpuBufferAllocator.h"
#include "Graphics/GpuTextureAllocator.h"

class RenderGraph;

struct CSMRenderPassData
{
	struct Outputs
	{
		enum
		{
			DepthBuffer
		};
	};

	struct ResourceViews
	{
		enum
		{
			DepthBuffer
		};
	};

	GpuBufferAllocator* pGpuBufferAllocator;
	GpuTextureAllocator* pGpuTextureAllocator;
	Buffer* pRenderPassConstantBuffer;
};

void AddCSMRenderPass(
	GpuBufferAllocator* pGpuBufferAllocator,
	GpuTextureAllocator* pGpuTextureAllocator,
	Buffer* pRenderPassConstantBuffer,
	RenderGraph* pRenderGraph)
{
	pRenderGraph->AddRenderPass<CSMRenderPassData>(
		RenderPassType::Graphics,
		[=](RenderPass<CSMRenderPassData>& This, RenderDevice* pRenderDevice)
	{
		auto handle = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [](TextureProxy& proxy)
		{
			proxy.SetFormat(DXGI_FORMAT_R32_TYPELESS);
			proxy.SetWidth(Resolutions::SunShadowMapResolution);
			proxy.SetHeight(Resolutions::SunShadowMapResolution);
			proxy.SetDepthOrArraySize(NUM_CASCADES);
			proxy.SetClearValue(CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0));
			proxy.BindFlags = Resource::BindFlags::DepthStencil;
			proxy.InitialState = Resource::State::DepthWrite;
		});

		auto resourceView = pRenderDevice->GetDescriptorAllocator().AllocateDepthStencilDescriptors(NUM_CASCADES);
		for (UINT i = 0; i < NUM_CASCADES; ++i)
		{
			Descriptor descriptor = resourceView[i];
			pRenderDevice->CreateDSV(handle, descriptor, i, {}, 1);
		}

		This.Outputs.push_back(handle);
		This.ResourceViews.push_back(resourceView);

		This.Data.pGpuBufferAllocator = pGpuBufferAllocator;
		This.Data.pGpuTextureAllocator = pGpuTextureAllocator;
		This.Data.pRenderPassConstantBuffer = pRenderPassConstantBuffer;
		return [](const RenderPass<CSMRenderPassData>& This, const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
		{
			std::vector<const Model*> visibleModelsIndices;
			std::unordered_map<const Model*, std::vector<UINT>> visibleMeshesIndices;
			visibleModelsIndices.resize(Scene.Models.size());
			for (const auto& model : Scene.Models)
			{
				visibleMeshesIndices[&model].resize(model.Meshes.size());
			}

			// Begin rendering
			PIXMarker(pCommandContext->GetD3DCommandList(), L"CSM");
			auto pOutput = RenderGraphRegistry.GetTexture(This.Outputs[CSMRenderPassData::Outputs::DepthBuffer]);

			pCommandContext->TransitionBarrier(pOutput, Resource::State::DepthWrite);

			pCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandContext->SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::Shadow));
			pCommandContext->SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::Shadow));

			This.Data.pGpuBufferAllocator->Bind(pCommandContext);

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.Width = vp.Height = Resolutions::SunShadowMapResolution;

			D3D12_RECT sr;
			sr.left = sr.top = 0;
			sr.right = sr.bottom = Resolutions::SunShadowMapResolution;

			pCommandContext->SetViewports(1, &vp);
			pCommandContext->SetScissorRects(1, &sr);

			for (UINT cascadeIndex = 0; cascadeIndex < NUM_CASCADES; ++cascadeIndex)
			{
				wchar_t msg[32]; swprintf(msg, 32, L"CSM %u", cascadeIndex);
				PIXMarker(pCommandContext->GetD3DCommandList(), msg);

				const OrthographicCamera& CascadeCamera = Scene.CascadeCameras[cascadeIndex];
				pCommandContext->SetGraphicsRootConstantBufferView(RootParameters::StandardShaderLayout::RenderPassDataCB, This.Data.pRenderPassConstantBuffer->GetGpuVirtualAddressAt(cascadeIndex));

				Descriptor descriptor = This.ResourceViews[CSMRenderPassData::ResourceViews::DepthBuffer][cascadeIndex];
				pCommandContext->SetRenderTargets(0, Descriptor(), FALSE, descriptor);
				pCommandContext->ClearDepthStencil(descriptor, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

				const UINT numVisibleModels = CullModelsOrthographic(CascadeCamera, true, Scene.Models, visibleModelsIndices);
				for (UINT i = 0; i < numVisibleModels; ++i)
				{
					auto& pModel = visibleModelsIndices[i];
					const UINT numVisibleMeshes = CullMeshesOrthographic(CascadeCamera, true, pModel->Meshes, visibleMeshesIndices[pModel]);

					for (UINT j = 0; j < numVisibleMeshes; ++j)
					{
						const UINT meshIndex = visibleMeshesIndices[pModel][j];
						auto& mesh = pModel->Meshes[meshIndex];

						pCommandContext->SetGraphicsRootConstantBufferView(RootParameters::StandardShaderLayout::ConstantDataCB,
							This.Data.pGpuBufferAllocator->GetConstantBuffer()->GetGpuVirtualAddressAt(mesh.ObjectConstantsIndex));
						pCommandContext->DrawIndexedInstanced(mesh.IndexCount, 1, mesh.StartIndexLocation, mesh.BaseVertexLocation, 0);
					}
				}
			}

			pCommandContext->TransitionBarrier(pOutput, Resource::State::DepthRead);
		};
	},
		[=](RenderPass<CSMRenderPassData>& This, UINT Width, UINT Height, RenderDevice* pRenderDevice)
	{
	});
}