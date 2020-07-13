#include "pch.h"
#include "Renderer.h"
#include "../Core/Application.h"
#include "../Core/Window.h"
#include "../Core/Time.h"

// MT
#include <future>

using namespace DirectX;

Renderer::Renderer(Application& RefApplication, Window& RefWindow)
	: m_RefWindow(RefWindow),
	m_RenderDevice(m_DXGIManager.QueryAdapter(API::API_D3D12).Get()),
	m_RenderGraph(m_RenderDevice),
	m_GpuBufferAllocator(256_MiB, 256_MiB, 256_MiB, m_RenderDevice),
	m_GpuTextureAllocator(m_RenderDevice)
{
	m_EventReceiver.Register(m_RefWindow, [&](const Event& event)
	{
		Window::Event windowEvent;
		event.Read(windowEvent.type, windowEvent.data);
		switch (windowEvent.type)
		{
		case Window::Event::Type::Maximize:
		case Window::Event::Type::Resize:
			Resize();
			break;
		}
	});

	m_AspectRatio = static_cast<float>(m_RefWindow.GetWindowWidth()) / static_cast<float>(m_RefWindow.GetWindowHeight());

	Shaders::Register(&m_RenderDevice);
	RootSignatures::Register(&m_RenderDevice);
	GraphicsPSOs::Register(&m_RenderDevice);
	ComputePSOs::Register(&m_RenderDevice);

	// Create swap chain after command objects have been created
	m_pSwapChain = m_DXGIManager.CreateSwapChain(m_RenderDevice.GetGraphicsQueue()->GetD3DCommandQueue(), m_RefWindow, RendererFormats::SwapChainBufferFormat, SwapChainBufferCount);

	// Initialize Non-transient resources
	for (UINT i = 0; i < SwapChainBufferCount; ++i)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffer;
		ThrowCOMIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
		m_BackBufferTexture[i] = Texture(pBackBuffer);
		m_RenderDevice.GetGlobalResourceStateTracker().AddResourceState(m_BackBufferTexture[i].GetD3DResource(), D3D12_RESOURCE_STATE_COMMON);
	}

	Texture::Properties textureProp{};
	textureProp.Type = Resource::Type::Texture2D;
	textureProp.Format = RendererFormats::HDRBufferFormat;
	textureProp.Width = m_RefWindow.GetWindowWidth();
	textureProp.Height = m_RefWindow.GetWindowHeight();
	textureProp.DepthOrArraySize = 1;
	textureProp.MipLevels = 1;
	textureProp.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	textureProp.InitialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	textureProp.pOptimizedClearValue = &CD3DX12_CLEAR_VALUE(RendererFormats::HDRBufferFormat, DirectX::Colors::LightBlue);
	m_FrameBuffer = m_RenderDevice.CreateTexture(textureProp);

	textureProp.Format = RendererFormats::DepthStencilFormat;
	textureProp.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	textureProp.InitialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	textureProp.pOptimizedClearValue = &CD3DX12_CLEAR_VALUE(RendererFormats::DepthStencilFormat, 1.0f, 0);
	m_FrameDepthStencilBuffer = m_RenderDevice.CreateTexture(textureProp);

	Buffer::Properties bufferProp{};
	bufferProp.SizeInBytes = Math::AlignUp<UINT64>(sizeof(RenderPassConstantsCPU), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	bufferProp.Stride = Math::AlignUp<UINT64>(sizeof(RenderPassConstantsCPU), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	m_RenderPassCBs = m_RenderDevice.CreateBuffer(bufferProp, CPUAccessibleHeapType::Upload, nullptr);

	struct SceneStagingData
	{
		GpuBufferAllocator* pGpuBufferAllocator;
		GpuTextureAllocator* pGpuTextureAllocator;
	};

	auto pSceneStagingPass = m_RenderGraph.AddRenderPass<RenderPassType::Graphics, SceneStagingData>(
		[&](SceneStagingData& Data, RenderDevice& RenderDevice)
	{
		Data.pGpuBufferAllocator = &m_GpuBufferAllocator;
		Data.pGpuTextureAllocator = &m_GpuTextureAllocator;

		return [=](const SceneStagingData& Data, Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* pRenderCommandContext)
		{
			Data.pGpuBufferAllocator->Stage(Scene, pRenderCommandContext);
			Data.pGpuBufferAllocator->Bind(pRenderCommandContext);
			Data.pGpuBufferAllocator->Update(Scene);
			Data.pGpuTextureAllocator->Stage(Scene, pRenderCommandContext);

			m_RenderGraph.GetRenderPass<RenderPassType::Graphics, SceneStagingData>()->Enabled = false;
		};
	});

	struct ShadowPassData
	{
		GpuBufferAllocator* pGpuBufferAllocator;
		GpuTextureAllocator* pGpuTextureAllocator;

		enum { NumCascades = 4 };

		UINT resolution;
		float lambda;

		RenderResourceHandle uploadBuffer;
		RenderResourceHandle outputDepthTexture;
		Descriptor depthStencilView;
		Cascade cascades[NumCascades];
	};

	auto pShadowPass = m_RenderGraph.AddRenderPass<RenderPassType::Graphics, ShadowPassData>(
		[&](ShadowPassData& Data, RenderDevice& RenderDevice)
	{
		Data.pGpuBufferAllocator = &m_GpuBufferAllocator;
		Data.pGpuTextureAllocator = &m_GpuTextureAllocator;

		Data.resolution = 2048;
		Data.lambda = 0.5f;

		struct ShadowPassConstantsCPU
		{
			DirectX::XMFLOAT4X4 ViewProjection;
		};
		Buffer::Properties bufferProp{};
		bufferProp.SizeInBytes = ShadowPassData::NumCascades * Math::AlignUp<UINT64>(sizeof(ShadowPassConstantsCPU), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		bufferProp.Stride = Math::AlignUp<UINT64>(sizeof(ShadowPassConstantsCPU), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		Data.uploadBuffer = RenderDevice.CreateBuffer(bufferProp, CPUAccessibleHeapType::Upload);
		Buffer* pUploadBuffer = RenderDevice.GetBuffer(Data.uploadBuffer);
		pUploadBuffer->Map();

		Texture::Properties textureProp{};
		textureProp.Type = Resource::Type::Texture2D;
		textureProp.Format = DXGI_FORMAT_R32_TYPELESS;
		textureProp.Width = textureProp.Height = Data.resolution;
		textureProp.DepthOrArraySize = ShadowPassData::NumCascades;
		textureProp.MipLevels = 1;
		textureProp.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		textureProp.IsCubemap = false;
		textureProp.pOptimizedClearValue = &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);
		textureProp.InitialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		Data.outputDepthTexture = RenderDevice.CreateTexture(textureProp);
		Data.depthStencilView = RenderDevice.GetDevice().GetDescriptorAllocator<D3D12_DEPTH_STENCIL_VIEW_DESC>().Allocate(ShadowPassData::NumCascades);
		for (UINT i = 0; i < ShadowPassData::NumCascades; ++i)
		{
			RenderDevice.CreateDSV(Data.outputDepthTexture, Data.depthStencilView[i], i, {}, 1);
		}

		return [=](const ShadowPassData& Data, Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* pRenderCommandContext)
		{
			using namespace DirectX;
			Buffer* pUploadBuffer = RenderGraphRegistry.GetBuffer(Data.uploadBuffer);

			// Calculate cascade
			const float MaxDistance = 1.0f;
			const float MinDistance = 0.0f;
			OrthographicCamera CascadeCameras[ShadowPassData::NumCascades];

			float cascadeSplits[ShadowPassData::NumCascades] = { 0.0f, 0.0f, 0.0f, 0.0f };

			if (const PerspectiveCamera* pPerspectiveCamera = dynamic_cast<const PerspectiveCamera*>(&Scene.Camera))
			{
				float nearClip = pPerspectiveCamera->NearZ();
				float farClip = pPerspectiveCamera->FarZ();
				float clipRange = farClip - nearClip;

				float minZ = nearClip + MinDistance * clipRange;
				float maxZ = nearClip + MaxDistance * clipRange;

				float range = maxZ - minZ;
				float ratio = maxZ / minZ;

				for (UINT i = 0; i < ShadowPassData::NumCascades; ++i)
				{
					float p = (i + 1) / static_cast<float>(ShadowPassData::NumCascades);
					float log = minZ * std::pow(ratio, p);
					float uniform = minZ + range * p;
					float d = Data.lambda * (log - uniform) + uniform;
					cascadeSplits[i] = (d - nearClip) / clipRange;
				}
			}
			else
			{
				for (UINT i = 0; i < ShadowPassData::NumCascades; ++i)
				{
					cascadeSplits[i] = Math::Lerp(MinDistance, MaxDistance, (i + 1.0f) / float(ShadowPassData::NumCascades));
				}
			}

			// Calculate projection matrix for each cascade
			for (UINT cascadeIndex = 0; cascadeIndex < ShadowPassData::NumCascades; ++cascadeIndex)
			{
				XMVECTOR frustumCornersWS[8] =
				{
					XMVectorSet(-1.0f, +1.0f, 0.0f, 1.0f),	// Near top left
					XMVectorSet(+1.0f, +1.0f, 0.0f, 1.0f),	// Near top right
					XMVectorSet(+1.0f, -1.0f, 0.0f, 1.0f),	// Near bottom right
					XMVectorSet(-1.0f, -1.0f, 0.0f, 1.0f),	// Near bottom left
					XMVectorSet(-1.0f, +1.0f, 1.0f, 1.0f),  // Far top left
					XMVectorSet(+1.0f, +1.0f, 1.0f, 1.0f),	// Far top right
					XMVectorSet(+1.0f, -1.0f, 1.0f, 1.0f),	// Far bottom right
					XMVectorSet(-1.0f, -1.0f, 1.0f, 1.0f)	// Far bottom left
				};

				// Used for unprojecting
				XMMATRIX invCameraViewProj = XMMatrixInverse(nullptr, Scene.Camera.ViewProjectionMatrix());
				for (UINT i = 0; i < 8u; ++i)
					frustumCornersWS[i] = XMVector3TransformCoord(frustumCornersWS[i], invCameraViewProj);

				float prevCascadeSplit = cascadeIndex == 0 ? 0.0f : cascadeSplits[cascadeIndex - 1];
				float cascadeSplit = cascadeSplits[cascadeIndex];

				// Calculate frustum corners of current cascade
				for (UINT i = 0; i < 4; ++i)
				{
					XMVECTOR cornerRay = frustumCornersWS[i + 4] - frustumCornersWS[i];
					XMVECTOR nearCornerRay = cornerRay * prevCascadeSplit;
					XMVECTOR farCornerRay = cornerRay * cascadeSplit;
					frustumCornersWS[i + 4] = frustumCornersWS[i] + farCornerRay;
					frustumCornersWS[i] = frustumCornersWS[i] + nearCornerRay;
				}

				XMVECTOR vCenter = XMVectorZero();
				for (UINT i = 0; i < 8; ++i)
					vCenter += frustumCornersWS[i];
				vCenter *= (1.0f / 8.0f);

				XMVECTOR vRadius = XMVectorZero();
				for (UINT i = 0; i < 8; ++i)
				{
					XMVECTOR distance = XMVector3Length(frustumCornersWS[i] - vCenter);
					vRadius = XMVectorMax(vRadius, distance);
				}
				float radius = std::ceilf(XMVectorGetX(vRadius) * 16.0f) / 16.0f;
				float scaledRadius = radius * ((static_cast<float>(Data.resolution) + 7.0f) / static_cast<float>(Data.resolution));

				// Negate direction
				XMVECTOR lightPos = vCenter + (-XMLoadFloat3(&Scene.DirectionalLight.Direction) * radius);

				CascadeCameras[cascadeIndex].SetLens(-scaledRadius, +scaledRadius, -scaledRadius, +scaledRadius, 0.0f, radius * 2.0f);
				CascadeCameras[cascadeIndex].SetLookAt(lightPos, vCenter, Math::Up);

				// Create the rounding matrix, by projecting the world-space origin and determining
				// the fractional offset in texel space
				DirectX::XMMATRIX shadowMatrix = CascadeCameras[cascadeIndex].ViewProjectionMatrix();
				DirectX::XMVECTOR shadowOrigin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
				shadowOrigin = DirectX::XMVector4Transform(shadowOrigin, shadowMatrix);
				shadowOrigin = DirectX::XMVectorScale(shadowOrigin, static_cast<float>(Data.resolution) * 0.5f);

				DirectX::XMVECTOR roundedOrigin = DirectX::XMVectorRound(shadowOrigin);
				DirectX::XMVECTOR roundOffset = DirectX::XMVectorSubtract(roundedOrigin, shadowOrigin);
				roundOffset = DirectX::XMVectorScale(roundOffset, 2.0f / static_cast<float>(Data.resolution));
				roundOffset = DirectX::XMVectorSetZ(roundOffset, 0.0f);
				roundOffset = DirectX::XMVectorSetW(roundOffset, 0.0f);

				DirectX::XMMATRIX shadowProj = CascadeCameras[cascadeIndex].ProjectionMatrix();
				shadowProj.r[3] = DirectX::XMVectorAdd(shadowProj.r[3], roundOffset);
				CascadeCameras[cascadeIndex].SetProjection(shadowProj);

				// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
				XMMATRIX T(0.5f, 0.0f, 0.0f, 0.0f,
					0.0f, -0.5f, 0.0f, 0.0f,
					0.0f, 0.0f, 1.0f, 0.0f,
					0.5f, 0.5f, 0.0f, 1.0f);
				XMMATRIX lightViewProj = CascadeCameras[cascadeIndex].ViewProjectionMatrix();
				XMMATRIX shadowTransform = lightViewProj * T;
				const float clipDist = Scene.Camera.FarZ() - Scene.Camera.NearZ();

				auto& mutableData = const_cast<ShadowPassData&>(Data);
				XMStoreFloat4x4(&mutableData.cascades[cascadeIndex].ShadowTransform, shadowTransform);
				mutableData.cascades[cascadeIndex].Split = Scene.Camera.NearZ() + cascadeSplit * clipDist;
			}

			// Update cpu buffer
			for (UINT cascadeIndex = 0; cascadeIndex < ShadowPassData::NumCascades; ++cascadeIndex)
			{
				ShadowPassConstantsCPU cascadeShadowPass;
				XMStoreFloat4x4(&cascadeShadowPass.ViewProjection, XMMatrixTranspose(CascadeCameras[cascadeIndex].ViewProjectionMatrix()));

				pUploadBuffer->Update<ShadowPassConstantsCPU>(cascadeIndex, cascadeShadowPass);
			}

			// Begin rendering
			PIXMarker(pRenderCommandContext->GetD3DCommandList(), L"Scene Cascade Shadow Map Render");

			pRenderCommandContext->SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::Shadow));
			pRenderCommandContext->SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::Shadow));

			Data.pGpuBufferAllocator->Bind(pRenderCommandContext);
			pRenderCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pRenderCommandContext->TransitionBarrier(RenderGraphRegistry.GetTexture(Data.outputDepthTexture), D3D12_RESOURCE_STATE_DEPTH_WRITE);

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.Width = vp.Height = Data.resolution;

			D3D12_RECT sr;
			sr.left = sr.top = 0;
			sr.right = sr.bottom = Data.resolution;

			pRenderCommandContext->SetViewports(1, &vp);
			pRenderCommandContext->SetScissorRects(1, &sr);

			for (UINT cascadeIndex = 0; cascadeIndex < ShadowPassData::NumCascades; ++cascadeIndex)
			{
				wchar_t msg[32]; swprintf(msg, 32, L"Cascade Shadow Map %u", cascadeIndex);
				PIXMarker(pRenderCommandContext->GetD3DCommandList(), msg);

				const OrthographicCamera& CascadeCamera = CascadeCameras[cascadeIndex];
				pRenderCommandContext->SetGraphicsRootCBV(1, pUploadBuffer->GetGpuVAAt(cascadeIndex));

				pRenderCommandContext->SetRenderTargets(0, 0, Descriptor(), FALSE, cascadeIndex, Data.depthStencilView);
				pRenderCommandContext->ClearDepthStencil(cascadeIndex, Data.depthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
					1.0f, 0, 0, nullptr);

				const UINT numVisibleModels = CullModelsOrthographic(CascadeCamera, true, Scene.Models, m_VisibleModelsIndices);
				for (UINT i = 0; i < numVisibleModels; ++i)
				{
					auto& pModel = m_VisibleModelsIndices[i];
					const UINT numVisibleMeshes = CullMeshesOrthographic(CascadeCamera, true, pModel->Meshes, m_VisibleMeshesIndices[pModel]);

					for (UINT j = 0; j < numVisibleMeshes; ++j)
					{
						const UINT meshIndex = m_VisibleMeshesIndices[pModel][j];
						auto& mesh = pModel->Meshes[meshIndex];

						pRenderCommandContext->SetGraphicsRootCBV(0, Data.pGpuBufferAllocator->GetConstantBuffer()->GetGpuVAAt(mesh.ConstantBufferIndex));
						pRenderCommandContext->DrawIndexedInstanced(mesh.IndexCount, 1, mesh.StartIndexLocation, mesh.BaseVertexLocation, 0);
					}
				}
			}
			pRenderCommandContext->TransitionBarrier(RenderGraphRegistry.GetTexture(Data.outputDepthTexture), D3D12_RESOURCE_STATE_DEPTH_READ);
		};
	});

	//struct ForwardPassData
	//{

	//};

	//auto pForwardPass = m_RenderGraph.AddRenderPass<RenderPassType::Graphics, ForwardPassData>(
	//	[&](ForwardPassData& Data, RenderDevice& RenderDevice)
	//{
	//	return [=](const ForwardPassData& Data, Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* pRenderCommandContext)
	//	{
	//	};
	//});

	m_RenderGraph.Setup();
}

