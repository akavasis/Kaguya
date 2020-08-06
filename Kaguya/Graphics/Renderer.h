#pragma once
#include <wrl/client.h>
#include "Core/EventReceiver.h"
#include "DXGIManager.h"
#include "Scene/Scene.h"
#include "RenderDevice.h"
#include "RenderGraph.h"
#include "RendererRegistry.h"
#include "GpuBufferAllocator.h"
#include "GpuTextureAllocator.h"

class Application;
class Window;
class Time;

class Renderer
{
public:
	Renderer(Window& Window);
	~Renderer();

	void UploadScene(Scene& Scene);

	void Update(const Time& Time);
	void Render(Scene& Scene);
private:
	void Resize(UINT Width, UINT Height);

	EventReceiver m_EventReceiver;

	// In FLIP swap chains the compositor owns one of your buffers at any given point
	// Thus to run unconstrained (>vsync) frame rates, you need at least 3 buffers
	enum { NumSwapChainBuffers = 3 };
	enum { NodeMask = 0 };
	enum { NumFramesToBuffer = 3 }; // Not used yet

	struct Constants
	{
		static constexpr UINT64 SunShadowMapResolution = 2048;
	};

	struct Statistics
	{
		UINT64 TotalFrameCount = 0;
		UINT64 FrameCount = 0;
		DOUBLE TimeElapsed = 0.0;
		DOUBLE FPS = 0.0;
		DOUBLE FPMS = 0.0;
	};

	struct Setting
	{
		bool VSync = true;
		bool WireFrame = false;
		bool UseIrradianceMapAsSkybox = false;
	};

	struct Debug
	{
		bool VisualizeCascade = false;
		bool EnableAlbedo = true;
		bool EnableNormal = true;
		bool EnableRoughness = true;
		bool EnableMetallic = true;
		bool EnableEmissive = true;
		// 0="None", 1="Albedo", 2="Normal", 3="Roughness", 4="Metallic", 5="Emissive" 
		int DebugViewInput = 0;
	};

	Statistics m_Statistics;
	Setting m_Setting;
	Debug m_Debug;

	// Swapchain resources
	static constexpr DXGI_FORMAT SwapChainBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	UINT m_CurrentBackBufferIndex;
	float m_AspectRatio;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain;
	RenderResourceHandle m_SwapChainTextureHandles[NumSwapChainBuffers];
	Texture* m_pSwapChainTextures[NumSwapChainBuffers];
	DescriptorAllocation m_SwapChainRTVs;

	DXGIManager m_DXGIManager;

	RenderDevice m_RenderDevice;
	CommandContext* m_pUploadCommandContext;
	RenderGraph m_RenderGraph;
	GpuBufferAllocator m_GpuBufferAllocator;
	GpuTextureAllocator m_GpuTextureAllocator;

	RenderResourceHandle m_RaytracingOutputBufferHandle;
	RenderResourceHandle m_DepthStencilBufferHandle;
	RenderResourceHandle m_RenderPassConstantBufferHandle;

	Texture* m_pRaytracingOutputBuffer;
	Texture* m_pDepthStencilBuffer;
	Buffer* m_pRenderPassConstantBuffer;

	struct GpuDescriptorIndices
	{
		UINT ShadowMapIndex;
		UINT FrameBufferIndex;
		DescriptorAllocation TextureShaderResourceViews;
	} m_GpuDescriptorIndices;
};