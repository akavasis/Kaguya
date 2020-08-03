#pragma once
#include <memory>
#include <mutex>
#include <atomic>
#include <unordered_map>

#include "../Core/Delegate.h"

#include "RenderResourceHandle.h"
#include "ShaderCompiler.h"

#include "AL/D3D12/Device.h"
#include "AL/D3D12/ResourceStateTracker.h"
#include "AL/D3D12/DescriptorHeap.h"
#include "AL/D3D12/DescriptorAllocator.h"
#include "AL/D3D12/CommandQueue.h"
#include "AL/D3D12/CommandContext.h"

#include "AL/D3D12/Buffer.h"
#include "AL/D3D12/Texture.h"
#include "AL/D3D12/Heap.h"
#include "AL/D3D12/RootSignature.h"
#include "AL/D3D12/RootSignatureProxy.h"
#include "AL/D3D12/PipelineState.h"

struct StandardShaderLayoutOptions
{
	BOOL InitConstantDataTypeAsRootConstants;
	UINT Num32BitValues;
};

/*
	Abstraction of underlying GPU Device, its able to create gpu resources
	and contains underlying gpu resources as well, resources are referred by using
	RenderResourceHandles
*/
class RenderDevice
{
public:
	RenderDevice(IDXGIAdapter4* pAdapter);
	~RenderDevice();

	[[nodiscard]] inline Device& GetDevice() { return m_Device; }
	[[nodiscard]] inline CommandQueue* GetGraphicsQueue() { return &m_GraphicsQueue; }
	[[nodiscard]] inline CommandQueue* GetComputeQueue() { return &m_ComputeQueue; }
	[[nodiscard]] inline CommandQueue* GetCopyQueue() { return &m_CopyQueue; }
	[[nodiscard]] inline ResourceStateTracker& GetGlobalResourceStateTracker() { return m_GlobalResourceStateTracker; }
	[[nodiscard]] inline DescriptorAllocator& GetDescriptorAllocator() { return m_DescriptorAllocator; }
	[[nodiscard]] CommandContext* AllocateContext(D3D12_COMMAND_LIST_TYPE Type);

	void BindUniversalGpuDescriptorHeap(CommandContext* pCommandContext) const;
	auto GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart() const
	{
		return m_DescriptorAllocator.GetUniversalGpuDescriptorHeap()->GetRangeHandleFromStart(CBSRUADescriptorHeap::RangeType::ShaderResource);
	}
	void ExecuteRenderCommandContexts(UINT NumCommandContexts, CommandContext* ppCommandContexts[]);

	// Resource creation
	[[nodiscard]] RenderResourceHandle LoadShader(Shader::Type Type, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);
	[[nodiscard]] RenderResourceHandle CreateBuffer(const Buffer::Properties& Properties);
	[[nodiscard]] RenderResourceHandle CreateBuffer(const Buffer::Properties& Properties, CPUAccessibleHeapType CPUAccessibleHeapType, const void* pData);
	[[nodiscard]] RenderResourceHandle CreateBuffer(const Buffer::Properties& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset);
	[[nodiscard]] RenderResourceHandle CreateTexture(const Texture::Properties& Properties);
	[[nodiscard]] RenderResourceHandle CreateTexture(const Texture::Properties& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset);
	[[nodiscard]] RenderResourceHandle CreateHeap(const Heap::Properties& Properties);
	[[nodiscard]] RenderResourceHandle CreateRootSignature(StandardShaderLayoutOptions* pOptions, Delegate<void(RootSignatureProxy&)> Configurator);
	[[nodiscard]] RenderResourceHandle CreateGraphicsPipelineState(const GraphicsPipelineState::Properties& Properties);
	[[nodiscard]] RenderResourceHandle CreateComputePipelineState(const ComputePipelineState::Properties& Properties);

