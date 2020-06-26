template<RenderPassType Type, typename Data, typename RenderPassSetupCallback, typename RenderPassExecuteCallback>
inline RenderPass<Type, Data>* RenderGraph::AddRenderPass(const char* pName, RenderPassSetupCallback SetupCallback, RenderPassExecuteCallback ExecuteCallback)
{
	// Enforce execute callback to be less than 1 KB to avoid capturing huge structures by value
#define KB(x)   ((size_t) (x) << 10)
	constexpr size_t EXECUTE_CALLBACK_MEMORY_LIMIT = KB(1);
#undef KB
	static_assert(sizeof(ExecuteCallback) <= EXECUTE_CALLBACK_MEMORY_LIMIT, "ExecuteCallback's memory exceeds EXECUTE_CALLBACK_MEMORY_LIMIT");

	D3D12_COMMAND_LIST_TYPE type;
	switch (Type)
	{
	case RenderPassType::Graphics: type = D3D12_COMMAND_LIST_TYPE_DIRECT; break;
	case RenderPassType::Compute: type = D3D12_COMMAND_LIST_TYPE_COMPUTE; break;
	case RenderPassType::Copy: type = D3D12_COMMAND_LIST_TYPE_COPY; break;
	}
	auto pRenderPass = std::make_unique<RenderPass<Type, Data>>(&m_RenderDevice.GetDevice(), type);
	pRenderPass->setupCallback = std::move(SetupCallback);
	pRenderPass->executeCallback = std::move(ExecuteCallback);
	m_RenderPasses.emplace_back(std::move(pRenderPass));
	return static_cast<RenderPass<Type, Data>*>(m_RenderPasses.back().get());
}