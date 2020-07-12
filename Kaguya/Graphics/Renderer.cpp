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
	m_GpuBufferAllocator(256_MiB, 256_MiB, m_RenderDevice),
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
			{
				PIXCapture();
				Data.pGpuBufferAllocator->Stage(Scene, pRenderCommandContext);
				Data.pGpuBufferAllocator->Bind(pRenderCommandContext);
				Data.pGpuTextureAllocator->Stage(Scene, pRenderCommandContext);
			}

			m_RenderGraph.GetRenderPass<RenderPassType::Graphics, SceneStagingData>()->Enabled = false;
		};
	});

	struct SkyboxConvolutionData
	{

	};

	auto pSkyboxConvolutionPass = m_RenderGraph.AddRenderPass<RenderPassType::Graphics, SkyboxConvolutionData>(
		[&](SkyboxConvolutionData& Data, RenderDevice& RenderDevice)
	{
		return [=](const SkyboxConvolutionData& Data, Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* pRenderCommandContext)
		{
		};
	});

	struct ShadowPassData
	{
		enum { NumCascades = 4 };

		UINT resolution;
		float lambda;

		RenderResourceHandle uploadBuffer;
		RenderResourceHandle outputDepthBuffer;
		Cascade cascades[NumCascades];
	};

	auto pShadowPass = m_RenderGraph.AddRenderPass<RenderPassType::Graphics, ShadowPassData>(
		[&](ShadowPassData& Data, RenderDevice& RenderDevice)
	{
		Data.resolution = 2048;
		Data.lambda = 0.5f;

		struct ShadowPassConstantsCPU
		{
			DirectX::XMFLOAT4X4 ViewProjection;
		};
		Buffer::Properties bufferProp{};
		bufferProp.SizeInBytes = Math::AlignUp<UINT64>(ShadowPassData::NumCascades * sizeof(ShadowPassConstantsCPU), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		bufferProp.Stride = sizeof(ShadowPassConstantsCPU);
		Data.uploadBuffer = RenderDevice.CreateBuffer(bufferProp, CPUAccessibleHeapType::Upload);

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
		Data.outputDepthBuffer = RenderDevice.CreateTexture(textureProp);

		return [=](const ShadowPassData& Data, Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* pRenderCommandContext)
		{
		};
	});

	struct ForwardPassData
	{

	};

	auto pForwardPass = m_RenderGraph.AddRenderPass<RenderPassType::Graphics, ForwardPassData>(
		[&](ForwardPassData& Data, RenderDevice& RenderDevice)
	{
		return [=](const ForwardPassData& Data, Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* pRenderCommandContext)
		{
		};
	});

	m_RenderGraph.Setup();
}

Renderer::~Renderer()
{
}

void Renderer::SetScene(Scene* pScene)
{
	m_pScene = pScene;
}

void Renderer::TEST()
{
	PIXCapture();
	auto context = m_RenderDevice.AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
	m_GpuBufferAllocator.Stage(*m_pScene, context);
	m_GpuBufferAllocator.Bind(context);
	m_GpuTextureAllocator.Stage(*m_pScene, context);
	m_RenderDevice.ExecuteRenderCommandContexts(1, &context);
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