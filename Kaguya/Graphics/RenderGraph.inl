template<typename RenderPass>
inline RenderPass* RenderGraph::GetRenderPass() const
{
	for (size_t i = 0; i < m_RenderPasses.size(); ++i)
	{
		if (typeid(*m_RenderPasses[i].get()) == typeid(RenderPass))
			return static_cast<RenderPass*>(m_RenderPasses[i].get());
	}
	assert(false && "Render pass not found");
	return nullptr;
}