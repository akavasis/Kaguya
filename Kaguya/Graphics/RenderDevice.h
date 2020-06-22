#pragma once
#include <memory>
#include <mutex>
#include <atomic>
#include <unordered_map>

#include "RenderResourceHandle.h"
#include "ShaderManager.h"

#include "AL/D3D12/Device.h"
#include "AL/D3D12/DescriptorAllocator.h"
#include "AL/D3D12/ResourceStateTracker.h"
#include "AL/D3D12/CommandQueue.h"
#include "AL/D3D12/Buffer.h"
#include "AL/D3D12/Texture.h"
#include "AL/D3D12/Heap.h"
#include "AL/D3D12/RootSignature.h"
#include "AL/D3D12/PipelineState.h"

class RenderDevice
{
public:
	RenderDevice(IUnknown* pAdapter);
	~RenderDevice();

	inline ShaderManager& GetShaderManager() { return m_ShaderManager; }
	inline Device& GetDevice() { return m_Device; }

	template<typename D3D12_XXX_VIEW_DESC> DescriptorAllocator& GetDescriptorAllocator();
	template<> DescriptorAllocator& GetDescriptorAllocator<D3D12_CONSTANT_BUFFER_VIEW_DESC>() { return m_CBVAllocator; }
	template<> DescriptorAllocator& GetDescriptorAllocator<D3D12_SHADER_RESOURCE_VIEW_DESC>() { return m_SRVAllocator; }
	template<> DescriptorAllocator& GetDescriptorAllocator<D3D12_UNORDERED_ACCESS_VIEW_DESC>() { return m_UAVAllocator; }
	template<> DescriptorAllocator& GetDescriptorAllocator<D3D12_RENDER_TARGET_VIEW_DESC>() { return m_RTVAllocator; }
	template<> DescriptorAllocator& GetDescriptorAllocator<D3D12_DEPTH_STENCIL_VIEW_DESC>() { return m_DSVAllocator; }
	inline DescriptorAllocator& GetSamplerDescriptorAllocator() { return m_SamplerAllocator; }

	inline ResourceStateTracker& GetGlobalResourceStateTracker() { return m_GlobalResourceTracker; }

	inline CommandQueue& GetGraphicsQueue() { return m_GraphicsQueue; }
	inline CommandQueue& GetComputeQueue() { return m_ComputeQueue; }
	inline CommandQueue& GetCopyQueue() { return m_CopyQueue; }

	// Resource creation
	template<typename T, bool UseConstantBufferAlignment>
	[[nodiscard]] RenderResourceHandle CreateBuffer(const Buffer::Properties<T, UseConstantBufferAlignment>& Properties);
	template<typename T, bool UseConstantBufferAlignment>
	[[nodiscard]] RenderResourceHandle CreateBuffer(const Buffer::Properties<T, UseConstantBufferAlignment>& Properties, CPUAccessibleHeapType CPUAccessibleHeapType);
	template<typename T, bool UseConstantBufferAlignment>
	[[nodiscard]] RenderResourceHandle CreateBuffer(const Buffer::Properties<T, UseConstantBufferAlignment>& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset);

	[[nodiscard]] RenderResourceHandle CreateTexture(const Texture::Properties& Properties);
	[[nodiscard]] RenderResourceHandle CreateTexture(const Texture::Properties& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset);

	[[nodiscard]] RenderResourceHandle CreateHeap(const Heap::Properties& Properties);
	[[nodiscard]] RenderResourceHandle CreateRootSignature(const RootSignature::Properties& Properties);
	[[nodiscard]] RenderResourceHandle CreateGraphicsPipelineState(const GraphicsPipelineState::Properties& Properties);
	[[nodiscard]] RenderResourceHandle CreateComputePipelineState(const ComputePipelineState::Properties& Properties);

	void Destroy(RenderResourceHandle& RenderResourceHandle);

	// Returns nullptr if a resource is not found
	Resource* GetResource(const RenderResourceHandle& RenderResourceHandle) const;
	Buffer* GetBuffer(const RenderResourceHandle& RenderResourceHandle) const;
	Texture* GetTexture(const RenderResourceHandle& RenderResourceHandle) const;
	Heap* GetHeap(const RenderResourceHandle& RenderResourceHandle) const;
	RootSignature* GetRootSignature(const RenderResourceHandle& RenderResourceHandle) const;
	GraphicsPipelineState* GetGraphicsPSO(const RenderResourceHandle& RenderResourceHandle) const;
	ComputePipelineState* GetComputePSO(const RenderResourceHandle& RenderResourceHandle) const;
private:
	// Helper methods
	RenderResourceHandle CreateBuffer(Buffer* pAllocatedBuffer, ResourceStates InitialState);
	RenderResourceHandle CreateTexture(Texture* pAllocatedTexture, Resource::Type Type, ResourceStates InitialState);

