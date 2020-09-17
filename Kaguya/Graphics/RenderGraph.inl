template<typename RenderPass>
inline RenderPass* RenderGraph::GetRenderPass()
{
	for (decltype(m_NumRenderPasses) i = 0; i < m_NumRenderPasses; ++i)
	{
		if (m_RenderPassDataIDs[i].get() == typeid(RenderPass))
			return static_cast<RenderPass*>(m_RenderPasses[i].get());
	}
	return nullptr;
}