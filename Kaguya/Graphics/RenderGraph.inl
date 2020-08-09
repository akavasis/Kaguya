#pragma region RenderPass
template<typename Data>
inline RenderPass<Data>::RenderPass(RenderPassType Type, SetupCallback&& RenderPassSetupCallback, ResizeCallback&& RenderPassResizeCallback)
	: RenderPassBase(Type),
	Data(),
	m_SetupCallback(std::forward<SetupCallback>(RenderPassSetupCallback)),
	m_ResizeCallback(std::forward<ResizeCallback>(RenderPassResizeCallback))
{
}

template<typename Data>
inline void RenderPass<Data>::Setup(RenderDevice* pRenderDevice)
{
	m_ExecuteCallback = m_SetupCallback(*this, std::forward<RenderDevice*>(pRenderDevice));
}

template<typename Data>
inline void RenderPass<Data>::Execute(const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
{
	m_ExecuteCallback(*this, Scene, RenderGraphRegistry, std::forward<CommandContext*>(pCommandContext));
}

template<typename Data>
inline void RenderPass<Data>::Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice)
{
	m_ResizeCallback(*this, std::forward<UINT>(Width), std::forward<UINT>(Height), std::forward<RenderDevice*>(pRenderDevice));
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

template<typename Data, typename SetupCallback, typename ResizeCallback>
inline RenderPass<Data>* RenderGraph::AddRenderPass(RenderPassType Type, typename SetupCallback&& RenderPassSetupCallback, typename ResizeCallback&& RenderPassResizeCallback)
{
	m_RenderPasses.emplace_back(std::make_unique<RenderPass<Data>>(Type, std::forward<SetupCallback>(RenderPassSetupCallback), std::forward<ResizeCallback>(RenderPassResizeCallback)));
	switch (Type)
	{
	case RenderPassType::Graphics: m_CommandContexts.emplace_back(pRenderDevice->AllocateContext(CommandContext::Direct)); break;
	case RenderPassType::Compute: m_CommandContexts.emplace_back(pRenderDevice->AllocateContext(CommandContext::Compute)); break;
	case RenderPassType::Copy: m_CommandContexts.emplace_back(pRenderDevice->AllocateContext(CommandContext::Copy)); break;
	}
	m_RenderPassDataIDs.emplace_back(typeid(Data));
	m_NumRenderPasses++;
	return static_cast<RenderPass<Data>*>(m_RenderPasses.back().get());
}