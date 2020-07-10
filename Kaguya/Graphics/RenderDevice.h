#pragma once
#include <memory>
#include <mutex>
#include <atomic>
#include <unordered_map>

#include "RenderResourceHandle.h"
#include "ShaderCompiler.h"
#include "RenderCommandContext.h"

#include "AL/D3D12/Device.h"
#include "AL/D3D12/ResourceStateTracker.h"
#include "AL/D3D12/CommandQueue.h"
#include "AL/D3D12/Buffer.h"
#include "AL/D3D12/Texture.h"
#include "AL/D3D12/Heap.h"
#include "AL/D3D12/RootSignature.h"
#include "AL/D3D12/PipelineState.h"

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

	RenderDevice(RenderDevice&&) = default;
	RenderDevice& operator=(RenderDevice&&) = default;

	RenderDevice(const RenderDevice&) = delete;
	RenderDevice& operator=(const RenderDevice&) = delete;

	[[nodiscard]] inline Device& GetDevice() { return m_Device; }
	[[nodiscard]] inline ResourceStateTracker& GetGlobalResourceStateTracker() { return m_GlobalResourceTracker; }
	[[nodiscard]] inline CommandQueue* GetGraphicsQueue() { return &m_GraphicsQueue; }
	[[nodiscard]] inline CommandQueue* GetComputeQueue() { return &m_ComputeQueue; }
	[[nodiscard]] inline CommandQueue* GetCopyQueue() { return &m_CopyQueue; }
	[[nodiscard]] RenderCommandContext* AllocateContext(D3D12_COMMAND_LIST_TYPE Type);

	void ExecuteRenderCommandContexts(UINT NumRenderCommandContexts, RenderCommandContext* ppRenderCommandContexts[]);

	// Resource creation
	[[nodiscard]] RenderResourceHandle LoadShader(Shader::Type Type, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);
	[[nodiscard]] RenderResourceHandle CreateBuffer(const Buffer::Properties& Properties);
	[[nodiscard]] RenderResourceHandle CreateBuffer(const Buffer::Properties& Properties, CPUAccessibleHeapType CPUAccessibleHeapType, const void* pData = nullptr);
	[[nodiscard]] RenderResourceHandle CreateBuffer(const Buffer::Properties& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset);
	[[nodiscard]] RenderResourceHandle CreateTexture(const Texture::Properties& Properties);
	[[nodiscard]] RenderResourceHandle CreateTexture(const Texture::Properties& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset);
	[[nodiscard]] RenderResourceHandle CreateHeap(const Heap::Properties& Properties);
	[[nodiscard]] RenderResourceHandle CreateRootSignature(const RootSignature::Properties& Properties);
	[[nodiscard]] RenderResourceHandle CreateGraphicsPipelineState(const GraphicsPipelineState::Properties& Properties);
	[[nodiscard]] RenderResourceHandle CreateComputePipelineState(const ComputePipelineState::Properties& Properties);

	void Destroy(RenderResourceHandle& RenderResourceHandle);

	// Resource view creation
	void CreateSRVForTexture(RenderResourceHandle RenderResourceHandle, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor);
	void CreateUAVForTexture(RenderResourceHandle RenderResourceHandle, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor, std::optional<UINT> MipSlice);

	// Returns nullptr if a resource is not found
	[[nodiscard]] inline Shader* GetShader(RenderResourceHandle RenderResourceHandle) const { return m_Shaders.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline Buffer* GetBuffer(RenderResourceHandle RenderResourceHandle) const { return m_Buffers.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline Texture* GetTexture(RenderResourceHandle RenderResourceHandle) const { return m_Textures.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline Heap* GetHeap(RenderResourceHandle RenderResourceHandle) const { return m_Heaps.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline RootSignature* GetRootSignature(RenderResourceHandle RenderResourceHandle) const { return m_RootSignatures.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline GraphicsPipelineState* GetGraphicsPSO(RenderResourceHandle RenderResourceHandle) const { return m_GraphicsPipelineStates.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline ComputePipelineState* GetComputePSO(RenderResourceHandle RenderResourceHandle) const { return m_ComputePipelineStates.GetResource(RenderResourceHandle); }
private:
	Device m_Device;
	CommandQueue m_GraphicsQueue;
	CommandQueue m_ComputeQueue;
	CommandQueue m_CopyQueue;
	ResourceStateTracker m_GlobalResourceTracker;
	ShaderCompiler m_ShaderCompiler;
	std::vector<std::unique_ptr<RenderCommandContext>> m_RenderCommandContexts[4];

	template <RenderResourceHandle::Types Type, typename T>
	class RenderResourceContainer
	{
	public:
		template<typename... Args>
		RenderResourceHandle CreateResource(Args&&... args)
		{
			T resource = T(std::forward<Args>(args)...);
			std::scoped_lock lk(m_Mutex);
			RenderResourceHandle handle = m_HandleFactory.GetNewHandle();
			m_ResourceTable[handle] = std::move(resource);
			return handle;
		}

		T* GetResource(const RenderResourceHandle& RenderResourceHandle)
		{
			std::scoped_lock lk(m_Mutex);
			auto iter = m_ResourceTable.find(RenderResourceHandle);
			if (iter != m_ResourceTable.end())
				return &iter->second;
			return nullptr;
		}

		void Destroy(const RenderResourceHandle& RenderResourceHandle)
		{
			std::scoped_lock lk(m_Mutex);
			auto iter = m_ResourceTable.find(RenderResourceHandle);
			if (iter != m_ResourceTable.end())
				m_ResourceTable.erase(iter);
		}
	private:
		template<RenderResourceHandle::Types Type>
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

		std::unordered_map<RenderResourceHandle, T> m_ResourceTable;
		RenderResourceHandleFactory<Type> m_HandleFactory;
		std::mutex m_Mutex;
	};

	mutable RenderResourceContainer<RenderResourceHandle::Types::Shader, Shader> m_Shaders;

	mutable RenderResourceContainer<RenderResourceHandle::Types::Buffer, Buffer> m_Buffers;
	mutable RenderResourceContainer<RenderResourceHandle::Types::Texture, Texture> m_Textures;

	mutable RenderResourceContainer<RenderResourceHandle::Types::Heap, Heap> m_Heaps;
	mutable RenderResourceContainer<RenderResourceHandle::Types::RootSignature, RootSignature> m_RootSignatures;
	mutable RenderResourceContainer<RenderResourceHandle::Types::GraphicsPSO, GraphicsPipelineState> m_GraphicsPipelineStates;
	mutable RenderResourceContainer<RenderResourceHandle::Types::ComputePSO, ComputePipelineState> m_ComputePipelineStates;
};