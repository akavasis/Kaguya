#include "pch.h"
#include "Renderer.h"
#include "../Core/Window.h"
#include "../Core/Time.h"

// MT
#include <future>

#include "pix3.h"
struct PIXMarker
{
	ID3D12GraphicsCommandList* pCommandList;
	PIXMarker(ID3D12GraphicsCommandList* pCommandList, LPCWSTR pMsg)
		: pCommandList(pCommandList)
	{
		PIXBeginEvent(pCommandList, 0, pMsg);
	}
	~PIXMarker()
	{
		PIXEndEvent(pCommandList);
	}
};

using Microsoft::WRL::ComPtr;
using namespace DirectX;

#define MAX_POINT_LIGHT 5
#define MAX_SPOT_LIGHT 5
struct LightSettings
{
	enum
	{
		NumPointLights = 1,
		NumSpotLights = 0
	};
	static_assert(NumPointLights <= MAX_POINT_LIGHT);
	static_assert(NumSpotLights <= MAX_SPOT_LIGHT);
};

struct ObjectConstantsCPU
{
	DirectX::XMFLOAT4X4 World;
};

struct MaterialDataCPU
{
	DirectX::XMFLOAT3 Albedo;
	float Metallic;
	DirectX::XMFLOAT3 Emissive;
	float Roughness;
	int Flags;
	DirectX::XMFLOAT3 _padding;
};

struct RenderPassConstantsCPU
{
	DirectX::XMFLOAT4X4 ViewProjection;
	DirectX::XMFLOAT3 EyePosition;
	float _padding;

	int NumPointLights;
	int NumSpotLights;
	DirectX::XMFLOAT2 _padding2;
	DirectionalLight DirectionalLight;
	PointLight PointLights[MAX_POINT_LIGHT];
	SpotLight SpotLights[MAX_SPOT_LIGHT];
	Cascade Cascades[4];
	int VisualizeCascade;
	int DebugViewInput;
	DirectX::XMFLOAT2 _padding3;
};
//constexpr UINT RenderPassConstantsCPUSize = Math::AlignUp<UINT>(sizeof(RenderPassConstantsCPU), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