Renderer::~Renderer()
{
}

void Renderer::SetScene(Scene* pScene)
{
	m_pScene = pScene;
	m_pScene->Camera.SetAspectRatio(m_AspectRatio);
	m_VisibleModelsIndices.resize(m_pScene->Models.size());
	for (const auto& model : m_pScene->Models)
	{
		m_VisibleMeshesIndices[&model].resize(model.Meshes.size());
	}
}

void Renderer::TEST()
{
	PIXCapture();
	auto pRenderCommandContext = m_RenderDevice.AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
	m_GpuBufferAllocator.Stage(*m_pScene, pRenderCommandContext);
	m_GpuBufferAllocator.Bind(pRenderCommandContext);
	m_GpuBufferAllocator.Update(*m_pScene);
	m_GpuTextureAllocator.Stage(*m_pScene, pRenderCommandContext);
	m_RenderDevice.ExecuteRenderCommandContexts(1, &pRenderCommandContext);
}

void Renderer::Update(const Time& Time)
{
	++m_Statistics.TotalFrameCount;
	++m_Statistics.FrameCount;
	if (Time.TotalTime() - m_Statistics.TimeElapsed >= 1.0)
	{
		m_Statistics.FPS = static_cast<DOUBLE>(m_Statistics.FrameCount);
		m_Statistics.FPMS = 1000.0 / m_Statistics.FPS;
		m_Statistics.FrameCount = 0;
		m_Statistics.TimeElapsed += 1.0;
	}
}

