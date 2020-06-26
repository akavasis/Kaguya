#pragma once
#include <wrl/client.h>
#include "../Core/EventReceiver.h"

#include "DXGIManager.h"

#include "Scene/Scene.h"
#include "Scene/Light.h"
#include "Scene/Camera.h"

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

	void SetActiveCamera(Camera* pCamera);
	void SetActiveScene(Scene* pScene);

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
	CommandList m_UploadCommandList;

	// Swapchain resources
	float m_AspectRatio;
	IDXGISwapChain4* m_pSwapChain;
	Texture m_BackBufferTexture[SwapChainBufferCount];

	RenderResourceHandle m_FrameBuffer;
	RenderResourceHandle m_FrameDepthStencilBuffer;
	RenderResourceHandle m_RenderPassCBs;

	//------------------------------------------------
	// Scene objects
	Camera* m_pCamera;
	Scene* m_pScene;
	std::vector<UINT> m_VisibleModelsIndices;
	using ModelIndex = UINT;
	std::unordered_map<ModelIndex, std::vector<UINT>> m_VisibleMeshesIndices;
	//------------------------------------------------

	//------------------------------------------------
	// Post process chain
	//std::vector<std::unique_ptr<PostProcess>> m_pPostProcessChain;
	//------------------------------------------------

	struct Skybox
	{
		Mesh Mesh;
		RenderResourceHandle RadianceTexture;
		RenderResourceHandle IrradianceTexture;
		RenderResourceHandle PrefilterdTexture;
		RenderResourceHandle BRDFLUT;

		D3D12_SHADER_RESOURCE_VIEW_DESC RadianceSRVDesc;
		D3D12_SHADER_RESOURCE_VIEW_DESC IrradianceSRVDesc;
		D3D12_SHADER_RESOURCE_VIEW_DESC PrefilterdSRVDesc;
		D3D12_SHADER_RESOURCE_VIEW_DESC BRDFLUTSRVDesc;

		RenderResourceHandle VertexBuffer;
		RenderResourceHandle IndexBuffer;
		RenderResourceHandle UploadBuffer;

		void InitMeshAndUploadBuffer(CommandList* pCommandList);
		void LoadFromFile(const char* FileName, CommandList* pCommandList);
		void GenerateConvolutionCubeMaps(CommandList* pCommandList);
		void GenerateBRDFLUT(CommandList* pCommandList);
	} m_Skybox;

	class ImGuiRenderManager
	{
	public:
		void Initialize();
		void ShutDown();

		void BeginFrame();
		void EndFrame(CommandList* pCommandList);
	private:
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DescriptorHeap;
	} m_ImGuiManager;


	Texture& CurrentBackBuffer();

	UINT CullModels(const Camera* pCamera, const std::vector<std::unique_ptr<Model>>& Models, std::vector<UINT>& Indices);
	UINT CullMeshes(const Camera* pCamera, const std::vector<std::unique_ptr<Mesh>>& Meshes, std::vector<UINT>& Indices);
	UINT CullModelsOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const std::vector<std::unique_ptr<Model>>& Models, std::vector<UINT>& Indices);
	UINT CullMeshesOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const std::vector<std::unique_ptr<Mesh>>& Meshes, std::vector<UINT>& Indices);

	void RenderImGuiWindow();

	void Resize();

	void CreatePostProcessChain();
	void RegisterRendererResources();
};