#pragma region RenderPass
template<typename Data>
inline RenderPass<Data>::RenderPass(RenderPassType Type, PassCallback&& RenderPassPassCallback, ResizeCallback&& RenderPassResizeCallback)
	: RenderPassBase(Type),
	m_Data(),
	m_PassCallback(std::forward<PassCallback>(RenderPassPassCallback)),
	m_ResizeCallback(std::forward<ResizeCallback>(RenderPassResizeCallback))
{
}

template<typename Data>
inline void RenderPass<Data>::Setup(RenderDevice* pRenderDevice)
{
	m_ExecuteCallback = m_PassCallback(m_Data, std::forward<RenderDevice*>(pRenderDevice));
}

template<typename Data>
inline void RenderPass<Data>::Execute(const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
{
	m_ExecuteCallback(m_Data, Scene, RenderGraphRegistry, std::forward<CommandContext*>(pCommandContext));
}

template<typename Data>
inline void RenderPass<Data>::Resize(RenderDevice* pRenderDevice)
{
	m_ResizeCallback(m_Data, std::forward<RenderDevice*>(pRenderDevice));
}
#pragma endregion RenderPass

template<typename Data>
inline RenderPass<Data>* RenderGraph::GetRenderPass()
{
	for (decltype(m_NumRenderPasses) i = 0; i < m_NumRenderPasses; ++i)
	{
		if (m_RenderPassDataIDs[i].get() == typeid(Data))
			return static_cast<RenderPass<Data>*>(m_RenderPasses[i].get());
	}
	return nullptr;
}

template<typename Data, typename PassCallback, typename ResizeCallback>
inline RenderPass<Data>* RenderGraph::AddRenderPass(RenderPassType Type, typename PassCallback&& RenderPassPassCallback, typename ResizeCallback&& RenderPassResizeCallback)
{
	m_RenderPasses.emplace_back(std::make_unique<RenderPass<Data>>(Type, std::forward<PassCallback>(RenderPassPassCallback), std::forward<ResizeCallback>(RenderPassResizeCallback)));
	switch (Type)
	{
		case RenderPassType::Graphics: m_CommandContexts.emplace_back(pRenderDevice->AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT)); break;
		case RenderPassType::Compute: m_CommandContexts.emplace_back(pRenderDevice->AllocateContext(D3D12_COMMAND_LIST_TYPE_COMPUTE)); break;
		case RenderPassType::Copy: m_CommandContexts.emplace_back(pRenderDevice->AllocateContext(D3D12_COMMAND_LIST_TYPE_COPY)); break;
	}
	m_RenderPassDataIDs.emplace_back(typeid(Data));
	m_NumRenderPasses++;
	return static_cast<RenderPass<Data>*>(m_RenderPasses.back().get());
}