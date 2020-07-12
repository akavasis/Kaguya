#pragma region RenderPass
template<RenderPassType Type, typename Data>
inline RenderPass<Type, Data>::RenderPass(PassCallback&& RenderPassPassCallback)
	: RenderPassBase(Type),
	m_Data(),
	m_PassCallback(std::forward<PassCallback>(RenderPassPassCallback))
{
}

template<RenderPassType Type, typename Data>
inline void RenderPass<Type, Data>::Setup(RenderDevice& RefRenderDevice)
{
	m_ExecuteCallback = m_PassCallback(m_Data, RefRenderDevice);
}

template<RenderPassType Type, typename Data>
inline void RenderPass<Type, Data>::Execute(Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* pRenderCommandContext)
{
	m_ExecuteCallback(m_Data, Scene, RenderGraphRegistry, std::forward<RenderCommandContext*>(pRenderCommandContext));
}
#pragma endregion RenderPass

template<RenderPassType Type, typename Data>
inline RenderPass<Type, Data>* RenderGraph::GetRenderPass()
{
	for (decltype(m_NumRenderPasses) i = 0; i < m_NumRenderPasses; ++i)
	{
		if (m_RenderPasses[i]->eRenderPassType == Type &&
			m_RenderPassDataIDs[i].get() == typeid(Data))
			return static_cast<RenderPass<Type, Data>*>(m_RenderPasses[i].get());
	}
	return nullptr;
}

template<RenderPassType Type, typename Data, typename PassCallback>
inline RenderPass<Type, Data>* RenderGraph::AddRenderPass(typename PassCallback&& RenderPassPassCallback)
{
	m_RenderPasses.emplace_back(std::make_unique<RenderPass<Type, Data>>(std::forward<PassCallback>(RenderPassPassCallback)));
	if constexpr (Type == RenderPassType::Graphics)
	{
		m_RenderContexts.emplace_back(m_RefRenderDevice.AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT));
	}
	else if constexpr (Type == RenderPassType::Compute)
	{
		m_RenderContexts.emplace_back(m_RefRenderDevice.AllocateContext(D3D12_COMMAND_LIST_TYPE_COMPUTE));
	}
	else if constexpr (Type == RenderPassType::Copy)
	{
		m_RenderContexts.emplace_back(m_RefRenderDevice.AllocateContext(D3D12_COMMAND_LIST_TYPE_COPY));
	}
	m_RenderPassDataIDs.emplace_back(typeid(Data));
	m_NumRenderPasses++;
	return static_cast<RenderPass<Type, Data>*>(m_RenderPasses.back().get());
}