Renderer::Renderer(Window& Window)
	: m_Window(Window),
	m_RenderDevice(m_DXGIManager.QueryAdapter(API::API_D3D12)),
	m_RenderGraph(m_RenderDevice)
{
	m_EventReceiver.Register(m_Window, [&](const Event& event)
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

	m_AspectRatio = static_cast<float>(m_Window.GetWindowWidth()) / static_cast<float>(m_Window.GetWindowHeight());

	std::future<void> voidFuture = std::async(std::launch::async, &Renderer::RegisterRendererResources, this);
	// Create swap chain after command objects have been created
	m_pSwapChain = m_DXGIManager.CreateSwapChain(m_RenderDevice.GetGraphicsQueue()->GetD3DCommandQueue(), m_Window, RendererFormats::SwapChainBufferFormat, SwapChainBufferCount);

	// Init imgui
	m_ImGuiManager.Initialize();

	// Initialize Non-transient resources
	for (UINT i = 0; i < SwapChainBufferCount; ++i)
	{
		ComPtr<ID3D12Resource> pBackBuffer;
		ThrowCOMIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
		m_BackBufferTexture[i] = Texture(pBackBuffer);
		m_RenderDevice.GetGlobalResourceStateTracker().AddResourceState(m_BackBufferTexture[i].GetD3DResource(), D3D12_RESOURCE_STATE_COMMON);
	}

	Texture::Properties textureProp{};
	textureProp.Type = Resource::Type::Texture2D;
	textureProp.Format = RendererFormats::HDRBufferFormat;
	textureProp.Width = m_Window.GetWindowWidth();
	textureProp.Height = m_Window.GetWindowHeight();
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

	Buffer::Properties<RenderPassConstantsCPU, true> bufferProp{};
	bufferProp.NumElements = 1;
	m_RenderPassCBs = m_RenderDevice.CreateBuffer(bufferProp, CPUAccessibleHeapType::Upload, nullptr);

	CreatePostProcessChain();

	voidFuture.wait();
	//// Begin recording our initialization commands
	//m_pDirectCommandList->Reset(nullptr);
	//{
	//	// Load sky box
	//	m_Skybox.InitMeshAndUploadBuffer(m_pDirectCommandList);
	//	m_Skybox.LoadFromFile("../../Engine/Assets/IBL/ChiricahuaPath.hdr", m_pDirectCommandList);
	//	m_Skybox.GenerateConvolutionCubeMaps(m_pDirectCommandList);
	//	m_Skybox.GenerateBRDFLUT(m_pDirectCommandList);
	//}
	//m_pDirectQueue->Execute();

	//m_pDirectQueue->Signal();
	//m_pDirectQueue->Flush();
	//m_pDirectCommandList->DisposeIntermediates();

	struct ShadowPassConstantsCPU
	{
		DirectX::XMFLOAT4X4 ViewProjection;
	};

	struct ShadowRenderPassData
	{
		UINT Resolution;
		RenderResourceHandle pUploadBuffer;
		RenderResourceHandle pOutputDepthBuffer;
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc;

		float Lambda;
		Cascade Cascades[NumCascades];
	};

	auto shadowRP = m_RenderGraph.AddRenderPass<RenderPassType::Graphics, ShadowRenderPassData>("Shadow render pass",
		[](ShadowRenderPassData& Data, RenderDevice& RenderDevice)
	{
		RenderDevice.Destroy(Data.pUploadBuffer);
		RenderDevice.Destroy(Data.pOutputDepthBuffer);

		Data.Resolution = 2048;

		Buffer::Properties<ShadowPassConstantsCPU, true> bufferProp{};
		bufferProp.NumElements = NumCascades;
		Data.pUploadBuffer = RenderDevice.CreateBuffer(bufferProp, CPUAccessibleHeapType::Upload, nullptr);

		Texture::Properties textureProp{};
		textureProp.Type = Resource::Type::Texture2D;
		textureProp.Format = DXGI_FORMAT_R32_TYPELESS;
		textureProp.Width = Data.Resolution;
		textureProp.Height = Data.Resolution;
		textureProp.DepthOrArraySize = NumCascades;
		textureProp.MipLevels = 1;
		textureProp.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		textureProp.InitialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		textureProp.pOptimizedClearValue = &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);
		Data.pOutputDepthBuffer = RenderDevice.CreateTexture(textureProp);

		// TODO: Add a mediator class that lets the render pass know we're writing/creating/reading a resource
		//Data.pOutputDepthBuffer = Mediator.Create<RGTexture, TextureDesc>("Output Depth Buffer", textureDesc);
		//Data.pOutputDepthBuffer = Mediator.Write(Data.pOutputDepthBuffer);

		Data.SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
		Data.SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		Data.SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		Data.SRVDesc.Texture2DArray.MostDetailedMip = 0u;
		Data.SRVDesc.Texture2DArray.MipLevels = -1;
		Data.SRVDesc.Texture2DArray.FirstArraySlice = 0u;
		Data.SRVDesc.Texture2DArray.ArraySize = NumCascades;
		Data.SRVDesc.Texture2DArray.PlaneSlice = 0u;
		Data.SRVDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;

		Data.Lambda = 0.5f;
	},
		[=](const ShadowRenderPassData& Data, RenderGraphRegistry& RenderGraphRegistry, CommandList& CommandList)
	{
		using namespace DirectX;

		// Calculate cascade
		const float MaxDistance = 1.0f;
		const float MinDistance = 0.0f;
		OrthographicCamera CascadeCameras[NumCascades];

		float cascadeSplits[NumCascades] = { 0.0f, 0.0f, 0.0f, 0.0f };

		if (const PerspectiveCamera* pPerspectiveCamera = dynamic_cast<const PerspectiveCamera*>(m_pCamera))
		{
			float nearClip = pPerspectiveCamera->NearZ();
			float farClip = pPerspectiveCamera->FarZ();
			float clipRange = farClip - nearClip;

			float minZ = nearClip + MinDistance * clipRange;
			float maxZ = nearClip + MaxDistance * clipRange;

			float range = maxZ - minZ;
			float ratio = maxZ / minZ;

			for (UINT i = 0; i < NumCascades; ++i)
			{
				float p = (i + 1) / static_cast<float>(NumCascades);
				float log = minZ * std::pow(ratio, p);
				float uniform = minZ + range * p;
				float d = Data.Lambda * (log - uniform) + uniform;
				cascadeSplits[i] = (d - nearClip) / clipRange;
			}
		}
		else
		{
			for (UINT i = 0; i < NumCascades; ++i)
			{
				cascadeSplits[i] = Math::Lerp(MinDistance, MaxDistance, (i + 1.0f) / float(NumCascades));
			}
		}

		// Calculate projection matrix for each cascade
		for (UINT cascadeIndex = 0; cascadeIndex < NumCascades; ++cascadeIndex)
		{
			XMVECTOR frustumCornersWS[8] =
			{
				XMVectorSet(-1.0f, +1.0f, 0.0f, 1.0f), // Near top left
				XMVectorSet(+1.0f, +1.0f, 0.0f, 1.0f), // Near top right
				XMVectorSet(+1.0f, -1.0f, 0.0f, 1.0f), // Near bottom right
				XMVectorSet(-1.0f, -1.0f, 0.0f, 1.0f), // Near bottom left
				XMVectorSet(-1.0f, +1.0f, 1.0f, 1.0f),  // Far top left
				XMVectorSet(+1.0f, +1.0f, 1.0f, 1.0f), // Far top right
				XMVectorSet(+1.0f, -1.0f, 1.0f, 1.0f), // Far bottom right
				XMVectorSet(-1.0f, -1.0f, 1.0f, 1.0f) // Far bottom left
			};

			// Used for unprojecting
			XMMATRIX invCameraViewProj = XMMatrixInverse(nullptr, m_pCamera->ViewProjectionMatrix());
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
			float scaledRadius = radius * ((static_cast<float>(Data.Resolution) + 7.0f) / static_cast<float>(Data.Resolution));

			// Negate direction
			XMVECTOR lightPos = vCenter + (-XMLoadFloat3(&m_pScene->DirectionalLight.Direction) * radius);

			CascadeCameras[cascadeIndex].SetLens(-scaledRadius, +scaledRadius, -scaledRadius, +scaledRadius, 0.0f, radius * 2.0f);
			CascadeCameras[cascadeIndex].SetLookAt(lightPos, vCenter, Math::Up);

			// Create the rounding matrix, by projecting the world-space origin and determining
			// the fractional offset in texel space
			DirectX::XMMATRIX shadowMatrix = CascadeCameras[cascadeIndex].ViewProjectionMatrix();
			DirectX::XMVECTOR shadowOrigin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
			shadowOrigin = DirectX::XMVector4Transform(shadowOrigin, shadowMatrix);
			shadowOrigin = DirectX::XMVectorScale(shadowOrigin, static_cast<float>(Data.Resolution) * 0.5f);

			DirectX::XMVECTOR roundedOrigin = DirectX::XMVectorRound(shadowOrigin);
			DirectX::XMVECTOR roundOffset = DirectX::XMVectorSubtract(roundedOrigin, shadowOrigin);
			roundOffset = DirectX::XMVectorScale(roundOffset, 2.0f / static_cast<float>(Data.Resolution));
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
			const float clipDist = m_pCamera->FarZ() - m_pCamera->NearZ();

			auto& mutableData = const_cast<ShadowRenderPassData&>(Data);
			DirectX::XMStoreFloat4x4(&mutableData.Cascades[cascadeIndex].ShadowTransform, shadowTransform);
			mutableData.Cascades[cascadeIndex].Split = m_pCamera->NearZ() + cascadeSplit * clipDist;
		}

		// Update cpu buffer
		for (UINT cascadeIndex = 0; cascadeIndex < NumCascades; ++cascadeIndex)
		{
			ShadowPassConstantsCPU cascadeShadowPass;
			XMStoreFloat4x4(&cascadeShadowPass.ViewProjection, XMMatrixTranspose(CascadeCameras[cascadeIndex].ViewProjectionMatrix()));

			auto rawBufferPtr = RenderGraphRegistry.GetBuffer(Data.pUploadBuffer);
			rawBufferPtr->Map();
			rawBufferPtr->Update<ShadowPassConstantsCPU>(cascadeIndex, cascadeShadowPass);
		}

		// Begin rendering
		PIXMarker pixmarker(CommandList.GetD3DCommandList(), L"Scene Cascade Shadow Map Render");

		CommandList.SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::Shadow));
		CommandList.SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::Shadow));

		CommandList.RSSetViewports(1, &CD3DX12_VIEWPORT(RenderGraphRegistry.GetTexture(Data.pOutputDepthBuffer)->GetD3DResource()));
		CommandList.RSSetScissorRects(1, &CD3DX12_RECT(0, 0, Data.Resolution, Data.Resolution));

		for (UINT cascadeIndex = 0; cascadeIndex < NumCascades; ++cascadeIndex)
		{
			const OrthographicCamera& CascadeCamera = CascadeCameras[cascadeIndex];
			PIXMarker pixmarker(CommandList.GetD3DCommandList(), std::wstring(L"Cascade Shadow Map " + std::to_wstring(cascadeIndex)).data());

			CommandList.SetGraphicsRootCBV(1, RenderGraphRegistry.GetBuffer(Data.pUploadBuffer)->GetBufferLocationAt(cascadeIndex));

			/*CommandList.OMSetRenderTargets(rt, -1, -1, cascadeIndex, 0);
			CommandList.ClearDepthStencilView(Data.pOutputDepthBuffer->Type()->DSV(cascadeIndex, 0),
				D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
				1.0f, 0,
				0, nullptr);*/

			const UINT numVisibleModels = CullModelsOrthographic(CascadeCamera, true, m_pScene->Models, m_VisibleModelsIndices);
			for (UINT i = 0; i < numVisibleModels; ++i)
			{
				const UINT modelIndex = m_VisibleModelsIndices[i];
				Model* pModel = m_pScene->Models[modelIndex].get();
				const UINT numVisibleMeshes = CullMeshesOrthographic(CascadeCamera, true, pModel->GetMeshes(), m_VisibleMeshesIndices[modelIndex]);

				Buffer* pVBs[] = { RenderGraphRegistry.GetBuffer(pModel->m_VertexBuffer) };
				CommandList.IASetVertexBuffers(0, 1, pVBs);
				CommandList.IASetIndexBuffer(RenderGraphRegistry.GetBuffer(pModel->m_IndexBuffer));
				for (UINT j = 0; j < numVisibleMeshes; ++j)
				{
					const UINT meshIndex = m_VisibleMeshesIndices[modelIndex][j];
					Mesh* pMesh = pModel->GetMeshes()[meshIndex].get();
					Material* pMeshMaterial = pModel->GetMaterials()[pMesh->MaterialIndex].get();

					CommandList.SetGraphicsRootCBV(0, RenderGraphRegistry.GetBuffer(pModel->m_ObjectCBs)->GetBufferLocationAt(meshIndex));
					CommandList.DrawIndexedInstanced(pMesh->IndexCount, 1, pMesh->StartIndexLocation, pMesh->BaseVertexLocation, 0);
				}
			}
		}
	});

	//struct SceneRenderPassData
	//{
	//	RGTexture* pOutputSceneTexture;
	//	RGTexture* pOutputDepthStencilTexture;
	//	RGTexture* pInputDepthBuffer;
	//	D3D12_SHADER_RESOURCE_VIEW_DESC InputDepthBufferSRVDesc;
	//};

	//auto sceneRP = m_RenderGraph.AddRenderPass<SceneRenderPassData>("Scene render pass",
	//	[&](SceneRenderPassData& Data, RenderGraphMediator& Mediator)
	//{
	//	Data.pOutputSceneTexture = Mediator.Write(pRGFrameBuffer);
	//	Data.pOutputDepthStencilTexture = Mediator.Write(pRGFrameDepthStencilBuffer);

	//	Data.pInputDepthBuffer = Mediator.Read(shadowRP->GetData().pOutputDepthBuffer);
	//	Data.InputDepthBufferSRVDesc = shadowRP->GetData().SRVDesc;
	//},
	//	[=](const SceneRenderPassData& Data)
	//{
	//	// Update scene data cbuffer
	//	for (auto& Model : m_pScene->Models)
	//	{
	//		for (size_t meshIndex = 0, numMeshes = Model->m_pMeshes.size(); meshIndex < numMeshes; ++meshIndex)
	//		{
	//			const Mesh* pMesh = Model->GetMeshes()[meshIndex].get();

	//			ObjectConstantsCPU ObjectCPU;
	//			DirectX::XMStoreFloat4x4(&ObjectCPU.World, XMMatrixTranspose(pMesh->Transform.Matrix()));
	//			Model->m_pObjectCBs->Update<ObjectConstantsCPU>(meshIndex, ObjectCPU);
	//		}

	//		// Update material cbuffer
	//		for (size_t materialIndex = 0, numMaterials = Model->m_pMaterials.size(); materialIndex < numMaterials; ++materialIndex)
	//		{
	//			const Material* pMaterial = Model->GetMaterials()[materialIndex].get();

	//			MaterialDataCPU materialDataCPU;
	//			materialDataCPU.Albedo = pMaterial->Properties.Albedo;
	//			materialDataCPU.Metallic = pMaterial->Properties.Metallic;
	//			materialDataCPU.Emissive = pMaterial->Properties.Emissive;
	//			materialDataCPU.Roughness = pMaterial->Properties.Roughness;
	//			materialDataCPU.Flags = pMaterial->Flags;
	//			if (!m_Debug.EnableAlbedo)
	//				materialDataCPU.Flags &= ~(Material::MATERIAL_FLAG_HAVE_ALBEDO_TEXTURE);
	//			if (!m_Debug.EnableNormal)
	//				materialDataCPU.Flags &= ~(Material::MATERIAL_FLAG_HAVE_NORMAL_TEXTURE);
	//			if (!m_Debug.EnableRoughness)
	//				materialDataCPU.Flags &= ~(Material::MATERIAL_FLAG_HAVE_ROUGHNESS_TEXTURE);
	//			if (!m_Debug.EnableMetallic)
	//				materialDataCPU.Flags &= ~(Material::MATERIAL_FLAG_HAVE_METALLIC_TEXTURE);
	//			if (!m_Debug.EnableEmissive)
	//				materialDataCPU.Flags &= ~(Material::MATERIAL_FLAG_HAVE_EMISSIVE_TEXTURE);
	//			Model->m_pMaterialCBs->Update<MaterialDataCPU>(materialIndex, materialDataCPU);
	//		}
	//	}

	//	// Update render pass cbuffer
	//	{
	//		RenderPassConstantsCPU renderPassCPU;
	//		XMStoreFloat4x4(&renderPassCPU.ViewProjection, XMMatrixTranspose(m_pCamera->ViewProjectionMatrix()));
	//		renderPassCPU.EyePosition = m_pCamera->GetTransform().Position;

	//		renderPassCPU.NumPointLights = LightSettings::NumPointLights;
	//		renderPassCPU.NumSpotLights = LightSettings::NumSpotLights;

	//		renderPassCPU.DirectionalLight = m_pScene->DirectionalLight;
	//		renderPassCPU.PointLights[0] = m_pScene->PointLights[0];

	//		for (UINT i = 0; i < NumCascades; ++i)
	//		{
	//			renderPassCPU.Cascades[i] = shadowRP->GetData().Cascades[i];
	//		}

	//		renderPassCPU.VisualizeCascade = m_Debug.VisualizeCascade;
	//		renderPassCPU.DebugViewInput = m_Debug.DebugViewInput;

	//		m_RenderPassCBs->Update<RenderPassConstantsCPU>(0, renderPassCPU);
	//	}

	//	PIXMarker pixmarker(m_pDirectCommandList->D3DCommandList(), L"Scene Render");

	//	auto pso = m_Setting.WireFrame ? GraphicsPSORegistry::Instance().Query(GraphicsPSOs::PBRWireFrame)
	//		: GraphicsPSORegistry::Instance().Query(GraphicsPSOs::PBR);
	//	auto rs = RootSignatureRegistry::Instance().Query(RootSignatures::PBR);

	//	m_pDirectCommandList->SetPipelineState(pso);
	//	m_pDirectCommandList->SetGraphicsRootSignature(*rs);

	//	RenderTarget rt;
	//	rt.Attach(RenderTarget0, Data.pOutputSceneTexture->Type());
	//	rt.Attach(DepthStencil, Data.pOutputDepthStencilTexture->Type());
	//	m_pDirectCommandList->RSSetViewports(1, &rt.Viewport());
	//	m_pDirectCommandList->RSSetScissorRects(1, &rt.ScissorRect());
	//	m_pDirectCommandList->OMSetRenderTargets(rt);
	//	m_pDirectCommandList->ClearRenderTargetView(rt[AttachmentPoint::RenderTarget0]->RTV(-1, -1), DirectX::Colors::LightBlue, 0, nullptr);
	//	m_pDirectCommandList->ClearDepthStencilView(rt[AttachmentPoint::DepthStencil]->DSV(-1, -1), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	//	m_pDirectCommandList->SetShaderResourceView(RootParameters::PBR::PBRRootParameter_ShadowMapAndEnvironmentMapsSRV, 0, Data.pInputDepthBuffer->Type(), &Data.InputDepthBufferSRVDesc);
	//	m_pDirectCommandList->SetShaderResourceView(RootParameters::PBR::PBRRootParameter_ShadowMapAndEnvironmentMapsSRV, 1, m_Skybox.IrradianceTexture, &m_Skybox.IrradianceSRVDesc);
	//	m_pDirectCommandList->SetShaderResourceView(RootParameters::PBR::PBRRootParameter_ShadowMapAndEnvironmentMapsSRV, 2, m_Skybox.PrefilterdTexture, &m_Skybox.PrefilterdSRVDesc);
	//	m_pDirectCommandList->SetShaderResourceView(RootParameters::PBR::PBRRootParameter_ShadowMapAndEnvironmentMapsSRV, 3, m_Skybox.BRDFLUT, &m_Skybox.BRDFLUTSRVDesc);
	//	m_pDirectCommandList->SetGraphicsRootConstantBufferView(RootParameters::PBR::PBRRootParameter_RenderPassCBuffer, m_RenderPassCBs->BufferLocationAt(0));

	//	const UINT numVisibleModels = CullModels(m_pCamera, m_pScene->Models, m_VisibleModelsIndices);
	//	for (UINT i = 0; i < numVisibleModels; ++i)
	//	{
	//		const UINT modelIndex = m_VisibleModelsIndices[i];
	//		Model* pModel = m_pScene->Models[modelIndex].get();
	//		const UINT numVisibleMeshes = CullMeshes(m_pCamera, pModel->GetMeshes(), m_VisibleMeshesIndices[modelIndex]);

	//		m_pDirectCommandList->IASetVertexBuffers(0, 1, &pModel->m_pVertexBuffer);
	//		m_pDirectCommandList->IASetIndexBuffer(pModel->m_pIndexBuffer);
	//		for (UINT j = 0; j < numVisibleMeshes; ++j)
	//		{
	//			const UINT meshIndex = m_VisibleMeshesIndices[modelIndex][j];
	//			Mesh* pMesh = pModel->GetMeshes()[meshIndex].get();
	//			Material* pMeshMaterial = pModel->GetMaterials()[pMesh->MaterialIndex].get();

	//			m_pDirectCommandList->SetShaderResourceView(RootParameters::PBR::PBRRootParameter_MaterialTexturesSRV, 0, pMeshMaterial->pMaps[TextureType::Albedo],
	//				&pMeshMaterial->ViewDescs[TextureType::Albedo]);
	//			m_pDirectCommandList->SetShaderResourceView(RootParameters::PBR::PBRRootParameter_MaterialTexturesSRV, 1, pMeshMaterial->pMaps[TextureType::Normal],
	//				&pMeshMaterial->ViewDescs[TextureType::Normal]);
	//			m_pDirectCommandList->SetShaderResourceView(RootParameters::PBR::PBRRootParameter_MaterialTexturesSRV, 2, pMeshMaterial->pMaps[TextureType::Roughness],
	//				&pMeshMaterial->ViewDescs[TextureType::Roughness]);
	//			m_pDirectCommandList->SetShaderResourceView(RootParameters::PBR::PBRRootParameter_MaterialTexturesSRV, 3, pMeshMaterial->pMaps[TextureType::Metallic],
	//				&pMeshMaterial->ViewDescs[TextureType::Metallic]);
	//			m_pDirectCommandList->SetShaderResourceView(RootParameters::PBR::PBRRootParameter_MaterialTexturesSRV, 4, pMeshMaterial->pMaps[TextureType::Emissive],
	//				&pMeshMaterial->ViewDescs[TextureType::Emissive]);

	//			m_pDirectCommandList->SetGraphicsRootConstantBufferView(RootParameters::PBR::PBRRootParameter_ObjectCBuffer, pModel->m_pObjectCBs->BufferLocationAt(meshIndex));
	//			m_pDirectCommandList->SetGraphicsRootConstantBufferView(RootParameters::PBR::PBRRootParameter_MaterialCBuffer, pModel->m_pMaterialCBs->BufferLocationAt(pMesh->MaterialIndex));
	//			m_pDirectCommandList->DrawIndexedInstanced(pMesh->IndexCount, 1, pMesh->StartIndexLocation, pMesh->BaseVertexLocation, 0);
	//		}
	//	}
	//});

	//struct SkyboxRenderPassData
	//{
	//	RGTexture* pOutputSceneTexture;
	//	RGTexture* pOutputDepthStencilTexture;
	//};

	//auto skyboxRP = m_RenderGraph.AddRenderPass<SkyboxRenderPassData>("Skybox render pass",
	//	[&](SkyboxRenderPassData& Data, RenderGraphMediator& Mediator)
	//{
	//	Data.pOutputSceneTexture = Mediator.Write(sceneRP->GetData().pOutputSceneTexture);
	//	Data.pOutputDepthStencilTexture = Mediator.Write(sceneRP->GetData().pOutputDepthStencilTexture);
	//},
	//	[=](const SkyboxRenderPassData& Data)
	//{
	//	PIXMarker pixmarker(m_pDirectCommandList->D3DCommandList(), L"Skybox Render");

	//	auto pso = GraphicsPSORegistry::Instance().Query(GraphicsPSOs::Skybox);
	//	auto rs = RootSignatureRegistry::Instance().Query(RootSignatures::Skybox);
	//	m_pDirectCommandList->SetPipelineState(pso);
	//	m_pDirectCommandList->SetGraphicsRootSignature(*rs);

	//	RenderTarget rt;
	//	rt.Attach(RenderTarget0, Data.pOutputSceneTexture->Type());
	//	rt.Attach(DepthStencil, Data.pOutputDepthStencilTexture->Type());
	//	m_pDirectCommandList->RSSetViewports(1, &rt.Viewport());
	//	m_pDirectCommandList->RSSetScissorRects(1, &rt.ScissorRect());
	//	m_pDirectCommandList->OMSetRenderTargets(rt);

	//	m_pDirectCommandList->IASetVertexBuffers(0, 1, &m_Skybox.pVertexBuffer);
	//	m_pDirectCommandList->IASetIndexBuffer(m_Skybox.pIndexBuffer);

	//	m_pDirectCommandList->SetGraphicsRootConstantBufferView(RootParameters::Skybox::SkyboxRootParameter_RenderPassCBuffer, m_RenderPassCBs->BufferLocationAt(0));
	//	if (m_Setting.UseIrradianceMap)
	//		m_pDirectCommandList->SetShaderResourceView(RootParameters::Skybox::SkyboxRootParameter_CubeMapSRV, 0, m_Skybox.IrradianceTexture, &m_Skybox.IrradianceSRVDesc);
	//	else
	//		m_pDirectCommandList->SetShaderResourceView(RootParameters::Skybox::SkyboxRootParameter_CubeMapSRV, 0, m_Skybox.PrefilterdTexture, &m_Skybox.PrefilterdSRVDesc);
	//	m_pDirectCommandList->DrawIndexedInstanced(m_Skybox.Mesh.IndexCount, 1, m_Skybox.Mesh.StartIndexLocation, m_Skybox.Mesh.BaseVertexLocation, 0);
	//});

	//struct PostProcessRenderPassData
	//{
	//	RGTexture* pInputSceneTexture;
	//};

	//auto postprocessRP = m_RenderGraph.AddRenderPass<PostProcessRenderPassData>("Post process render pass",
	//	[&](PostProcessRenderPassData& Data, RenderGraphMediator& Mediator)
	//{
	//	Data.pInputSceneTexture = Mediator.Read(sceneRP->GetData().pOutputSceneTexture);
	//},
	//	[=](const PostProcessRenderPassData& Data)
	//{
	//	PIXMarker pixmarker(m_pDirectCommandList->D3DCommandList(), L"Post Process Render");
	//	m_pDirectCommandList->TransitionBarrier(Data.pInputSceneTexture->Type(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	//	m_pDirectCommandList->FlushResourceBarriers();
	//	Texture* pCurrentPostProcessRT = Data.pInputSceneTexture->Type();
	//	for (auto& PostProcessEffect : m_pPostProcessChain)
	//	{
	//		if (!PostProcessEffect->IsEnabled())
	//			continue;

	//		pCurrentPostProcessRT = PostProcessEffect->Process(pCurrentPostProcessRT, m_pDirectCommandList);
	//	}

	//	m_pDirectCommandList->CopyResource(CurrentBackBuffer(), pCurrentPostProcessRT);
	//});

	m_RenderGraph.Setup();
	m_RenderGraph.Debug();
}

