#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

#include "Graphics/GpuBufferAllocator.h"
#include "Graphics/GpuTextureAllocator.h"

#include "CSM.h"

class RenderGraph;

struct ForwardRenderingRenderPassData
{
	struct Outputs
	{
		enum
		{
			RenderTarget,
			DepthStencil
		};
	};

	struct ResourceViews
	{
		enum
		{
			RenderTarget,
			DepthStencil
		};
	};

	GpuBufferAllocator* pGpuBufferAllocator;
	GpuTextureAllocator* pGpuTextureAllocator;
	Buffer* pRenderPassConstantBuffer;
	Texture* pShadowDepthBuffer;
};

void AddForwardRenderingRenderPass(
	UINT Width, UINT Height,
	GpuBufferAllocator* pGpuBufferAllocator,
	GpuTextureAllocator* pGpuTextureAllocator,
	Buffer* pRenderPassConstantBuffer,
	RenderGraph* pRenderGraph)
{
	auto pCSMRenderPass = pRenderGraph->GetRenderPass<CSMRenderPassData>();

	pRenderGraph->AddRenderPass<ForwardRenderingRenderPassData>(
		RenderPassType::Graphics,
		[=](RenderPass<ForwardRenderingRenderPassData>& This, RenderDevice* pRenderDevice)
	{
		auto renderTarget = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(RendererFormats::HDRBufferFormat);
			proxy.SetWidth(Width);
			proxy.SetHeight(Height);
			proxy.SetClearValue(CD3DX12_CLEAR_VALUE(RendererFormats::HDRBufferFormat, DirectX::Colors::LightBlue));
			proxy.BindFlags = Resource::BindFlags::RenderTarget;
			proxy.InitialState = Resource::State::RenderTarget;
		});

		auto depthStencil = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(RendererFormats::DepthStencilFormat);
			proxy.SetWidth(Width);
			proxy.SetHeight(Height);
			proxy.SetClearValue(CD3DX12_CLEAR_VALUE(RendererFormats::DepthStencilFormat, 1.0f, 0));
			proxy.BindFlags = Resource::BindFlags::DepthStencil;
			proxy.InitialState = Resource::State::DepthWrite;
		});

		This.Outputs.push_back(renderTarget);
		This.Outputs.push_back(depthStencil);

		auto rtv = pRenderDevice->GetDescriptorAllocator()->AllocateRenderTargetDescriptors(1);
		auto dsv = pRenderDevice->GetDescriptorAllocator()->AllocateDepthStencilDescriptors(1);

		pRenderDevice->CreateRTV(renderTarget, rtv[0], {}, {}, {});
		pRenderDevice->CreateDSV(depthStencil, dsv[0], {}, {}, {});

		This.ResourceViews.push_back(std::move(rtv));
		This.ResourceViews.push_back(std::move(dsv));

		This.Data.pGpuBufferAllocator = pGpuBufferAllocator;
		This.Data.pGpuTextureAllocator = pGpuTextureAllocator;
		This.Data.pRenderPassConstantBuffer = pRenderPassConstantBuffer;
		return [](const RenderPass<ForwardRenderingRenderPassData>& This, const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
		{
			std::vector<const Model*> visibleModelsIndices;
			std::unordered_map<const Model*, std::vector<UINT>> visibleMeshesIndices;
			visibleModelsIndices.resize(Scene.Models.size());
			for (const auto& model : Scene.Models)
			{
				visibleMeshesIndices[&model].resize(model.Meshes.size());
			}

			PIXMarker(pCommandContext->GetD3DCommandList(), L"Forward Rendering");
			auto pRenderTarget = RenderGraphRegistry.GetTexture(This.Outputs[ForwardRenderingRenderPassData::Outputs::RenderTarget]);
			auto pDepthStencil = RenderGraphRegistry.GetTexture(This.Outputs[ForwardRenderingRenderPassData::Outputs::DepthStencil]);
			const auto& RenderTargetView = This.ResourceViews[ForwardRenderingRenderPassData::Outputs::RenderTarget];
			const auto& DepthStencilView = This.ResourceViews[ForwardRenderingRenderPassData::Outputs::DepthStencil];

			pCommandContext->TransitionBarrier(pRenderTarget, Resource::State::RenderTarget);
			pCommandContext->TransitionBarrier(pDepthStencil, Resource::State::DepthWrite);

			pCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandContext->SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::PBR));
			pCommandContext->SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::PBR));

			This.Data.pGpuBufferAllocator->Bind(pCommandContext);
			This.Data.pGpuTextureAllocator->Bind(RootParameters::PBR::MaterialTextureIndicesSBuffer, RootParameters::PBR::MaterialTexturePropertiesSBuffer, pCommandContext);
			pCommandContext->SetGraphicsRootConstantBufferView(RootParameters::StandardShaderLayout::RenderPassDataCB + RootParameters::PBR::NumRootParameters,
				This.Data.pRenderPassConstantBuffer->GetGpuVirtualAddressAt(NUM_CASCADES));
			pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::StandardShaderLayout::DescriptorTables + RootParameters::PBR::NumRootParameters,
				RenderGraphRegistry.GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart());

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.Width = pRenderTarget->GetWidth();
			vp.Height = pRenderTarget->GetHeight();

			D3D12_RECT sr;
			sr.left = sr.top = 0;
			sr.right = pRenderTarget->GetWidth();
			sr.bottom = pRenderTarget->GetHeight();

			pCommandContext->SetViewports(1, &vp);
			pCommandContext->SetScissorRects(1, &sr);

			pCommandContext->SetRenderTargets(1, RenderTargetView[0], TRUE, DepthStencilView[0]);
			pCommandContext->ClearRenderTarget(RenderTargetView[0], DirectX::Colors::LightBlue, 0, nullptr);
			pCommandContext->ClearDepthStencil(DepthStencilView[0], D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

			const UINT numVisibleModels = CullModels(&Scene.Camera, Scene.Models, visibleModelsIndices);
			for (UINT i = 0; i < numVisibleModels; ++i)
			{
				auto& pModel = visibleModelsIndices[i];
				const UINT numVisibleMeshes = CullMeshes(&Scene.Camera, pModel->Meshes, visibleMeshesIndices[pModel]);

				for (UINT j = 0; j < numVisibleMeshes; ++j)
				{
					const UINT meshIndex = visibleMeshesIndices[pModel][j];
					auto& mesh = pModel->Meshes[meshIndex];

					pCommandContext->SetGraphicsRootConstantBufferView(RootParameters::StandardShaderLayout::ConstantDataCB + RootParameters::PBR::NumRootParameters,
						This.Data.pGpuBufferAllocator->GetConstantBuffer()->GetGpuVirtualAddressAt(mesh.ObjectConstantsIndex));
					pCommandContext->DrawIndexedInstanced(mesh.IndexCount, 1, mesh.StartIndexLocation, mesh.BaseVertexLocation, 0);
				}
			}

			// Skybox
			pCommandContext->SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::Skybox));
			pCommandContext->SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::Skybox));

			pCommandContext->SetGraphicsRootConstantBufferView(RootParameters::StandardShaderLayout::RenderPassDataCB, This.Data.pRenderPassConstantBuffer->GetGpuVirtualAddressAt(NUM_CASCADES));
			pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::StandardShaderLayout::DescriptorTables, RenderGraphRegistry.GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart());

			pCommandContext->DrawIndexedInstanced(Scene.Skybox.Mesh.IndexCount, 1, Scene.Skybox.Mesh.StartIndexLocation, Scene.Skybox.Mesh.BaseVertexLocation, 0);

			pCommandContext->TransitionBarrier(pRenderTarget, Resource::State::PixelShaderResource);
			pCommandContext->TransitionBarrier(pDepthStencil, Resource::State::DepthRead);
		};
	},
		[](RenderPass<ForwardRenderingRenderPassData>& This, UINT Width, UINT Height, RenderDevice* pRenderDevice)
	{
		// Destroy render target and depth stencil
		for (auto& output : This.Outputs)
		{
			pRenderDevice->Destroy(&output);
		}

		// Recreate render target and depth stencil
		This.Outputs[ForwardRenderingRenderPassData::Outputs::RenderTarget] = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(RendererFormats::HDRBufferFormat);
			proxy.SetWidth(Width);
			proxy.SetHeight(Height);
			proxy.SetClearValue(CD3DX12_CLEAR_VALUE(RendererFormats::HDRBufferFormat, DirectX::Colors::LightBlue));
			proxy.BindFlags = Resource::BindFlags::RenderTarget;
			proxy.InitialState = Resource::State::RenderTarget;
		});

		This.Outputs[ForwardRenderingRenderPassData::Outputs::DepthStencil] = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(RendererFormats::DepthStencilFormat);
			proxy.SetWidth(Width);
			proxy.SetHeight(Height);
			proxy.SetClearValue(CD3DX12_CLEAR_VALUE(RendererFormats::DepthStencilFormat, 1.0f, 0));
			proxy.BindFlags = Resource::BindFlags::DepthStencil;
			proxy.InitialState = Resource::State::DepthWrite;
		});

		// Recreate descriptors
		const auto& RenderTargetView = This.ResourceViews[ForwardRenderingRenderPassData::Outputs::RenderTarget];
		const auto& DepthStencilView = This.ResourceViews[ForwardRenderingRenderPassData::Outputs::DepthStencil];

		pRenderDevice->CreateRTV(This.Outputs[ForwardRenderingRenderPassData::Outputs::RenderTarget], RenderTargetView[0], {}, {}, {});
		pRenderDevice->CreateDSV(This.Outputs[ForwardRenderingRenderPassData::Outputs::DepthStencil], DepthStencilView[0], {}, {}, {});
	});
}