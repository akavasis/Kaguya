#pragma once
#include <memory>
#include <vector>
#include <string>
#include <functional>

#include "../RenderResourceHandle.h"

class RenderDevice;

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
	std::vector<RenderResourceHandle> creates;
	std::vector<RenderResourceHandle> reads;
	std::vector<RenderResourceHandle> writes;

	virtual void Setup(RenderDevice& RenderDevice) = 0;
	virtual void Execute(RenderDevice& RenderDevice) = 0;
};

template<RenderPassType Type, typename Data>
struct RenderPass : public IRenderPass
{
	using SetupCallback = std::function<void(Data&, RenderDevice&)>;
	using ExecuteCallback = std::function<void(const Data&, RenderDevice&)>;

	Data data;
	SetupCallback setupCallback;
	ExecuteCallback executeCallback;
	std::unique_ptr<CommandContext> commandContext;

	RenderPass()
	{
		this->type = Type;
	}

	void Setup(RenderDevice& RenderDevice)
	{
		setupCallback(data, RenderDevice);
	}
	void Execute(RenderDevice& RenderDevice)
	{
		executeCallback(data, RenderDevice);
	}
};