#pragma once
#include <functional>
#include <Template/Pool.h>
#include "DXGIManager.h"
#include "RenderResourceHandle.h"
#include "RenderResourceHandleRegistry.h"
#include "RenderBuffer.h"
#include "RenderTexture.h"
#include "RenderResourceContainer.h"

#include "AL/D3D12/Device.h"
#include "AL/D3D12/ShaderCompiler.h"
#include "AL/D3D12/ResourceStateTracker.h"
#include "AL/D3D12/DescriptorHeap.h"
#include "AL/D3D12/CommandQueue.h"
#include "AL/D3D12/CommandContext.h"
#include "AL/D3D12/RaytracingAccelerationStructure.h"

#include "AL/Proxy/DeviceBufferProxy.h"
#include "AL/Proxy/DeviceTextureProxy.h"
#include "AL/Proxy/HeapProxy.h"
#include "AL/Proxy/RootSignatureProxy.h"
#include "AL/Proxy/PipelineStateProxy.h"
#include "AL/Proxy/RaytracingPipelineStateProxy.h"

struct RootParameters
{
	struct ShaderLayout
	{
		enum
		{
			SystemConstants,
			RenderPassDataCB,
			ShaderResourceDescriptorTable,
			UnorderedAccessDescriptorTable,
			SamplerDescriptorTable,
			NumRootParameters
		};
	};
};

/*
	Abstraction of underlying GPU Device, its able to create gpu resources
	and contains underlying gpu resources as well, resources are referred by using
	RenderResourceHandles
*/
class RenderDevice
{
public:
	enum
	{
		NumSwapChainBuffers				= 3,
		NumConstantBufferDescriptors	= 1024,
		NumShaderResourceDescriptors	= 1024,
		NumUnorderedAccessDescriptors	= 1024,
		NumSamplerDescriptors			= 1024,
		NumRenderTargetDescriptors		= 1024,
		NumDepthStencilDescriptors		= 1024
	};

	static constexpr DXGI_FORMAT SwapChainBufferFormat	= DXGI_FORMAT_R8G8B8A8_UNORM;
	static constexpr DXGI_FORMAT DepthStencilFormat		= DXGI_FORMAT_D32_FLOAT;

	RenderDevice(IDXGIAdapter4* pAdapter);
	~RenderDevice();

	[[nodiscard]] CommandContext* AllocateContext(CommandContext::Type Type);

	void BindGpuDescriptorHeap(CommandContext* pCommandContext);
	inline auto GetImGuiDescriptorHeap() { return &m_ImGuiDescriptorHeap; }
	inline auto GetGpuDescriptorHeapCBDescriptorFromStart() const { return m_ShaderVisibleCBSRUADescriptorHeap.GetCBDescriptorAt(0); }
	inline auto GetGpuDescriptorHeapSRDescriptorFromStart() const { return m_ShaderVisibleCBSRUADescriptorHeap.GetSRDescriptorAt(0); }
	inline auto GetGpuDescriptorHeapUADescriptorFromStart() const { return m_ShaderVisibleCBSRUADescriptorHeap.GetUADescriptorAt(0); }
	inline auto GetSamplerDescriptorHeapDescriptorFromStart() const { return m_SamplerDescriptorHeap.GetDescriptorFromStart(); }
	void ExecuteCommandContexts(CommandContext::Type Type, UINT NumCommandContexts, CommandContext* ppCommandContexts[]);

	void ResetDescriptor();

	// Shader compilation
	[[nodiscard]] Shader CompileShader(Shader::Type Type, const std::filesystem::path& Path, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);
	[[nodiscard]] Library CompileLibrary(const std::filesystem::path& Path);

	DeviceResourceAllocationInfo GetDeviceResourceAllocationInfo(UINT NumDescs, DeviceBufferProxy* pDescs);

	RenderResourceHandle InitializeRenderResourceHandle(RenderResourceType Type, const std::string& Name);
	std::string GetRenderResourceHandleName(RenderResourceHandle RenderResourceHandle);

	// Resource creation
	void CreateDeviceBuffer(RenderResourceHandle Handle, std::function<void(DeviceBufferProxy&)> Configurator);
	void CreateDeviceBuffer(RenderResourceHandle Handle, RenderResourceHandle HeapHandle, UINT64 HeapOffset, std::function<void(DeviceBufferProxy&)> Configurator);

	void CreateDeviceTexture(RenderResourceHandle Handle, Microsoft::WRL::ComPtr<ID3D12Resource> ExistingResource, Resource::State InitialState);
	void CreateDeviceTexture(RenderResourceHandle Handle, Resource::Type Type, std::function<void(DeviceTextureProxy&)> Configurator);
	void CreateDeviceTexture(RenderResourceHandle Handle, Resource::Type Type, RenderResourceHandle HeapHandle, UINT64 HeapOffset, std::function<void(DeviceTextureProxy&)> Configurator);

	void CreateHeap(RenderResourceHandle Handle, std::function<void(HeapProxy&)> Configurator);

	void CreateRootSignature(RenderResourceHandle Handle, std::function<void(RootSignatureProxy&)> Configurator, bool AddShaderLayoutRootParameters = true);
	
