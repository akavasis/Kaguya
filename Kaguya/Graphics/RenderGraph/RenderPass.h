#pragma once
#include <memory>
#include <vector>
#include <string>
#include <functional>

#include "../RenderResourceHandle.h"
#include "RenderGraphRegistry.h"

class RenderDevice;
class RenderCommandContext;
class GraphicsCommandContext;
class ComputeCommandContext;
class CopyCommandContext;

enum class RenderPassType
{
	Graphics,
	Compute,
	Copy
};

template<RenderPassType Type> struct ActualRenderContext {};
template<> struct ActualRenderContext<RenderPassType::Graphics> { using Type = GraphicsCommandContext; };
template<> struct ActualRenderContext<RenderPassType::Compute> { using Type = ComputeCommandContext; };
template<> struct ActualRenderContext<RenderPassType::Copy> { using Type = CopyCommandContext; };

struct IRenderPass
{
	bool enabled = true;
	unsigned int refCount = 0;
	RenderPassType type;
	std::string name;
	RenderCommandContext* renderCommandContext = nullptr;

	virtual void Setup(RenderDevice&) = 0;
	virtual void Execute(RenderGraphRegistry&) = 0;
};

template<RenderPassType Type, typename Data>
struct RenderPass : public IRenderPass
{
	using ExecuteCallback = std::function<void(const Data&, RenderGraphRegistry&, typename ActualRenderContext<Type>::Type*)>;
	using PassCallback = std::function<ExecuteCallback(Data&, RenderDevice&)>;

	Data data;
	PassCallback passCallback;
	ExecuteCallback executeCallback;

	RenderPass()
	{
		this->type = Type;
	}

	void Setup(RenderDevice& RenderDevice) override
	{
		executeCallback = std::move(passCallback(data, RenderDevice));
	}
	void Execute(RenderGraphRegistry& RenderGraphRegistry) override
	{
		executeCallback(data, RenderGraphRegistry, static_cast<ActualRenderContext<Type>::Type*>(renderCommandContext));
	}
};