void Renderer::Render()
{
	PIXCapture();
	m_RenderGraph.Execute(*m_pScene);
	m_RenderGraph.ThreadBarrier();
	m_RenderGraph.ExecuteCommandContexts();
}

void Renderer::Present()
{
	UINT syncInterval = m_Setting.VSync ? 1u : 0u;
	UINT presentFlags = (m_DXGIManager.TearingSupport() && !m_Setting.VSync) ? DXGI_PRESENT_ALLOW_TEARING : 0u;
	DXGI_PRESENT_PARAMETERS presentParameters = { 0u, NULL, NULL, NULL };
	HRESULT hr = m_pSwapChain->Present1(syncInterval, presentFlags, &presentParameters);
	if (hr == DXGI_ERROR_DEVICE_REMOVED)
	{
		CORE_ERROR("DXGI_ERROR_DEVICE_REMOVED");
		Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData> pDRED;
		ThrowCOMIfFailed(m_RenderDevice.GetDevice().GetD3DDevice()->QueryInterface(IID_PPV_ARGS(&pDRED)));
		D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DREDAutoBreadcrumbsOutput;
		D3D12_DRED_PAGE_FAULT_OUTPUT DREDPageFaultOutput;
		ThrowCOMIfFailed(pDRED->GetAutoBreadcrumbsOutput(&DREDAutoBreadcrumbsOutput));
		ThrowCOMIfFailed(pDRED->GetPageFaultAllocationOutput(&DREDPageFaultOutput));
	}
}