	ShaderManager m_ShaderManager;

	Device m_Device;
	DescriptorAllocator m_CBVAllocator;
	DescriptorAllocator m_SRVAllocator;
	DescriptorAllocator m_UAVAllocator;
	DescriptorAllocator m_RTVAllocator;
	DescriptorAllocator m_DSVAllocator;
	DescriptorAllocator m_SamplerAllocator;
	ResourceStateTracker m_GlobalResourceTracker;

	CommandQueue m_GraphicsQueue;
	CommandQueue m_ComputeQueue;
	CommandQueue m_CopyQueue;

	using BufferPtr = std::unique_ptr<Buffer, std::function<void(Buffer*)>>;
	using TexturePtr = std::unique_ptr<Texture, std::function<void(Texture*)>>;
	using HeapPtr = std::unique_ptr<Heap>;
	using RootSignaturePtr = std::unique_ptr<RootSignature>;
	using GraphicsPipelineStatePtr = std::unique_ptr<GraphicsPipelineState>;
	using ComputePipelineStatePtr = std::unique_ptr<ComputePipelineState>;

	using BufferTable = std::unordered_map<RenderResourceHandle, BufferPtr>;
	using TextureTable = std::unordered_map<RenderResourceHandle, TexturePtr>;
	using HeapTable = std::unordered_map<RenderResourceHandle, HeapPtr>;
	using RootSignatureTable = std::unordered_map<RenderResourceHandle, RootSignaturePtr>;
	using GraphicsPipelineStateTable = std::unordered_map<RenderResourceHandle, GraphicsPipelineStatePtr>;
	using ComputePipelineStateTable = std::unordered_map<RenderResourceHandle, ComputePipelineStatePtr>;

	template<RenderResourceHandle::Type Type>
	class RenderResourceHandleFactory
	{
	public:
		RenderResourceHandleFactory()
		{
			m_AtomicData = 0;
			m_Handle._Type = Type;
			m_Handle._Flag = RenderResourceHandle::Flags::Active;
			m_Handle._Data = 0;
		}

		RenderResourceHandle GetNewHandle()
		{
			m_Handle._Data = m_AtomicData.fetch_add(1);
			return m_Handle;
		}
	private:
		// TODO: Add thread safe handle query using atomic
		RenderResourceHandle m_Handle;
		std::atomic<std::size_t> m_AtomicData;
	};

	using BufferHandleFactory = RenderResourceHandleFactory<RenderResourceHandle::Type::Buffer>;
	using TextureHandleFactory = RenderResourceHandleFactory<RenderResourceHandle::Type::Texture>;
	using HeapHandleFactory = RenderResourceHandleFactory<RenderResourceHandle::Type::Heap>;
	using RootSignatureHandleFactory = RenderResourceHandleFactory<RenderResourceHandle::Type::RootSignature>;
	using GraphicsPipelineStateHandleFactory = RenderResourceHandleFactory<RenderResourceHandle::Type::GraphicsPSO>;
	using ComputePipelineStateHandleFactory = RenderResourceHandleFactory<RenderResourceHandle::Type::ComputePSO>;

	std::pair<BufferHandleFactory, BufferTable> m_Buffers;
	std::mutex m_BufferAllocMutex;
	std::pair<TextureHandleFactory, TextureTable> m_Textures;
	std::mutex m_TextureAllocMutex;
	std::pair<HeapHandleFactory, HeapTable> m_Heaps;
	std::mutex m_HeapAllocMutex;
	std::pair<RootSignatureHandleFactory, RootSignatureTable> m_RootSignatures;
	std::mutex m_RootSignatureAllocMutex;
	std::pair<GraphicsPipelineStateHandleFactory, GraphicsPipelineStateTable> m_GraphicsPipelineStates;
	std::mutex m_GraphicsPipelineStatesAllocMutex;
	std::pair<ComputePipelineStateHandleFactory, ComputePipelineStateTable> m_ComputePipelineStates;
	std::mutex m_ComputePipelineStatesAllocMutex;
};

#include "RenderDevice.inl"