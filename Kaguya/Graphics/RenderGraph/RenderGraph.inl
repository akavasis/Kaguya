template<RenderPassType Type, typename Data>
inline RenderPass<Type, Data>* RenderGraph::AddRenderPass(const char* pName, typename RenderPass<Type, Data>::PassCallback PassCallback)
{
	auto pRenderPass = std::make_unique<RenderPass<Type, Data>>();
	pRenderPass->name = pName;
	if constexpr (Type == RenderPassType::Graphics)
		pRenderPass->renderCommandContext = m_RenderDevice.AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
	else if constexpr (Type == RenderPassType::Compute)
		pRenderPass->renderCommandContext = m_RenderDevice.AllocateContext(D3D12_COMMAND_LIST_TYPE_COMPUTE);
	else if constexpr (Type == RenderPassType::Copy)
		pRenderPass->renderCommandContext = m_RenderDevice.AllocateContext(D3D12_COMMAND_LIST_TYPE_COPY);
	
	pRenderPass->passCallback = std::move(PassCallback);
	m_RenderPasses.emplace_back(std::move(pRenderPass));
	return static_cast<RenderPass<Type, Data>*>(m_RenderPasses.back().get());
}