#pragma once
#include <memory>
#include <vector>
#include <string>
#include <functional>

#include "../RenderResourceHandle.h"
#include "RenderGraphRegistry.h"
#include "../AL/D3D12/CommandList.h"

class RenderDevice;
class CommandList;

enum class RenderPassType
{
	Graphics,
	Compute,
	Copy
};

struct IRenderPass
{
	bool enabled = true;
	unsigned int refCount = 0;
	RenderPassType type;
	std::string name;
	CommandList commandList;
	std::vector<RenderResourceHandle> creates;
	std::vector<RenderResourceHandle> reads;
	std::vector<RenderResourceHandle> writes;

	IRenderPass(const Device* pDevice, D3D12_COMMAND_LIST_TYPE CommandListType)
		: commandList(pDevice, CommandListType)
	{
	}

	virtual void Setup(RenderDevice& RenderDevice) = 0;
	virtual void Execute(RenderGraphRegistry& RenderGraphRegistry) = 0;
};

template<RenderPassType Type, typename Data>
struct RenderPass : public IRenderPass
{
	using SetupCallback = std::function<void(Data&, RenderDevice&)>;
	using ExecuteCallback = std::function<void(const Data&, RenderGraphRegistry&, CommandList&)>;

	Data data;
	SetupCallback setupCallback;
	ExecuteCallback executeCallback;

	RenderPass(const Device* pDevice, D3D12_COMMAND_LIST_TYPE CommandListType)
		: IRenderPass(pDevice, CommandListType)
	{
		this->type = Type;
	}

	void Setup(RenderDevice& RenderDevice)
	{
		setupCallback(data, RenderDevice);
	}
	void Execute(RenderGraphRegistry& RenderGraphRegistry)
	{
		executeCallback(data, RenderGraphRegistry, commandList);
	}
};