Texture& Renderer::CurrentBackBuffer()
{
	return m_BackBufferTexture[m_pSwapChain->GetCurrentBackBufferIndex()];
}

void Renderer::Resize()
{
	m_RenderDevice.GetGraphicsQueue()->WaitForIdle();
	{
		m_AspectRatio = static_cast<float>(m_RefWindow.GetWindowWidth()) / static_cast<float>(m_RefWindow.GetWindowHeight());
		m_pScene->Camera.SetAspectRatio(m_AspectRatio);

		// Release resources before resize swap chain
		for (UINT i = 0u; i < SwapChainBufferCount; ++i)
			m_BackBufferTexture[i].Release();

		// Resize backbuffer
		UINT Nodes[SwapChainBufferCount];
		IUnknown* Queues[SwapChainBufferCount];
		for (UINT i = 0; i < SwapChainBufferCount; ++i)
		{
			Nodes[i] = NodeMask;
			Queues[i] = m_RenderDevice.GetGraphicsQueue()->GetD3DCommandQueue();
		}
		ThrowCOMIfFailed(m_pSwapChain->ResizeBuffers1(SwapChainBufferCount, m_RefWindow.GetWindowWidth(), m_RefWindow.GetWindowHeight(), RendererFormats::SwapChainBufferFormat,
			m_DXGIManager.TearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH, Nodes, Queues));
		// Recreate descriptors
		for (UINT i = 0; i < SwapChainBufferCount; ++i)
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffer;
			ThrowCOMIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
			m_BackBufferTexture[i] = Texture(pBackBuffer);
			m_RenderDevice.GetGlobalResourceStateTracker().AddResourceState(m_BackBufferTexture[i].GetD3DResource(), D3D12_RESOURCE_STATE_COMMON);
		}
	}
	// Wait until resize is complete.
	m_RenderDevice.GetGraphicsQueue()->WaitForIdle();
}

