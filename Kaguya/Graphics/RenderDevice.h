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

class RenderDevice
{
public:
	RenderDevice(IUnknown* pAdapter);
	~RenderDevice();

	[[nodiscard]] inline Device& GetDevice() { return m_Device; }

	[[nodiscard]] inline ResourceStateTracker& GetGlobalResourceStateTracker() { return m_GlobalResourceTracker; }

	[[nodiscard]] inline CommandQueue* GetGraphicsQueue() { return &m_GraphicsQueue; }
	[[nodiscard]] inline CommandQueue* GetComputeQueue() { return &m_ComputeQueue; }
	[[nodiscard]] inline CommandQueue* GetCopyQueue() { return &m_CopyQueue; }

	[[nodiscard]] RenderCommandContext* AllocateContext(D3D12_COMMAND_LIST_TYPE Type);
	void ExecuteRenderCommandContexts(UINT NumRenderCommandContexts, RenderCommandContext* pRenderCommandContexts[]);

	// Resource creation
	[[nodiscard]] RenderResourceHandle LoadShader(Shader::Type Type, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);

	template<typename T, bool UseConstantBufferAlignment>
	[[nodiscard]] RenderResourceHandle CreateBuffer(const Buffer::Properties<T, UseConstantBufferAlignment>& Properties);
	template<typename T, bool UseConstantBufferAlignment>
	[[nodiscard]] RenderResourceHandle CreateBuffer(const Buffer::Properties<T, UseConstantBufferAlignment>& Properties, CPUAccessibleHeapType CPUAccessibleHeapType, const void* pData);
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
	[[nodiscard]] Shader* GetShader(const RenderResourceHandle& RenderResourceHandle) const;

	[[nodiscard]] Buffer* GetBuffer(const RenderResourceHandle& RenderResourceHandle) const;
	[[nodiscard]] Texture* GetTexture(const RenderResourceHandle& RenderResourceHandle) const;
	[[nodiscard]] Heap* GetHeap(const RenderResourceHandle& RenderResourceHandle) const;
	[[nodiscard]] RootSignature* GetRootSignature(const RenderResourceHandle& RenderResourceHandle) const;
	[[nodiscard]] GraphicsPipelineState* GetGraphicsPSO(const RenderResourceHandle& RenderResourceHandle) const;
	[[nodiscard]] ComputePipelineState* GetComputePSO(const RenderResourceHandle& RenderResourceHandle) const;
private:
	Device m_Device;
	ResourceStateTracker m_GlobalResourceTracker;

	CommandQueue m_GraphicsQueue;
	CommandQueue m_ComputeQueue;
	CommandQueue m_CopyQueue;

	std::vector<std::unique_ptr<RenderCommandContext>> m_RenderCommandContexts[4];

	ShaderCompiler m_ShaderCompiler;

	template <RenderResourceHandle::Type Type, typename T>
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

		std::unordered_map<RenderResourceHandle, T> m_ResourceTable;
		RenderResourceHandleFactory<Type> m_HandleFactory;
		std::mutex m_Mutex;
	};

	mutable RenderResourceContainer<RenderResourceHandle::Type::Shader, Shader> m_Shaders;

	mutable RenderResourceContainer<RenderResourceHandle::Type::Buffer, Buffer> m_Buffers;
	mutable RenderResourceContainer<RenderResourceHandle::Type::Texture, Texture> m_Textures;

	mutable RenderResourceContainer<RenderResourceHandle::Type::Heap, Heap> m_Heaps;
	mutable RenderResourceContainer<RenderResourceHandle::Type::RootSignature, RootSignature> m_RootSignatures;
	mutable RenderResourceContainer<RenderResourceHandle::Type::GraphicsPSO, GraphicsPipelineState> m_GraphicsPipelineStates;
	mutable RenderResourceContainer<RenderResourceHandle::Type::ComputePSO, ComputePipelineState> m_ComputePipelineStates;
};

#include "RenderDevice.inl"