	void CreateGraphicsPipelineState(RenderResourceHandle Handle, std::function<void(GraphicsPipelineStateProxy&)> Configurator);
	void CreateComputePipelineState(RenderResourceHandle Handle, std::function<void(ComputePipelineStateProxy&)> Configurator);
	void CreateRaytracingPipelineState(RenderResourceHandle Handle, std::function<void(RaytracingPipelineStateProxy&)> Configurator);

	void Destroy(RenderResourceHandle RenderResourceHandle);

	// Resource view creation
	void CreateShaderResourceView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> MostDetailedMip = {}, std::optional<UINT> MipLevels = {});
	void CreateUnorderedAccessView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {});
	void CreateRenderTargetView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {});
	void CreateDepthStencilView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {});

	// Returns nullptr if a resource is not found
	[[nodiscard]] inline auto GetBuffer(RenderResourceHandle RenderResourceHandle)			{ return m_Buffers.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetTexture(RenderResourceHandle RenderResourceHandle)			{ return m_Textures.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetHeap(RenderResourceHandle RenderResourceHandle)			{ return m_Heaps.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetRootSignature(RenderResourceHandle RenderResourceHandle)	{ return m_RootSignatures.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetGraphicsPSO(RenderResourceHandle RenderResourceHandle)		{ return m_GraphicsPipelineStates.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetComputePSO(RenderResourceHandle RenderResourceHandle)		{ return m_ComputePipelineStates.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetRaytracingPSO(RenderResourceHandle RenderResourceHandle)	{ return m_RaytracingPipelineStates.GetResource(RenderResourceHandle); }

	Descriptor GetShaderResourceView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> MostDetailedMip = {}, std::optional<UINT> MipLevels = {}) const;
	Descriptor GetUnorderedAccessView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}) const;
	Descriptor GetRenderTargetView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {}) const;
	Descriptor GetDepthStencilView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {}) const;

	Device																					Device;
	CommandQueue																			GraphicsQueue, ComputeQueue, CopyQueue;
	ResourceStateTracker																	GlobalResourceStateTracker;
	ShaderCompiler																			ShaderCompiler;

	UINT																					FrameIndex;
	RenderResourceHandle																	SwapChainTextures[NumSwapChainBuffers];
private:
	void AddShaderLayoutRootParameter(RootSignatureProxy& RootSignatureProxy);

	std::vector<std::unique_ptr<CommandContext>>											m_CommandContexts[CommandContext::NumTypes];

	RenderResourceHandleRegistry															m_BufferHandleRegistry;
	RenderResourceHandleRegistry															m_TextureHandleRegistry;
	RenderResourceHandleRegistry															m_HeapHandleRegistry;
	RenderResourceHandleRegistry															m_RootSignatureHandleRegistry;
	RenderResourceHandleRegistry															m_GraphicsPipelineStateHandleRegistry;
	RenderResourceHandleRegistry															m_ComputePipelineStateHandleRegistry;
	RenderResourceHandleRegistry															m_RaytracingPipelineStateHandleRegistry;

	RenderResourceContainer<RenderResourceType::Buffer, Buffer>								m_Buffers;
	RenderResourceContainer<RenderResourceType::Texture, Texture>							m_Textures;
	RenderResourceContainer<RenderResourceType::Heap, Heap>									m_Heaps;
	RenderResourceContainer<RenderResourceType::RootSignature, RootSignature>				m_RootSignatures;
	RenderResourceContainer<RenderResourceType::GraphicsPSO, GraphicsPipelineState>			m_GraphicsPipelineStates;
	RenderResourceContainer<RenderResourceType::ComputePSO, ComputePipelineState>			m_ComputePipelineStates;
	RenderResourceContainer<RenderResourceType::RaytracingPSO, RaytracingPipelineState>		m_RaytracingPipelineStates;

	CBSRUADescriptorHeap																	m_ImGuiDescriptorHeap;
	CBSRUADescriptorHeap																	m_NonShaderVisibleCBSRUADescriptorHeap;
	CBSRUADescriptorHeap																	m_ShaderVisibleCBSRUADescriptorHeap;
	SamplerDescriptorHeap																	m_SamplerDescriptorHeap;
	RenderTargetDescriptorHeap																m_RenderTargetDescriptorHeap;
	DepthStencilDescriptorHeap																m_DepthStencilDescriptorHeap;

	Pool<void, NumConstantBufferDescriptors>												m_ConstantBufferDescriptorIndexPool;
	Pool<void, NumShaderResourceDescriptors>												m_ShaderResourceDescriptorIndexPool;
	Pool<void, NumUnorderedAccessDescriptors>												m_UnorderedAccessDescriptorIndexPool;
	Pool<void, NumSamplerDescriptors>														m_SamplerDescriptorIndexPool;
	Pool<void, NumRenderTargetDescriptors>													m_RenderTargetDescriptorIndexPool;
	Pool<void, NumDepthStencilDescriptors>													m_DepthStencilDescriptorIndexPool;

	std::unordered_map<RenderResourceHandle, RenderBuffer>									m_RenderBuffers;
	std::unordered_map<RenderResourceHandle, RenderTexture>									m_RenderTextures;
};