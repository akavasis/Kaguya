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
	struct Statistics
	{
		inline static UINT64 TotalFrameCount = 0;
		inline static UINT64 FrameCount = 0;
		inline static DOUBLE TimeElapsed = 0.0;
		inline static DOUBLE FPS = 0.0;
		inline static DOUBLE FPMS = 0.0;
	};

	struct Settings
	{
		inline static bool VSync = false;
		inline static constexpr bool Rasterization = false;
	};

	Renderer(Window& Window);
	~Renderer();

	void UploadScene(Scene& Scene);

	void Update(const Time& Time);
	void Render(Scene& Scene);
private:
	void Resize(UINT Width, UINT Height);

	struct GpuDescriptorIndices
	{
		DescriptorAllocation RenderTargetShaderResourceViews;
		DescriptorAllocation TextureShaderResourceViews;
	};

	const Window* pWindow;
	EventReceiver m_EventReceiver;

	enum { NumSwapChainBuffers = 3 };

	// Swapchain resources
	static constexpr DXGI_FORMAT SwapChainBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	UINT m_FrameIndex;
	float m_AspectRatio;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain;

	DXGIManager m_DXGIManager;

	RenderDevice m_RenderDevice;
	CommandContext* m_pUploadCommandContext;
	RenderGraph m_RenderGraph;
	GpuBufferAllocator m_GpuBufferAllocator;
	GpuTextureAllocator m_GpuTextureAllocator;

	RenderResourceHandle m_SwapChainTextureHandles[NumSwapChainBuffers];
	Texture* m_pSwapChainTextures[NumSwapChainBuffers];
	DescriptorAllocation m_SwapChainRTVs;

	RenderResourceHandle m_RenderPassConstantBufferHandle;
	Buffer* m_pRenderPassConstantBuffer;

	GpuDescriptorIndices m_GpuDescriptorIndices;
};