UINT Renderer::CullModels(const Camera* pCamera, const Scene::ModelList& Models, std::vector<const Model*>& Indices)
{
	DirectX::BoundingFrustum frustum(pCamera->ProjectionMatrix());
	frustum.Transform(frustum, pCamera->WorldMatrix());

	UINT numVisible = 0;
	for (auto iter = Models.begin(); iter != Models.end(); ++iter)
	{
		auto& model = (*iter);
		DirectX::BoundingBox aabb;
		model.BoundingBox.Transform(aabb, model.Transform.Matrix());
		if (frustum.Contains(aabb) != DirectX::ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = &(*iter);
		}
	}

	return numVisible;
}

UINT Renderer::CullModelsOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const Scene::ModelList& Models, std::vector<const Model*>& Indices)
{
	using namespace DirectX;

	DirectX::XMVECTOR mins = DirectX::XMVectorSet(Camera.ViewLeft(), Camera.ViewBottom(), Camera.NearZ(), 1.0f);
	DirectX::XMVECTOR maxes = DirectX::XMVectorSet(Camera.ViewRight(), Camera.ViewTop(), Camera.FarZ(), 1.0f);
	if (IgnoreNearZ)
	{
		mins = XMVectorSet(XMVectorGetX(mins), XMVectorGetY(mins), XMVectorGetX(g_XMNegInfinity.v), 1.0f);
	}

	DirectX::XMVECTOR extents = (maxes - mins) * 0.5f;
	DirectX::XMVECTOR center = mins + extents;
	center = XMVector3TransformCoord(center, XMMatrixRotationQuaternion(XMLoadFloat4(&Camera.GetTransform().Orientation)));
	center += XMLoadFloat3(&Camera.GetTransform().Position);

	DirectX::BoundingOrientedBox obb;
	XMStoreFloat3(&obb.Extents, extents);
	XMStoreFloat3(&obb.Center, center);
	obb.Orientation = Camera.GetTransform().Orientation;

	UINT numVisible = 0;
	for (auto iter = Models.begin(); iter != Models.end(); ++iter)
	{
		auto& model = (*iter);
		DirectX::BoundingBox aabb;
		model.BoundingBox.Transform(aabb, model.Transform.Matrix());
		if (obb.Contains(aabb) != DirectX::ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = &(*iter);
		}
	}

	return numVisible;
}

