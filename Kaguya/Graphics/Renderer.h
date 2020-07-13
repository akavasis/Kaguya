#pragma once
#include <wrl/client.h>
#include "../Core/EventReceiver.h"

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
	Renderer(Application& RefApplication, Window& RefWindow);
	~Renderer();

	inline auto& GetRenderDevice() { return m_RenderDevice; }
	void SetScene(Scene* pScene);
	void TEST();

	void Update(const Time& Time);
	void Render();
	void Present();
private:
	Texture& CurrentBackBuffer();
	void Resize();
	UINT CullModels(const Camera* pCamera, const Scene::ModelList& Models, std::vector<const Model*>& Indices);
	UINT CullModelsOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const Scene::ModelList& Models, std::vector<const Model*>& Indices);
	UINT CullMeshes(const Camera* pCamera, const std::vector<Mesh>& Meshes, std::vector<UINT>& Indices);
	UINT CullMeshesOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const std::vector<Mesh>& Meshes, std::vector<UINT>& Indices);

	Window& m_RefWindow;
	EventReceiver m_EventReceiver;

	// In FLIP swap chains the compositor owns one of your buffers at any given point
	// Thus to run unconstrained (>vsync) frame rates, you need at least 3 buffers
	enum { SwapChainBufferCount = 3 };
	enum { NodeMask = 0 };
	enum { NumFramesToBuffer = 3 }; // Not used yet

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
	GpuBufferAllocator m_GpuBufferAllocator;
	GpuTextureAllocator m_GpuTextureAllocator;

	// Swapchain resources
	UINT m_FrameIndex;
	float m_AspectRatio;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain;
	Texture m_BackBufferTexture[SwapChainBufferCount];

	RenderResourceHandle m_FrameBuffer;
	RenderResourceHandle m_FrameDepthStencilBuffer;
	RenderResourceHandle m_RenderPassCBs;

	Scene* m_pScene;
	std::vector<const Model*> m_VisibleModelsIndices;
	std::unordered_map<const Model*, std::vector<UINT>> m_VisibleMeshesIndices;
};