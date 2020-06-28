#pragma once
#include <wrl/client.h>
#include "../Core/EventReceiver.h"

#include "DXGIManager.h"

#include "Scene/Scene.h"

#include "RenderDevice.h"
#include "RendererRegistry.h"
#include "RenderGraph/RenderGraph.h"

class Window;
class Time;

class PostProcess;

class Renderer
{
public:
	Renderer(Window& Window);
	~Renderer();

	void Update(const Time& Time);
	void Render();
	void Present();
private:
	Window& m_Window;
	EventReceiver m_EventReceiver;

	// In FLIP swap chains the compositor owns one of your buffers at any given point
	// Thus to run unconstrained (>vsync) frame rates, you need at least 3 buffers
	enum { SwapChainBufferCount = 3 };
	enum { NodeMask = 0 };
	//enum { NumFramesToBuffer = 3 }; // Not used yet
	enum { NumCascades = 4 };

	struct Statistics
	{
		UINT64 TotalFrameCount = 0;
		UINT64 FrameCount = 0;
		DOUBLE TimeElapsed = 0.0;
		DOUBLE FPS = 0.0;
		DOUBLE FPMS = 0.0;
	} m_Statistics;

	struct Setting
	{
		bool VSync = true;
		bool WireFrame = false;
		bool UseIrradianceMapAsSkybox = false;
	} m_Setting;

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
	} m_Debug;

	struct Status
	{
		bool IsBRDFLUTGenerated = false;
	} m_Status;

	DXGIManager m_DXGIManager;

	RenderDevice m_RenderDevice;
	RenderGraph m_RenderGraph;

	// Swapchain resources
	float m_AspectRatio;
	IDXGISwapChain4* m_pSwapChain;
	Texture m_BackBufferTexture[SwapChainBufferCount];

	RenderResourceHandle m_FrameBuffer;
	RenderResourceHandle m_FrameDepthStencilBuffer;
	RenderResourceHandle m_RenderPassCBs;

	Texture& CurrentBackBuffer();

	void Resize();

	void RegisterRendererResources();
};