Renderer::~Renderer()
{
	m_ImGuiManager.ShutDown();
	if (m_pSwapChain)
	{
		m_pSwapChain->Release();
		m_pSwapChain = nullptr;
	}
}

void Renderer::SetActiveCamera(Camera* pCamera)
{
	m_pCamera = pCamera;
	if (PerspectiveCamera* pPerspectiveCamera = dynamic_cast<PerspectiveCamera*>(m_pCamera))
		pPerspectiveCamera->SetAspectRatio(m_AspectRatio);
}

void Renderer::SetActiveScene(Scene* pScene)
{
	std::vector<RenderResourceHandle> stagingBuffers;

	m_pScene = pScene;
	m_UploadCommandList.Reset(nullptr);
	{
		m_VisibleModelsIndices.resize(m_pScene->Models.size());

		for (size_t i = 0, size = m_pScene->Models.size(); i < size; ++i)
		{
			Model* pCurrentModel = m_pScene->Models[i].get();
			m_VisibleMeshesIndices[i].resize(pCurrentModel->GetMeshes().size());

			{
				Buffer::Properties<Vertex, false> vbProp{};
				vbProp.NumElements = pCurrentModel->GetVertices().size();
				vbProp.InitialState = D3D12_RESOURCE_STATE_COPY_DEST;

				pCurrentModel->m_VertexBuffer = m_RenderDevice.CreateBuffer(vbProp);
				RenderResourceHandle vbStagingBuffer = m_RenderDevice.CreateBuffer(vbProp, CPUAccessibleHeapType::Upload, pCurrentModel->GetVertices().data());
				stagingBuffers.push_back(vbStagingBuffer);

				m_UploadCommandList.CopyResource(m_RenderDevice.GetBuffer(pCurrentModel->m_VertexBuffer), m_RenderDevice.GetBuffer(vbStagingBuffer));
			}

			{
				Buffer::Properties<unsigned int, false> ibProp{};
				ibProp.NumElements = pCurrentModel->GetIndices().size();
				ibProp.InitialState = D3D12_RESOURCE_STATE_COPY_DEST;

				pCurrentModel->m_IndexBuffer = m_RenderDevice.CreateBuffer(ibProp);
				RenderResourceHandle ibStagingBuffer = m_RenderDevice.CreateBuffer(ibProp, CPUAccessibleHeapType::Upload, pCurrentModel->GetIndices().data());
				stagingBuffers.push_back(ibStagingBuffer);

				m_UploadCommandList.CopyResource(m_RenderDevice.GetBuffer(pCurrentModel->m_IndexBuffer), m_RenderDevice.GetBuffer(ibStagingBuffer));
			}

			{
				Buffer::Properties<ObjectConstantsCPU, true> ubProp{};
				ubProp.NumElements = pCurrentModel->GetMeshes().size();
				pCurrentModel->m_ObjectCBs = m_RenderDevice.CreateBuffer(ubProp, CPUAccessibleHeapType::Upload, nullptr);
			}

			{
				Buffer::Properties<MaterialDataCPU, true> ubProp{};
				ubProp.NumElements = pCurrentModel->GetMaterials().size();
				pCurrentModel->m_MaterialCBs = m_RenderDevice.CreateBuffer(ubProp, CPUAccessibleHeapType::Upload, nullptr);
			}

			/*for (size_t j = 0, materialSize = pCurrentModel->m_pMaterials.size(); j < materialSize; ++j)
			{
				Material* pCurrentMaterial = pCurrentModel->GetMaterials()[j].get();

				auto LoadTexture = [&](TextureType Type)
				{
					auto HaveTexture = [Type](Material* pMaterial)
					{
						switch (Type)
						{
						case TextureType::Albedo: return pMaterial->HaveAlbedoMap();
						case TextureType::Normal: return pMaterial->HaveNormalMap();
						case TextureType::Roughness: return pMaterial->HaveRoughnessMap();
						case TextureType::Metallic: return pMaterial->HaveMetallicMap();
						case TextureType::Emissive: return pMaterial->HaveEmissiveMap();
						default: throw std::exception("Type error");
						}
					};

					auto ForceSRGB = [Type]()
					{
						switch (Type)
						{
						case TextureType::Albedo:
						case TextureType::Emissive:
							return true;
						case TextureType::Normal:
						case TextureType::Roughness:
						case TextureType::Metallic:
							return false;
						default: throw std::exception("Type error");
						}
					};

					if (HaveTexture(pCurrentMaterial))
					{
						bool forceSRGB = ForceSRGB();
						if (!pCurrentMaterial->Maps[Type].empty())
							m_pDirectCommandList->LoadTextureFromFile(pCurrentMaterial->Maps[Type].data(), forceSRGB, &pCurrentMaterial->pMaps[Type]);
						else
							m_pDirectCommandList->LoadTextureFromScratchImage(pCurrentMaterial->ScratchImages[Type], forceSRGB, &pCurrentMaterial->pMaps[Type]);

						pCurrentMaterial->ViewDescs[Type].Format = pCurrentMaterial->pMaps[Type]->Resource()->GetDesc().Format;
						pCurrentMaterial->ViewDescs[Type].ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
						pCurrentMaterial->ViewDescs[Type].Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
						pCurrentMaterial->ViewDescs[Type].Texture2D.MostDetailedMip = 0;
						pCurrentMaterial->ViewDescs[Type].Texture2D.MipLevels = -1;
						pCurrentMaterial->ViewDescs[Type].Texture2D.PlaneSlice = 0;
						pCurrentMaterial->ViewDescs[Type].Texture2D.ResourceMinLODClamp = 0.0f;
					}
				};

				LoadTexture(TextureType::Albedo);
				if (pCurrentMaterial->HaveAlbedoMap())
					if (pCurrentMaterial->pMaps[TextureType::Albedo]->IsMasked())
						pCurrentMaterial->Flags |= Material::Flags::MATERIAL_FLAG_IS_MASKED;
				LoadTexture(TextureType::Normal);
				LoadTexture(TextureType::Roughness);
				LoadTexture(TextureType::Metallic);
				LoadTexture(TextureType::Emissive);
			}*/
		}
	}
	CommandList* pCommandLists[] = { &m_UploadCommandList };
	m_RenderDevice.GetGraphicsQueue()->Execute(1, pCommandLists, &m_RenderDevice.GetGlobalResourceStateTracker());
	m_RenderDevice.GetGraphicsQueue()->WaitForIdle();
	for (auto renderHandle : stagingBuffers)
	{
		m_RenderDevice.Destroy(renderHandle);
	}
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
	m_RenderGraph.Execute();
	//m_ImGuiManager.BeginFrame();
	//m_pScene->RenderImGuiWindow();
	//RenderImGuiWindow();
	//RenderTarget rt;
	//rt.Attach(RenderTarget0, CurrentBackBuffer());
	//m_ImGuiManager.EndFrame(rt, m_pDirectCommandList);
	//
	//m_pDirectCommandList->TransitionBarrier(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);
	//
	//m_pDirectQueue->Execute();
	//m_pDirectQueue->Signal();
	//m_pDirectQueue->Flush();
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
		ComPtr<ID3D12DeviceRemovedExtendedData> pDRED;
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

#pragma region CULLING
UINT Renderer::CullModels(const Camera* pCamera, const std::vector<std::unique_ptr<Model>>& Models, std::vector<UINT>& Indices)
{
	DirectX::BoundingFrustum frustum(pCamera->ProjectionMatrix());
	frustum.Transform(frustum, pCamera->WorldMatrix());

	UINT numVisible = 0;
	for (size_t i = 0, numModels = Models.size(); i < numModels; ++i)
	{
		DirectX::BoundingBox aabb;
		Models[i]->GetRootAABB().Transform(aabb, Models[i]->GetRootTransform().Matrix());
		if (frustum.Contains(aabb) != DirectX::ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = i;
		}
	}

	return numVisible;
}

UINT Renderer::CullMeshes(const Camera* pCamera, const std::vector<std::unique_ptr<Mesh>>& Meshes, std::vector<UINT>& Indices)
{
	DirectX::BoundingFrustum frustum(pCamera->ProjectionMatrix());
	frustum.Transform(frustum, pCamera->WorldMatrix());

	UINT numVisible = 0;
	for (size_t i = 0, numMeshes = Meshes.size(); i < numMeshes; ++i)
	{
		DirectX::BoundingBox aabb;
		Meshes[i]->BoundingBox.Transform(aabb, Meshes[i]->Transform.Matrix());
		if (frustum.Contains(aabb) != DirectX::ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = i;
		}
	}

	return numVisible;
}

UINT Renderer::CullModelsOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const std::vector<std::unique_ptr<Model>>& Models, std::vector<UINT>& Indices)
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
	for (size_t i = 0, numModels = Models.size(); i < numModels; ++i)
	{
		DirectX::BoundingBox aabb;
		Models[i]->GetRootAABB().Transform(aabb, Models[i]->GetRootTransform().Matrix());
		if (obb.Contains(aabb) != DirectX::ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = i;
		}
	}

	return numVisible;
}