	void ReplaceShader(Shader::Type Type, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines, RenderResourceHandle ExistingRenderResourceHandle);
	void ReplaceBuffer(const Buffer::Properties& Properties, RenderResourceHandle ExistingRenderResourceHandle);
	void ReplaceBuffer(const Buffer::Properties& Properties, CPUAccessibleHeapType CPUAccessibleHeapType, const void* pData, RenderResourceHandle ExistingRenderResourceHandle);
	void ReplaceBuffer(const Buffer::Properties& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset, RenderResourceHandle ExistingRenderResourceHandle);
	void ReplaceTexture(const Texture::Properties& Properties, RenderResourceHandle ExistingRenderResourceHandle);
	void ReplaceTexture(const Texture::Properties& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset, RenderResourceHandle ExistingRenderResourceHandle);
	void ReplaceHeap(const Heap::Properties& Properties, RenderResourceHandle ExistingRenderResourceHandle);
	void ReplaceRootSignature(StandardShaderLayoutOptions* pOptions, Delegate<void(RootSignatureProxy&)> Configurator, RenderResourceHandle ExistingRenderResourceHandle);
	void ReplaceGraphicsPipelineState(const GraphicsPipelineState::Properties& Properties, RenderResourceHandle ExistingRenderResourceHandle);
	void ReplaceComputePipelineState(const ComputePipelineState::Properties& Properties, RenderResourceHandle ExistingRenderResourceHandle);

	void Destroy(RenderResourceHandle* pRenderResourceHandle);

