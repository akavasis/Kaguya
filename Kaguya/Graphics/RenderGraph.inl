#pragma region RenderPass
template<RenderPassType Type, typename Data>
inline RenderPass<Type, Data>::RenderPass(PassCallback PassCallback)
	: RenderPassBase(Type),
	m_PassCallback(std::move(PassCallback))
{
}

template<RenderPassType Type, typename Data>
inline RenderPass<Type, Data>::~RenderPass()
{
}

template<RenderPassType Type, typename Data>
inline void RenderPass<Type, Data>::Setup(RenderDevice& RefRenderDevice)
{
	m_ExecuteCallback = m_PassCallback(m_Data, RefRenderDevice);
}

template<RenderPassType Type, typename Data>
inline void RenderPass<Type, Data>::Execute(RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* RenderCommandContext)
{
	m_ExecuteCallback(m_Data, RenderGraphRegistry, RenderCommandContext);
}
#pragma endregion RenderPass

template<RenderPassType Type, typename Data>
inline RenderPass<Type, Data>* RenderGraph::GetRenderPass()
{
	for (decltype(m_NumRenderPasses) i = 0; i < m_NumRenderPasses; ++i)
	{
		if (m_RenderPasses[i]->eRenderPassType == Type &&
			m_RenderPassDataIDs[i].get() == typeid(Data))
			return m_RenderPasses[i].get();
	}
	return nullptr;
}

template<RenderPassType Type, typename Data, typename PassCallback>
inline RenderPass<Type, Data>* RenderGraph::AddRenderPass(const char* pName, typename PassCallback RenderPassPassCallback)
{
	m_RenderPassNames.emplace_back(pName);
	m_RenderPasses.emplace_back(std::make_unique<RenderPass<Type, Data>>(std::move(RenderPassPassCallback)));
	switch (Type)
	{
	case RenderPassType::Graphics: m_RenderContexts.emplace_back(m_RefRenderDevice.AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT)); break;
	case RenderPassType::Compute: m_RenderContexts.emplace_back(m_RefRenderDevice.AllocateContext(D3D12_COMMAND_LIST_TYPE_COMPUTE)); break;
	case RenderPassType::Copy: m_RenderContexts.emplace_back(m_RefRenderDevice.AllocateContext(D3D12_COMMAND_LIST_TYPE_COPY)); break;
	}
	m_RenderPassDataIDs.emplace_back(typeid(Data));
	m_NumRenderPasses++;
	return static_cast<RenderPass<Type, Data>*>(m_RenderPasses.back().get());
}