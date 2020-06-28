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


	m_RenderGraph.Setup();
}

Renderer::~Renderer()
{
	if (m_pSwapChain)
	{
		m_pSwapChain->Release();
		m_pSwapChain = nullptr;
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

void Renderer::Resize()
{
	m_RenderDevice.GetGraphicsQueue()->WaitForIdle();
	{
		m_AspectRatio = static_cast<float>(m_Window.GetWindowWidth()) / static_cast<float>(m_Window.GetWindowHeight());

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

void Renderer::RegisterRendererResources()
{
	Shaders::Register(&m_RenderDevice);
	RootSignatures::Register(&m_RenderDevice);
	GraphicsPSOs::Register(&m_RenderDevice);
	ComputePSOs::Register(&m_RenderDevice);
}