UINT Renderer::CullMeshesOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const std::vector<std::unique_ptr<Mesh>>& Meshes, std::vector<UINT>& Indices)
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
		DirectX::BoundingBox aabb;
		Meshes[i]->BoundingBox.Transform(aabb, Meshes[i]->Transform.Matrix());
		if (obb.Contains(aabb) != DirectX::ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = i;
		}
	}

	return numVisible;
}
#pragma endregion CULLING

void Renderer::RenderImGuiWindow()
{
	if (ImGui::Begin("Renderer"))
	{
		ImGui::Text("Statistics");
		ImGui::Text("Total Frame Count: %d", m_Statistics.TotalFrameCount);
		ImGui::Text("FPS: %f", m_Statistics.FPS);
		ImGui::Text("FPMS: %f", m_Statistics.FPMS);

		if (ImGui::TreeNode("Setting"))
		{
			ImGui::Checkbox("V-Sync", &m_Setting.VSync);
			ImGui::Checkbox("Wire Frame", &m_Setting.WireFrame);
			ImGui::Checkbox("Use Irradiance Map", &m_Setting.UseIrradianceMapAsSkybox);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Debug"))
		{
			ImGui::Checkbox("Visualize Cascade", &m_Debug.VisualizeCascade);
			//ImGui::SliderFloat("Lambda", &m_ParallelSplitShadowMap.Lambda, 0.0f, 1.0f, "%.01f");
			ImGui::Checkbox("Enable Albedo", &m_Debug.EnableAlbedo);
			ImGui::Checkbox("Enable Normal", &m_Debug.EnableNormal);
			ImGui::Checkbox("Enable Roughness", &m_Debug.EnableRoughness);
			ImGui::Checkbox("Enable Metallic", &m_Debug.EnableMetallic);
			ImGui::Checkbox("Enable Emissive", &m_Debug.EnableEmissive);

			const char* debugNameInputs[] = { "None", "Albedo", "Normal", "Roughness", "Metallic", "Emissive" };
			ImGui::Combo("Debug View Inputs", &m_Debug.DebugViewInput, debugNameInputs, ARRAYSIZE(debugNameInputs), ARRAYSIZE(debugNameInputs));
			ImGui::TreePop();
		}

		/*if (ImGui::TreeNode("Post Process"))
		{
			for (auto& PostProcessEffect : m_pPostProcessChain)
			{
				PostProcessEffect->RenderImGuiWindow();
			}
			ImGui::TreePop();
		}*/
	}
	ImGui::End();
}

void Renderer::Resize()
{
	m_RenderDevice.GetGraphicsQueue()->WaitForIdle();
	{
		m_AspectRatio = static_cast<float>(m_Window.GetWindowWidth()) / static_cast<float>(m_Window.GetWindowHeight());
		SetActiveCamera(m_pCamera);

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
		ThrowCOMIfFailed(m_pSwapChain->ResizeBuffers1(SwapChainBufferCount, m_Window.GetWindowWidth(), m_Window.GetWindowHeight(), RendererFormats::SwapChainBufferFormat,
			m_DXGIManager.TearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH, Nodes, Queues));
		// Recreate descriptors
		for (UINT i = 0; i < SwapChainBufferCount; ++i)
		{
			ComPtr<ID3D12Resource> pBackBuffer;
			ThrowCOMIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
			m_BackBufferTexture[i] = Texture(pBackBuffer);
			m_RenderDevice.GetGlobalResourceStateTracker().AddResourceState(m_BackBufferTexture[i].GetD3DResource(), D3D12_RESOURCE_STATE_COMMON);
		}
	}
	// Wait until resize is complete.
	m_RenderDevice.GetGraphicsQueue()->WaitForIdle();
}

void Renderer::CreatePostProcessChain()
{
	/*const DXGI_FORMAT postProcessFormat = RendererFormats::HDRBufferFormat;
	const DXGI_FORMAT toneMapFormat = RendererFormats::SwapChainBufferFormat;
	std::unique_ptr<ToneMapping> pToneMapping = std::make_unique<ToneMapping>(m_Window.GetWindowWidth(), m_Window.GetWindowHeight(), toneMapFormat);
	std::unique_ptr<Blur> pBlur = std::make_unique<Blur>(m_Window.GetWindowWidth(), m_Window.GetWindowHeight(), postProcessFormat);
	std::unique_ptr<Sepia> pSepia = std::make_unique<Sepia>(m_Window.GetWindowWidth(), m_Window.GetWindowHeight(), postProcessFormat);
	std::unique_ptr<Sobel> pSobel = std::make_unique<Sobel>(m_Window.GetWindowWidth(), m_Window.GetWindowHeight(), postProcessFormat);
	std::unique_ptr<Bloom> pBloom = std::make_unique<Bloom>(m_Window.GetWindowWidth(), m_Window.GetWindowHeight(), postProcessFormat);

	m_pPostProcessChain.push_back(std::move(pBlur));
	m_pPostProcessChain.push_back(std::move(pSepia));
	m_pPostProcessChain.push_back(std::move(pSobel));
	m_pPostProcessChain.push_back(std::move(pBloom));
	m_pPostProcessChain.push_back(std::move(pToneMapping));*/
}

void Renderer::RegisterRendererResources()
{
	RootSignatures::Register(&m_RenderDevice);
	GraphicsPSOs::Register(&m_RenderDevice);
	ComputePSOs::Register(&m_RenderDevice);
}

void Renderer::Skybox::InitMeshAndUploadBuffer(CommandList* pCommandList)
{
	//// Generate sphere mesh
	//auto box = Model::CreateBox(1.0f, 1.0f, 1.0f, 0, Material());
	//std::vector<Vertex> vertices = box->GetVertices();
	//std::vector<UINT> indices = box->GetIndices();

	//Mesh.BoundingBox = box->GetRootAABB();
	//Mesh.IndexCount = indices.size();
	//Mesh.StartIndexLocation = 0;
	//Mesh.VertexCount = vertices.size();
	//Mesh.BaseVertexLocation = 0;

	//RenderDevice::Instance().CreateVertexBuffer(vertices.size(), sizeof(Vertex), &pVertexBuffer);
	//pCommandList->CopyVertexBufferSubresource(vertices.data(), pVertexBuffer);

	//RenderDevice::Instance().CreateIndexBuffer(indices.size(), DXGI_FORMAT_R32_UINT, &pIndexBuffer);
	//pCommandList->CopyIndexBufferSubresource(indices.data(), pIndexBuffer);

	//// Create upload buffer
	//RenderDevice::Instance().CreateUploadBuffer(6, RenderPassConstantsCPUSize, &pUploadBuffer);
	//// Create 6 cameras for cube map
	//PerspectiveCamera cameras[6];

	//// Look along each coordinate axis.
	//DirectX::XMVECTOR targets[6] =
	//{
	//	DirectX::XMVectorSet(+1.0f, 0.0f, 0.0f, 0.0f), // +X
	//	DirectX::XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f), // -X
	//	DirectX::XMVectorSet(0.0f, +1.0f, 0.0f, 0.0f), // +Y
	//	DirectX::XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f), // -Y
	//	DirectX::XMVectorSet(0.0f, 0.0f, +1.0f, 0.0f), // +Z
	//	DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f)  // -Z
	//};

	//// Use world up vector (0,1,0) for all directions except +Y/-Y.  In these cases, we
	//// are looking down +Y or -Y, so we need a different "up" vector.
	//DirectX::XMVECTOR ups[6] =
	//{
	//	DirectX::XMVectorSet(0.0f, +1.0f, 0.0f, 0.0f), // +X
	//	DirectX::XMVectorSet(0.0f, +1.0f, 0.0f, 0.0f), // -X
	//	DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), // +Y
	//	DirectX::XMVectorSet(0.0f, 0.0f, +1.0f, 0.0f), // -Y
	//	DirectX::XMVectorSet(0.0f, +1.0f, 0.0f, 0.0f), // +Z
	//	DirectX::XMVectorSet(0.0f, +1.0f, 0.0f, 0.0f)  // -Z
	//};

	//// Update view matrix in upload buffer
	//for (UINT i = 0; i < 6; ++i)
	//{
	//	cameras[i].SetLookAt(DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), targets[i], ups[i]);
	//	cameras[i].SetLens(DirectX::XM_PIDIV2, 1.0f, 0.1f, 1000.0f);
	//	RenderPassConstantsCPU rpcCPU;
	//	XMStoreFloat4x4(&rpcCPU.ViewProjection, XMMatrixTranspose(cameras[i].ViewProjectionMatrix()));
	//	pUploadBuffer->Update<RenderPassConstantsCPU>(i, rpcCPU);
	//}
}

void Renderer::Skybox::LoadFromFile(const char* FileName, CommandList* pCommandList)
{
	/*Texture* HDRTexture = nullptr;
	pCommandList->LoadTextureFromFile(FileName, false, &HDRTexture);

	auto desc = HDRTexture->Resource()->GetDesc();
	desc.Width = desc.Height = 1024;
	desc.DepthOrArraySize = 6;
	desc.MipLevels = 0;
	RenderDevice::Instance().CreateTexture(&desc, nullptr, &RadianceTexture);

	pCommandList->EquirectangularToCubeMap(HDRTexture, RadianceTexture);

	RadianceSRVDesc.Format = RadianceTexture->Resource()->GetDesc().Format;
	RadianceSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	RadianceSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	RadianceSRVDesc.TextureCube.MostDetailedMip = 0u;
	RadianceSRVDesc.TextureCube.MipLevels = -1;
	RadianceSRVDesc.TextureCube.ResourceMinLODClamp = 0.0f;*/
}

void Renderer::Skybox::GenerateConvolutionCubeMaps(CommandList* pCommandList)
{
	//for (int i = 0; i < CubemapConvolution::CubemapConvolution_Count; ++i)
	//{
	//	const DXGI_FORMAT format = i == CubemapConvolution::Irradiance ? RendererFormats::IrradianceFormat : RendererFormats::PrefilterFormat;
	//	const UINT resolution = i == CubemapConvolution::Irradiance ? CubemapConvolutionResolution::Irradiance : CubemapConvolutionResolution::Prefilter;
	//	const UINT numMips = static_cast<UINT>(floor(log2(resolution))) + 1;
	//	const UINT num32bitValues = i == Irradiance ? sizeof(IrradianceConvolutionSetting) / 4 : sizeof(PrefilterConvolutionSetting) / 4;

	//	// Create cubemap texture
	//	Texture* pCubemap = nullptr;
	//	auto desc = CD3DX12_RESOURCE_DESC::Tex2D(format, resolution, resolution, 6, numMips);
	//	desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	//	RenderDevice::Instance().CreateTexture(&desc, nullptr, &pCubemap);

	//	// Generate cube map
	//	auto pso = GraphicsPSORegistry::Instance().Query(i == CubemapConvolution::Irradiance ? GraphicsPSOs::IrradiaceConvolution : GraphicsPSOs::PrefilterConvolution);
	//	auto rs = RootSignatureRegistry::Instance().Query(i == CubemapConvolution::Irradiance ? RootSignatures::IrradiaceConvolution : RootSignatures::PrefilterConvolution);

	//	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//	pCommandList->SetPipelineState(pso);
	//	pCommandList->SetGraphicsRootSignature(*rs);
	//	RenderTarget temp;
	//	temp.Attach(AttachmentPoint::RenderTarget0, pCubemap);
	//	pCommandList->IASetVertexBuffers(0, 1, &pVertexBuffer);
	//	pCommandList->IASetIndexBuffer(pIndexBuffer);
	//	pCommandList->SetShaderResourceView(RootParameters::CubemapConvolution::CubemapConvolutionRootParameter_CubemapSRV, 0, RadianceTexture, &RadianceSRVDesc);

	//	for (UINT mip = 0; mip < numMips; ++mip)
	//	{
	//		D3D12_VIEWPORT vp;
	//		vp.TopLeftX = vp.TopLeftY = 0.0f;
	//		vp.MinDepth = 0.0f;
	//		vp.MaxDepth = 1.0f;
	//		vp.Width = vp.Height = static_cast<FLOAT>(resolution * std::pow(0.5f, mip));

	//		D3D12_RECT sr;
	//		sr.left = sr.top = 0;
	//		sr.right = sr.bottom = resolution;

	//		pCommandList->RSSetViewports(1, &vp);
	//		pCommandList->RSSetScissorRects(1, &sr);
	//		for (UINT face = 0; face < 6; ++face)
	//		{
	//			pCommandList->OMSetRenderTargets(temp, face, mip);

	//			pCommandList->SetGraphicsRootConstantBufferView(RootParameters::CubemapConvolution::CubemapConvolutionRootParameter_RenderPassCBuffer, pUploadBuffer->BufferLocationAt(face));
	//			switch (i)
	//			{
	//			case Irradiance:
	//			{
	//				IrradianceConvolutionSetting icSetting;
	//				pCommandList->SetGraphicsRoot32BitConstants(RootParameters::CubemapConvolution::CubemapConvolutionRootParameter_Setting, num32bitValues, &icSetting, 0);
	//			}
	//			break;
	//			case Prefilter:
	//			{
	//				PrefilterConvolutionSetting pcSetting;
	//				pcSetting.Roughness = (float)mip / (float)(numMips - 1);
	//				pCommandList->SetGraphicsRoot32BitConstants(RootParameters::CubemapConvolution::CubemapConvolutionRootParameter_Setting, num32bitValues, &pcSetting, 0);
	//			}
	//			break;
	//			}
	//			pCommandList->DrawIndexedInstanced(Mesh.IndexCount, 1, Mesh.StartIndexLocation, Mesh.BaseVertexLocation, 0);
	//		}
	//	}

	//	// Init srv desc
	//	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	//	srvDesc.Format = format;
	//	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	//	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	//	srvDesc.TextureCube.MostDetailedMip = 0;
	//	srvDesc.TextureCube.MipLevels = -1;
	//	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	//	// Set cubemap
	//	switch (i)
	//	{
	//	case Irradiance:
	//	{
	//		IrradianceTexture = pCubemap;
	//		IrradianceTexture->SetName(L"Irradiance CubeMap");
	//		IrradianceSRVDesc = srvDesc;
	//	}
	//	break;
	//	case Prefilter:
	//	{
	//		PrefilterdTexture = pCubemap;
	//		PrefilterdTexture->SetName(L"Prefiltered CubeMap");
	//		PrefilterdSRVDesc = srvDesc;
	//	}
	//	break;
	//	}
	//}
}

void Renderer::Skybox::GenerateBRDFLUT(CommandList* pCommandList)
{
	//// Create texture
	//const UINT resolution = 512;
	//auto desc = CD3DX12_RESOURCE_DESC::Tex2D(RendererFormats::BRDFLUTFormat, resolution, resolution, 1, 1);
	//desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	//RenderDevice::Instance().CreateTexture(&desc, nullptr, &BRDFLUT);
	//BRDFLUT->SetName(L"BRDFLUT");

	//// Init srv desc
	//BRDFLUTSRVDesc.Format = RendererFormats::BRDFLUTFormat;
	//BRDFLUTSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	//BRDFLUTSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	//BRDFLUTSRVDesc.Texture2D.MostDetailedMip = 0;
	//BRDFLUTSRVDesc.Texture2D.MipLevels = -1;
	//BRDFLUTSRVDesc.Texture2D.PlaneSlice = 0;
	//BRDFLUTSRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	//// Generate BRDF LUT
	//auto pso = GraphicsPSORegistry::Instance().Query(GraphicsPSOs::BRDFIntegration);
	//auto rs = RootSignatureRegistry::Instance().Query(RootSignatures::Null);

	//pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//pCommandList->SetPipelineState(pso);
	//pCommandList->SetGraphicsRootSignature(*rs);
	//RenderTarget temp;
	//temp.Attach(AttachmentPoint::RenderTarget0, BRDFLUT);
	//pCommandList->OMSetRenderTargets(temp);
	//pCommandList->RSSetViewports(1, &temp.Viewport());
	//pCommandList->RSSetScissorRects(1, &temp.ScissorRect());
	//pCommandList->DrawInstanced(3, 1, 0, 0);
}

void Renderer::ImGuiRenderManager::Initialize()
{
	//D3D12_DESCRIPTOR_HEAP_DESC desc;
	//desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	//desc.NumDescriptors = 1;
	//desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	//desc.NodeMask = NodeMask;
	//auto pD3DDevice = RenderDevice::Instance().D3DDevice();
	//pD3DDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&DescriptorHeap));

	//// Initialize ImGui for d3d12
	//ImGui_ImplDX12_Init(pD3DDevice, 1,
	//	RendererFormats::SwapChainBufferFormat, DescriptorHeap.Get(),
	//	DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
	//	DescriptorHeap->GetGPUDescriptorHandleForHeapStart());
}

void Renderer::ImGuiRenderManager::ShutDown()
{
	ImGui_ImplDX12_Shutdown();
}

void Renderer::ImGuiRenderManager::BeginFrame()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::ShowDemoWindow();
}

void Renderer::ImGuiRenderManager::EndFrame(CommandList* pCommandList)
{
	//PIXMarker pixmarker(pCommandList->GetD3DCommandList(), L"ImGui Render");
	//pCommandList->OMSetRenderTargets(RenderTarget);
	//pCommandList->SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, DescriptorHeap.Get());
	//pCommandList->FlushResourceBarriers();
	//ImGui::Render();
	//ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommandList->D3DCommandList());
}