	// Special creation methods
	[[nodiscard]] RenderResourceHandle CreateSwapChainTexture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingResource, D3D12_RESOURCE_STATES InitialState);
	[[nodiscard]] inline auto GetSwapChainTexture(RenderResourceHandle RenderResourceHandle) { return m_SwapChainTextures.GetResource(RenderResourceHandle); }
	void DestroySwapChainTexture(RenderResourceHandle* pRenderResourceHandle);
	void ReplaceSwapChainTexture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingResource, D3D12_RESOURCE_STATES InitialState, RenderResourceHandle ExistingRenderResourceHandle);
	void CreateRTVForSwapChainTexture(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor);

	// Resource view creation
	void CreateSRV(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor);
	void CreateUAV(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice);
	void CreateRTV(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice, std::optional<UINT> ArraySize);
	void CreateDSV(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice, std::optional<UINT> ArraySize);

	// Returns nullptr if a resource is not found
	[[nodiscard]] inline auto GetShader(RenderResourceHandle RenderResourceHandle) { return m_Shaders.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetBuffer(RenderResourceHandle RenderResourceHandle) { return m_Buffers.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetTexture(RenderResourceHandle RenderResourceHandle) { return m_Textures.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetHeap(RenderResourceHandle RenderResourceHandle) { return m_Heaps.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetRootSignature(RenderResourceHandle RenderResourceHandle) { return m_RootSignatures.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetGraphicsPSO(RenderResourceHandle RenderResourceHandle) { return m_GraphicsPipelineStates.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetComputePSO(RenderResourceHandle RenderResourceHandle) { return m_ComputePipelineStates.GetResource(RenderResourceHandle); }
private:
	void AddStandardShaderLayoutRootParameter(StandardShaderLayoutOptions* pOptions, RootSignatureProxy& RootSignatureProxy);

	Device m_Device;
	CommandQueue m_GraphicsQueue;
	CommandQueue m_ComputeQueue;
	CommandQueue m_CopyQueue;
	ResourceStateTracker m_GlobalResourceStateTracker;
	ShaderCompiler m_ShaderCompiler;
	DescriptorAllocator m_DescriptorAllocator;
	std::vector<std::unique_ptr<CommandContext>> m_RenderCommandContexts[4];

	enum DescriptorRanges
	{
		Tex2DTableRange,
		Tex2DArrayTableRange,
		TexCubeTableRange,

		NumDescriptorRanges
	};
	std::array<D3D12_DESCRIPTOR_RANGE1, std::size_t(DescriptorRanges::NumDescriptorRanges)> m_StandardShaderLayoutDescriptorRanges;

	template <RenderResourceHandle::Types Type, typename T>
	class RenderResourceContainer
	{
	public:
		template<typename... Args>
		std::pair<RenderResourceHandle, T*> CreateResource(Args&&... args)
		{
			T resource = T(std::forward<Args>(args)...);
			std::scoped_lock lk(m_Mutex);
			RenderResourceHandle handle = m_HandleFactory.GetNewHandle();
			m_ResourceTable[handle] = std::move(resource);
			return { handle, &m_ResourceTable[handle] };
		}

		// Returns false if ExistingRenderResourceHandle does not exist in the table
		template<typename... Args>
		std::pair<T*, bool> ReplaceResource(const RenderResourceHandle& ExistingRenderResourceHandle, Args&&... args)
		{
			std::scoped_lock lk(m_Mutex);
			if (m_ResourceTable.find(ExistingRenderResourceHandle) == m_ResourceTable.end())
				return { nullptr, false };
			T resource = T(std::forward<Args>(args)...);
			m_ResourceTable[ExistingRenderResourceHandle] = std::move(resource);
			return { &m_ResourceTable[ExistingRenderResourceHandle], true };
		}

		T* GetResource(const RenderResourceHandle& RenderResourceHandle)
		{
			std::scoped_lock lk(m_Mutex);
			auto iter = m_ResourceTable.find(RenderResourceHandle);
			if (iter != m_ResourceTable.end())
				return &iter->second;
			return nullptr;
		}

		const T* GetResource(const RenderResourceHandle& RenderResourceHandle) const
		{
			std::scoped_lock lk(m_Mutex);
			auto iter = m_ResourceTable.find(RenderResourceHandle);
			if (iter != m_ResourceTable.end())
				return &iter->second;
			return nullptr;
		}

		void Destroy(const RenderResourceHandle& RenderResourceHandle, bool RemoveRenderResourceHandle)
		{
			std::scoped_lock lk(m_Mutex);
			auto iter = m_ResourceTable.find(RenderResourceHandle);
			if (iter != m_ResourceTable.end())
			{
				if (RemoveRenderResourceHandle)
				{
					m_ResourceTable.erase(iter);
				}
				else
				{
					m_ResourceTable[RenderResourceHandle] = T();
				}
			}
		}
	private:
		template<RenderResourceHandle::Types Type>
		class RenderResourceHandleFactory
		{
		public:
			RenderResourceHandleFactory()
			{
				m_AtomicData = 0;
				m_Handle.Type = Type;
				m_Handle.Flag = RenderResourceHandle::Flags::Active;
				m_Handle.Data = 0;
			}

			RenderResourceHandle GetNewHandle()
			{
				m_Handle.Data = m_AtomicData.fetch_add(1);
				return m_Handle;
			}
		private:
			RenderResourceHandle m_Handle;
			std::atomic<size_t> m_AtomicData;
		};

		std::unordered_map<RenderResourceHandle, T> m_ResourceTable;
		RenderResourceHandleFactory<Type> m_HandleFactory;
		std::mutex m_Mutex;
	};

	RenderResourceContainer<RenderResourceHandle::Types::Shader, Shader> m_Shaders;

	RenderResourceContainer<RenderResourceHandle::Types::Buffer, Buffer> m_Buffers;
	RenderResourceContainer<RenderResourceHandle::Types::Texture, Texture> m_Textures;

	RenderResourceContainer<RenderResourceHandle::Types::Heap, Heap> m_Heaps;
	RenderResourceContainer<RenderResourceHandle::Types::RootSignature, RootSignature> m_RootSignatures;
	RenderResourceContainer<RenderResourceHandle::Types::GraphicsPSO, GraphicsPipelineState> m_GraphicsPipelineStates;
	RenderResourceContainer<RenderResourceHandle::Types::ComputePSO, ComputePipelineState> m_ComputePipelineStates;

	RenderResourceContainer<RenderResourceHandle::Types::Texture, Texture> m_SwapChainTextures;
};