UINT Renderer::CullMeshes(const Camera* pCamera, const std::vector<Mesh>& Meshes, std::vector<UINT>& Indices)
{
	DirectX::BoundingFrustum frustum(pCamera->ProjectionMatrix());
	frustum.Transform(frustum, pCamera->WorldMatrix());

	UINT numVisible = 0;
	for (size_t i = 0, numMeshes = Meshes.size(); i < numMeshes; ++i)
	{
		auto& mesh = Meshes[i];
		DirectX::BoundingBox aabb;
		mesh.BoundingBox.Transform(aabb, mesh.Transform.Matrix());
		if (frustum.Contains(aabb) != DirectX::ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = i;
		}
	}

	return numVisible;
}

UINT Renderer::CullMeshesOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const std::vector<Mesh>& Meshes, std::vector<UINT>& Indices)
{
	using namespace DirectX;

	DirectX::XMVECTOR mins = DirectX::XMVectorSet(Camera.ViewLeft(), Camera.ViewBottom(), Camera.NearZ(), 1.0f);
	DirectX::XMVECTOR maxes = DirectX::XMVectorSet(Camera.ViewRight(), Camera.ViewTop(), Camera.FarZ(), 1.0f);
	if (IgnoreNearZ)
	{
		mins = XMVectorSet(XMVectorGetX(mins), XMVectorGetY(mins), XMVectorGetX(g_XMNegInfinity.v), 1.0f);
	}

	DirectX::XMVECTOR extents = (maxes - mins) / 2.0f;
	DirectX::XMVECTOR center = mins + extents;
	center = XMVector3TransformCoord(center, XMMatrixRotationQuaternion(XMLoadFloat4(&Camera.GetTransform().Orientation)));
	center += XMLoadFloat3(&Camera.GetTransform().Position);

	DirectX::BoundingOrientedBox obb;
	XMStoreFloat3(&obb.Extents, extents);
	XMStoreFloat3(&obb.Center, center);
	obb.Orientation = Camera.GetTransform().Orientation;

	UINT numVisible = 0;
	for (size_t i = 0, numMeshes = Meshes.size(); i < numMeshes; ++i)
	{
		auto& mesh = Meshes[i];
		DirectX::BoundingBox aabb;
		mesh.BoundingBox.Transform(aabb, mesh.Transform.Matrix());
		if (obb.Contains(aabb) != DirectX::ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = i;
		}
	}

	return numVisible;
}